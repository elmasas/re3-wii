/**
 * OpenAL cross platform audio library
 * Wii AESND backend
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <ogc/audio.h>
#include <ogc/cache.h>

#include "alMain.h"
#include "alu.h"
#include "threads.h"
#include "compat.h"

#include "backends/base.h"


/* One AI transfer, 10ms of 48KHz stereo. */
#define DMA_BYTES  1920
/* A transfer is too small to mix in one call, so a thread fills a ring ahead. */
#define MIX_FRAMES 1024
#define RING_BYTES (64*1024)

static const ALCchar defaultDeviceName[] = "AESND Default";

typedef struct ALCaesndBackend {
    DERIVE_FROM_TYPE(ALCbackend);

    ALsizei frameSize;
    ALuint Frequency;
    enum DevFmtChannels FmtChans;
    enum DevFmtType     FmtType;
    ALuint UpdateSize;

    ALbyte *ring;
    ALbyte *mixBuf;
    ATOMIC(ALuint) readPos;
    ATOMIC(ALuint) writePos;

    althrd_t thread;
    ATOMIC(ALenum) killNow;
    ALCboolean registered;
} ALCaesndBackend;

static void ALCaesndBackend_Construct(ALCaesndBackend *self, ALCdevice *device);
static void ALCaesndBackend_Destruct(ALCaesndBackend *self);
static ALCenum ALCaesndBackend_open(ALCaesndBackend *self, const ALCchar *name);
static ALCboolean ALCaesndBackend_reset(ALCaesndBackend *self);
static ALCboolean ALCaesndBackend_start(ALCaesndBackend *self);
static void ALCaesndBackend_stop(ALCaesndBackend *self);
static DECLARE_FORWARD2(ALCaesndBackend, ALCbackend, ALCenum, captureSamples, void*, ALCuint)
static DECLARE_FORWARD(ALCaesndBackend, ALCbackend, ALCuint, availableSamples)
static DECLARE_FORWARD(ALCaesndBackend, ALCbackend, ClockLatency, getClockLatency)
static DECLARE_FORWARD(ALCaesndBackend, ALCbackend, void, lock)
static DECLARE_FORWARD(ALCaesndBackend, ALCbackend, void, unlock)
DECLARE_DEFAULT_ALLOCATORS(ALCaesndBackend)

DEFINE_ALCBACKEND_VTABLE(ALCaesndBackend);

/* The AI callback takes no argument. */
static ALCaesndBackend *ActiveBackend;
static alignas(32) ALbyte dmaBuf[2][DMA_BYTES];
static ALuint dmaCurrent;


static int ALCaesndBackend_mixerProc(void *ptr)
{
    ALCaesndBackend *self = (ALCaesndBackend*)ptr;
    ALCdevice *device = STATIC_CAST(ALCbackend, self)->mDevice;
    ALuint blockSize = MIX_FRAMES * self->frameSize;

    SetRTPriority();
    althrd_setname(althrd_current(), MIXER_THREAD_NAME);

    while(!ATOMIC_LOAD(&self->killNow, almemory_order_acquire))
    {
        ALuint writePos = ATOMIC_LOAD(&self->writePos, almemory_order_relaxed);
        ALuint readPos = ATOMIC_LOAD(&self->readPos, almemory_order_acquire);
        ALuint pos, first;

        if(RING_BYTES - (writePos - readPos) < blockSize)
        {
            al_nssleep(1000000);
            continue;
        }

        ALCaesndBackend_lock(self);
        aluMixData(device, self->mixBuf, MIX_FRAMES);
        ALCaesndBackend_unlock(self);

        pos = writePos & (RING_BYTES-1);
        first = RING_BYTES - pos;
        if(first > blockSize) first = blockSize;
        memcpy(self->ring + pos, self->mixBuf, first);
        if(first < blockSize)
            memcpy(self->ring, self->mixBuf + first, blockSize - first);

        ATOMIC_STORE(&self->writePos, writePos + blockSize, almemory_order_release);
    }

    return 0;
}

static void ALCaesndBackend_fillTransfer(ALCaesndBackend *self, ALbyte *dst)
{
    ALuint writePos = ATOMIC_LOAD(&self->writePos, almemory_order_acquire);
    ALuint readPos = ATOMIC_LOAD(&self->readPos, almemory_order_relaxed);

    if(writePos - readPos < DMA_BYTES)
        memset(dst, 0, DMA_BYTES);
    else
    {
        ALuint pos = readPos & (RING_BYTES-1);
        ALuint first = RING_BYTES - pos;
        if(first > DMA_BYTES) first = DMA_BYTES;
        memcpy(dst, self->ring + pos, first);
        if(first < DMA_BYTES)
            memcpy(dst + first, self->ring, DMA_BYTES - first);

        ATOMIC_STORE(&self->readPos, readPos + DMA_BYTES, almemory_order_release);
    }

    /* Nothing in libogc flushes this, and the AI reads main memory. */

    DCFlushRange(dst, DMA_BYTES);
}

static void ALCaesndBackend_transferDone(void)
{
    ALCaesndBackend *self = ActiveBackend;
    if(!self) return;

    dmaCurrent ^= 1;
    ALCaesndBackend_fillTransfer(self, dmaBuf[dmaCurrent]);
    AUDIO_InitDMA((ALuint)dmaBuf[dmaCurrent], DMA_BYTES);
}


static void ALCaesndBackend_Construct(ALCaesndBackend *self, ALCdevice *device)
{
    ALCbackend_Construct(STATIC_CAST(ALCbackend, self), device);
    SET_VTABLE2(ALCaesndBackend, ALCbackend, self);

    self->frameSize = FrameSizeFromDevFmt(device->FmtChans, device->FmtType, device->AmbiOrder);
    self->Frequency = device->Frequency;
    self->FmtChans = device->FmtChans;
    self->FmtType = device->FmtType;
    self->UpdateSize = device->UpdateSize;
    self->ring = NULL;
    self->mixBuf = NULL;
    self->registered = ALC_FALSE;
    ATOMIC_INIT(&self->readPos, 0u);
    ATOMIC_INIT(&self->writePos, 0u);
    ATOMIC_INIT(&self->killNow, AL_TRUE);
}

static void ALCaesndBackend_Destruct(ALCaesndBackend *self)
{
    if(self->registered)
    {
        AUDIO_RegisterDMACallback(NULL);
        self->registered = ALC_FALSE;
    }
    if(ActiveBackend == self)
        ActiveBackend = NULL;

    al_free(self->ring);
    al_free(self->mixBuf);
    self->ring = NULL;
    self->mixBuf = NULL;

    ALCbackend_Destruct(STATIC_CAST(ALCbackend, self));
}

static ALCenum ALCaesndBackend_open(ALCaesndBackend *self, const ALCchar *name)
{
    ALCdevice *device = STATIC_CAST(ALCbackend, self)->mDevice;

    if(ActiveBackend != NULL)
        return ALC_INVALID_VALUE;

    /* The AI streams 16 bit stereo at a fixed rate, nothing to negotiate. */
    device->Frequency = 48000;
    device->FmtChans = DevFmtStereo;
    device->FmtType = DevFmtShort;
    device->UpdateSize = MIX_FRAMES;
    device->NumUpdates = RING_BYTES / (MIX_FRAMES * FrameSizeFromDevFmt(DevFmtStereo, DevFmtShort, 0));

    self->frameSize = FrameSizeFromDevFmt(device->FmtChans, device->FmtType, device->AmbiOrder);
    self->Frequency = device->Frequency;
    self->FmtChans = device->FmtChans;
    self->FmtType = device->FmtType;
    self->UpdateSize = device->UpdateSize;

    self->ring = al_calloc(16, RING_BYTES);
    self->mixBuf = al_calloc(16, MIX_FRAMES * self->frameSize);
    if(!self->ring || !self->mixBuf)
    {
        al_free(self->ring);
        al_free(self->mixBuf);
        self->ring = NULL;
        self->mixBuf = NULL;
        return ALC_OUT_OF_MEMORY;
    }

    alstr_copy_cstr(&device->DeviceName, name ? name : defaultDeviceName);

    ActiveBackend = self;

    return ALC_NO_ERROR;
}

static ALCboolean ALCaesndBackend_reset(ALCaesndBackend *self)
{
    ALCdevice *device = STATIC_CAST(ALCbackend, self)->mDevice;
    device->Frequency = self->Frequency;
    device->FmtChans = self->FmtChans;
    device->FmtType = self->FmtType;
    device->UpdateSize = self->UpdateSize;
    device->NumUpdates = RING_BYTES / (MIX_FRAMES * self->frameSize);
    SetDefaultWFXChannelOrder(device);
    return ALC_TRUE;
}

static ALCboolean ALCaesndBackend_start(ALCaesndBackend *self)
{
    ATOMIC_STORE(&self->readPos, 0u, almemory_order_relaxed);
    ATOMIC_STORE(&self->writePos, 0u, almemory_order_relaxed);
    ATOMIC_STORE(&self->killNow, AL_FALSE, almemory_order_release);

    if(althrd_create(&self->thread, ALCaesndBackend_mixerProc, self) != althrd_success)
    {
        ATOMIC_STORE(&self->killNow, AL_TRUE, almemory_order_release);
        return ALC_FALSE;
    }

    /* Prime both halves so the first interrupt has something to follow. */
    dmaCurrent = 0;
    memset(dmaBuf, 0, sizeof(dmaBuf));
    DCFlushRange(dmaBuf, sizeof(dmaBuf));

    AUDIO_RegisterDMACallback(ALCaesndBackend_transferDone);
    self->registered = ALC_TRUE;
    AUDIO_InitDMA((ALuint)dmaBuf[0], DMA_BYTES);
    AUDIO_StartDMA();
    return ALC_TRUE;
}

static void ALCaesndBackend_stop(ALCaesndBackend *self)
{
    int res;

    AUDIO_StopDMA();
    AUDIO_RegisterDMACallback(NULL);
    self->registered = ALC_FALSE;

    if(ATOMIC_EXCHANGE(&self->killNow, AL_TRUE, almemory_order_acq_rel) != AL_FALSE)
        return;
    althrd_join(self->thread, &res);
}


typedef struct ALCaesndBackendFactory {
    DERIVE_FROM_TYPE(ALCbackendFactory);
} ALCaesndBackendFactory;
#define ALCaesndBACKENDFACTORY_INITIALIZER { { GET_VTABLE2(ALCaesndBackendFactory, ALCbackendFactory) } }

ALCbackendFactory *ALCaesndBackendFactory_getFactory(void);

static ALCboolean ALCaesndBackendFactory_init(ALCaesndBackendFactory *self);
static void ALCaesndBackendFactory_deinit(ALCaesndBackendFactory *self);
static ALCboolean ALCaesndBackendFactory_querySupport(ALCaesndBackendFactory *self, ALCbackend_Type type);
static void ALCaesndBackendFactory_probe(ALCaesndBackendFactory *self, enum DevProbe type, al_string *outnames);
static ALCbackend* ALCaesndBackendFactory_createBackend(ALCaesndBackendFactory *self, ALCdevice *device, ALCbackend_Type type);
DEFINE_ALCBACKENDFACTORY_VTABLE(ALCaesndBackendFactory);


ALCbackendFactory *ALCaesndBackendFactory_getFactory(void)
{
    static ALCaesndBackendFactory factory = ALCaesndBACKENDFACTORY_INITIALIZER;
    return STATIC_CAST(ALCbackendFactory, &factory);
}


static ALCboolean ALCaesndBackendFactory_init(ALCaesndBackendFactory* UNUSED(self))
{
    AUDIO_Init(NULL);
    AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);
    return ALC_TRUE;
}

static void ALCaesndBackendFactory_deinit(ALCaesndBackendFactory* UNUSED(self))
{
}

static ALCboolean ALCaesndBackendFactory_querySupport(ALCaesndBackendFactory* UNUSED(self), ALCbackend_Type type)
{
    if(type == ALCbackend_Playback)
        return ALC_TRUE;
    return ALC_FALSE;
}

static void ALCaesndBackendFactory_probe(ALCaesndBackendFactory* UNUSED(self), enum DevProbe type, al_string *outnames)
{
    if(type != ALL_DEVICE_PROBE)
        return;

    alstr_append_range(outnames, defaultDeviceName, defaultDeviceName+sizeof(defaultDeviceName));
}

static ALCbackend* ALCaesndBackendFactory_createBackend(ALCaesndBackendFactory* UNUSED(self), ALCdevice *device, ALCbackend_Type type)
{
    if(type == ALCbackend_Playback)
    {
        ALCaesndBackend *backend;
        NEW_OBJ(backend, ALCaesndBackend)(device);
        if(!backend) return NULL;
        return STATIC_CAST(ALCbackend, backend);
    }

    return NULL;
}
