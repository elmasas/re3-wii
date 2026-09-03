#ifdef __WII__

#include "wii.h"

#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/lwp.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/system.h>
#include <ogc/conf.h>
#include <fat.h>
#include <sdcard/wiisd_io.h>
#include <unistd.h>

#include <stdio.h>
#include "rwcore.h"
#include "skeleton.h"
#include "platform.h"
#include "crossplatform.h"

#include "main.h"
#include "FileMgr.h"
#include "Text.h"
#include "Pad.h"
#include "Timer.h"
#include "DMAudio.h"
#include "ControllerConfig.h"
#include "Frontend.h"
#include "Game.h"
#include "PCSave.h"
#include "MemoryCard.h"
#include "Sprite2d.h"
#include "AnimViewer.h"
#include "Font.h"
#include "MemoryMgr.h"

#include <stddef.h>
#include <locale.h>
#include <signal.h>
#include <errno.h>

#define MAX_SUBSYSTEMS		(16)

rw::EngineOpenParams openParams;

static psGlobalType PsGlobal;

static RwBool ForegroundApp = TRUE;

static RwBool		  RwInitialised = FALSE;

static RwSubSystemInfo GsubSysInfo[MAX_SUBSYSTEMS];
static RwInt32		GnumSubSystems = 0;
static RwInt32		GcurSel = 0, GcurSelVM = 0;

static RwBool useDefault;

static RwInt32 bestWndMode = -1;

static GXRModeObj *WiiRMode = nil;

size_t _dwMemAvailPhys;
RwUInt32 gGameState;
long _dwOperatingSystemVersion;

static RwBool WiiShouldRun = TRUE;

static volatile bool sdMountDone = false;
static volatile bool sdMountResult = false;

static void *sdMountThread(void *arg)
{
	sdMountResult = fatMountSimple("sd", &__io_wiisd);
	sdMountDone = true;
	return nil;
}

static bool MountSdWithTimeout(uint32 timeoutMs)
{
	lwp_t thread;
	if (LWP_CreateThread(&thread, sdMountThread, nil, nil, 0x8000, 70) != 0)
		return false;

	u64 start = gettime();
	while (!sdMountDone) {
		if (ticks_to_millisecs(gettime() - start) > timeoutMs) {
			LWP_SuspendThread(thread);
			return false;
		}
		VIDEO_WaitVSync();
	}
	return sdMountResult;
}

extern "C" int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
	return -1;
}

/*
 *****************************************************************************
 */
void _psCreateFolder(const char *path)
{
	struct stat info;
	char fullpath[PATH_MAX];
	realpath(path, fullpath);

	if (stat(fullpath, &info) != 0) {
		if (errno == ENOENT || (errno != EACCES && !S_ISDIR(info.st_mode))) {
			mkdir(fullpath, 0755);
		}
	}
}

/*
 *****************************************************************************
 */
const char *_psGetUserFilesFolder()
{
	static char szUserFiles[256];
	strcpy(szUserFiles, "userfiles");
	_psCreateFolder(szUserFiles);
	return szUserFiles;
}

/*
 *****************************************************************************
 */
RwBool
psCameraBeginUpdate(RwCamera *camera)
{
	if ( !RwCameraBeginUpdate(Scene.camera) )
	{
		ForegroundApp = FALSE;
		RsEventHandler(rsACTIVATE, (void *)FALSE);
		return FALSE;
	}

	return TRUE;
}

/*
 *****************************************************************************
 */
void
psCameraShowRaster(RwCamera *camera)
{
	if (CMenuManager::m_PrefsVsync)
		RwCameraShowRaster(camera, PSGLOBAL(window), rwRASTERFLIPWAITVSYNC);
	else
		RwCameraShowRaster(camera, PSGLOBAL(window), rwRASTERFLIPDONTWAIT);

	return;
}

/*
 *****************************************************************************
 */
RwImage *
psGrabScreen(RwCamera *pCamera)
{
#ifndef LIBRW
	RwRaster *pRaster = RwCameraGetRaster(pCamera);
	if (RwImage *pImage = RwImageCreate(pRaster->width, pRaster->height, 32)) {
		RwImageAllocatePixels(pImage);
		RwImageSetFromRaster(pImage, pRaster);
		return pImage;
	}
#endif
	return nil;
}

/*
 *****************************************************************************
 */
double
psTimer(void)
{
	return ticks_to_millisecs(gettime());
}


/*
 *****************************************************************************
 */
void
psMouseSetPos(RwV2d *pos)
{
	PSGLOBAL(lastMousePos.x) = (RwInt32)pos->x;

	PSGLOBAL(lastMousePos.y) = (RwInt32)pos->y;

	return;
}

/*
 *****************************************************************************
 */
RwMemoryFunctions*
psGetMemoryFunctions(void)
{
#ifdef USE_CUSTOM_ALLOCATOR
	return &memFuncs;
#else
	return nil;
#endif
}

/*
 *****************************************************************************
 */
RwBool
psInstallFileSystem(void)
{
	return (TRUE);
}


/*
 *****************************************************************************
 */
RwBool
psNativeTextureSupport(void)
{
	return true;
}

/*
 *****************************************************************************
 */
RwBool
psInitialize(void)
{
	PsGlobal.lastMousePos.x = PsGlobal.lastMousePos.y = 0.0f;

	RsGlobal.ps = &PsGlobal;

	PsGlobal.fullScreen = FALSE;
	PsGlobal.cursorIsInWindow = TRUE;

	PsGlobal.joy1id	= -1;
	PsGlobal.joy2id	= -1;

	CFileMgr::Initialise();

	C_PcSave::SetSaveDirectory(_psGetUserFilesFolder());

	InitialiseLanguage();

#if GTA_VERSION < GTA3_PC_11
	FrontEndMenuManager.LoadSettings();
#endif

	gGameState = GS_START_UP;
	TRACE("gGameState = GS_START_UP");

	_dwOperatingSystemVersion = OS_WINXP;

#if GTA_VERSION >= GTA3_PC_11
	FrontEndMenuManager.LoadSettings();
#endif

	_dwMemAvailPhys = (size_t)SYS_GetArena1Size() + (size_t)SYS_GetArena2Size();
	// streaming doesn't account for the GX texture pool, keep it out of the budget
	if (_dwMemAvailPhys > 32*1024*1024)
		_dwMemAvailPhys -= 32*1024*1024;

	debug("_dwMemAvailPhys: %u MB", _dwMemAvailPhys/1024/1024);

	TheText.Unload();

	return TRUE;
}


/*
 *****************************************************************************
 */
void
psTerminate(void)
{
	return;
}

/*
 *****************************************************************************
 */
static RwChar **_VMList;

RwInt32 _psGetNumVideModes()
{
	return RwEngineGetNumVideoModes();
}

/*
 *****************************************************************************
 */
RwBool _psFreeVideoModeList()
{
	RwInt32 numModes;
	RwInt32 i;

	numModes = _psGetNumVideModes();

	if ( _VMList == nil )
		return TRUE;

	for ( i = 0; i < numModes; i++ )
	{
		RwFree(_VMList[i]);
	}

	RwFree(_VMList);

	_VMList = nil;

	return TRUE;
}

/*
 *****************************************************************************
 */
RwChar **_psGetVideoModeList()
{
	RwInt32 numModes;
	RwInt32 i;

	if ( _VMList != nil )
	{
		return _VMList;
	}

	numModes = RwEngineGetNumVideoModes();

	_VMList = (RwChar **)RwCalloc(numModes, sizeof(RwChar*));

	for ( i = 0; i < numModes; i++	)
	{
		RwVideoMode			vm;

		RwEngineGetVideoModeInfo(&vm, i);

		if ( vm.flags & rwVIDEOMODEEXCLUSIVE )
		{
			_VMList[i] = (RwChar*)RwCalloc(100, sizeof(RwChar));
			rwsprintf(_VMList[i],"%d X %d X %d", vm.width, vm.height, vm.depth);
		}
		else
			_VMList[i] = nil;
	}

	return _VMList;
}

/*
 *****************************************************************************
 */
void _psSelectScreenVM(RwInt32 videoMode)
{
	RwTexDictionarySetCurrent( nil );

	FrontEndMenuManager.UnloadTextures();

	if (!_psSetVideoMode(RwEngineGetCurrentSubSystem(), videoMode))
	{
		RsGlobal.quit = TRUE;

		printf("ERROR: Failed to select new screen resolution\n");
	}
	else
		FrontEndMenuManager.LoadAllTextures();
}

/*
 *****************************************************************************
 */

RwBool IsForegroundApp()
{
	return !!ForegroundApp;
}

/*
 *****************************************************************************
 */
RwBool
psSelectDevice()
{
	RwVideoMode			vm;
	RwInt32				subSysNum;

	RwBool modeFound = FALSE;

	if ( !useDefault )
	{
		GnumSubSystems = RwEngineGetNumSubSystems();
		if ( !GnumSubSystems )
		{
			 return FALSE;
		}

		/* Just to be sure ... */
		GnumSubSystems = (GnumSubSystems > MAX_SUBSYSTEMS) ? MAX_SUBSYSTEMS : GnumSubSystems;

		/* Get the names of all the sub systems */
		for (subSysNum = 0; subSysNum < GnumSubSystems; subSysNum++)
		{
			RwEngineGetSubSystemInfo(&GsubSysInfo[subSysNum], subSysNum);
		}

		/* Get the default selection */
		GcurSel = RwEngineGetCurrentSubSystem();
	}

	/* Set the driver to use the correct sub system */
	if (!RwEngineSetSubSystem(GcurSel))
	{
		return FALSE;
	}

	if ( !useDefault )
	{
		if(FrontEndMenuManager.m_nPrefsWidth == 0 ||
		   FrontEndMenuManager.m_nPrefsHeight == 0 ||
		   FrontEndMenuManager.m_nPrefsDepth == 0){
			// Defaults if nothing specified
			FrontEndMenuManager.m_nPrefsWidth = RsGlobal.maximumWidth;
			FrontEndMenuManager.m_nPrefsHeight = RsGlobal.maximumHeight;
			FrontEndMenuManager.m_nPrefsDepth = 32;
			FrontEndMenuManager.m_nPrefsWindowed = 0;
		}

		// Find the videomode that best fits what we got from the settings file
		RwInt32 bestFsMode = -1;
		RwInt32 bestWidth = -1;
		RwInt32 bestHeight = -1;
		RwInt32 bestDepth = -1;
		for(GcurSelVM = 0; GcurSelVM < RwEngineGetNumVideoModes(); GcurSelVM++){
			RwEngineGetVideoModeInfo(&vm, GcurSelVM);

			if (vm.flags & rwVIDEOMODEEXCLUSIVE)
			{
				// try the largest one that isn't larger than what we wanted
				if(vm.width >= bestWidth && vm.width <= FrontEndMenuManager.m_nPrefsWidth &&
				   vm.height >= bestHeight && vm.height <= FrontEndMenuManager.m_nPrefsHeight &&
				   vm.depth >= bestDepth && vm.depth <= FrontEndMenuManager.m_nPrefsDepth){
					bestWidth = vm.width;
					bestHeight = vm.height;
					bestDepth = vm.depth;
					bestFsMode = GcurSelVM;
				}
			}
			else
			{
				bestWndMode = GcurSelVM;
			}
		}

		if(bestFsMode < 0){
			printf("WARNING: Cannot find desired video mode, selecting device cancelled\n");
			return FALSE;
		}
		GcurSelVM = bestFsMode;

		FrontEndMenuManager.m_nDisplayVideoMode = GcurSelVM;
		FrontEndMenuManager.m_nPrefsVideoMode = FrontEndMenuManager.m_nDisplayVideoMode;

		FrontEndMenuManager.m_nSelectedScreenMode = FrontEndMenuManager.m_nPrefsWindowed;
	}

	RwEngineGetVideoModeInfo(&vm, GcurSelVM);

	FrontEndMenuManager.m_nCurrOption = 0;

	/* Set up the video mode and set the apps window
	* dimensions to match */
	if (!RwEngineSetVideoMode(GcurSelVM))
	{
		return FALSE;
	}

	if (vm.flags & rwVIDEOMODEEXCLUSIVE)
	{
		RsGlobal.maximumWidth = vm.width;
		RsGlobal.maximumHeight = vm.height;
		RsGlobal.width = vm.width;
		RsGlobal.height = vm.height;

		PSGLOBAL(fullScreen) = TRUE;
	}

	return TRUE;
}

void _InputInitialiseJoys()
{
	PAD_Init();
	WPAD_Init();
	WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC);

	ControlsManager.InitDefaultControlConfigJoyPad(16);
}

long _InputInitialiseMouse()
{
	return 0;
}

void psPostRWinit(void)
{
	_InputInitialiseJoys();
	_InputInitialiseMouse();

	// Make sure all keys are released
	CPad::GetPad(0)->Clear(true);
	CPad::GetPad(1)->Clear(true);
}

/*
 *****************************************************************************
 */
RwBool _psSetVideoMode(RwInt32 subSystem, RwInt32 videoMode)
{
	RwInitialised = FALSE;

	RsEventHandler(rsRWTERMINATE, nil);

	GcurSel = subSystem;
	GcurSelVM = videoMode;

	useDefault = TRUE;

	if ( RsEventHandler(rsRWINITIALIZE, &openParams) == rsEVENTERROR )
		return FALSE;

	RwInitialised = TRUE;
	useDefault = FALSE;

	RwRect r;

	r.x = 0;
	r.y = 0;
	r.w = RsGlobal.maximumWidth;
	r.h = RsGlobal.maximumHeight;

	RsEventHandler(rsCAMERASIZE, &r);

	psPostRWinit();

	return TRUE;
}

/*
 *****************************************************************************
 */
void InitialiseLanguage()
{
	s32 lang = CONF_GetLanguage();
	switch (lang)
	{
		case CONF_LANG_GERMAN:
			CGame::nastyGame = false;
			CMenuManager::m_PrefsAllowNastyGame = false;
			CGame::germanGame = true;
			CMenuManager::m_PrefsLanguage = CMenuManager::LANGUAGE_GERMAN;
			break;
		case CONF_LANG_FRENCH:
			CGame::nastyGame = false;
			CMenuManager::m_PrefsAllowNastyGame = false;
			CGame::frenchGame = true;
			CMenuManager::m_PrefsLanguage = CMenuManager::LANGUAGE_FRENCH;
			break;
		case CONF_LANG_SPANISH:
			CMenuManager::m_PrefsLanguage = CMenuManager::LANGUAGE_SPANISH;
			break;
		case CONF_LANG_ITALIAN:
			CMenuManager::m_PrefsLanguage = CMenuManager::LANGUAGE_ITALIAN;
			break;
		default:
			CMenuManager::m_PrefsLanguage = CMenuManager::LANGUAGE_AMERICAN;
			break;
	}

	TheText.Unload();
	TheText.Load();

	// TODO this is needed for strcasecmp to work correctly across all languages, but can these cause other problems??
	setlocale(LC_CTYPE, "C");
	setlocale(LC_COLLATE, "C");
	setlocale(LC_NUMERIC, "C");
}

/*
 *****************************************************************************
 */

void HandleExit()
{
	return;
}

bool lshiftStatus = false;
bool rshiftStatus = false;

// R* calls that in ControllerConfig, idk why
void
_InputTranslateShiftKeyUpDown(RsKeyCodes *rs) {
	RsKeyboardEventHandler(lshiftStatus ? rsKEYDOWN : rsKEYUP, &(*rs = rsLSHIFT));
	RsKeyboardEventHandler(rshiftStatus ? rsKEYDOWN : rsKEYUP, &(*rs = rsRSHIFT));
}

RwV2d leftStickPos;

static uint32 remapWpadButtons(uint32 wpad, int32 expansion)
{
	uint32 out = 0;

	if (wpad & WPAD_BUTTON_A)     out |= PAD_BUTTON_A;
	if (wpad & WPAD_BUTTON_B)     out |= PAD_BUTTON_B;
	if (wpad & WPAD_BUTTON_1)     out |= PAD_BUTTON_X;
	if (wpad & WPAD_BUTTON_2)     out |= PAD_BUTTON_Y;
	if (wpad & WPAD_BUTTON_PLUS)  out |= PAD_BUTTON_START;
	if (wpad & WPAD_BUTTON_MINUS) out |= PAD_TRIGGER_Z;
	if (wpad & WPAD_BUTTON_UP)    out |= PAD_BUTTON_UP;
	if (wpad & WPAD_BUTTON_DOWN)  out |= PAD_BUTTON_DOWN;
	if (wpad & WPAD_BUTTON_LEFT)  out |= PAD_BUTTON_LEFT;
	if (wpad & WPAD_BUTTON_RIGHT) out |= PAD_BUTTON_RIGHT;

	if (expansion == WPAD_EXP_NUNCHUK)
	{
		if (wpad & WPAD_NUNCHUK_BUTTON_Z) out |= PAD_TRIGGER_R;
		if (wpad & WPAD_NUNCHUK_BUTTON_C) out |= PAD_TRIGGER_L;
	}
	else if (expansion == WPAD_EXP_CLASSIC)
	{
		if (wpad & WPAD_CLASSIC_BUTTON_A)      out |= PAD_BUTTON_A;
		if (wpad & WPAD_CLASSIC_BUTTON_B)      out |= PAD_BUTTON_B;
		if (wpad & WPAD_CLASSIC_BUTTON_X)      out |= PAD_BUTTON_X;
		if (wpad & WPAD_CLASSIC_BUTTON_Y)      out |= PAD_BUTTON_Y;
		if (wpad & WPAD_CLASSIC_BUTTON_PLUS)   out |= PAD_BUTTON_START;
		if (wpad & WPAD_CLASSIC_BUTTON_FULL_L) out |= PAD_TRIGGER_L;
		if (wpad & WPAD_CLASSIC_BUTTON_FULL_R) out |= PAD_TRIGGER_R;
		if (wpad & (WPAD_CLASSIC_BUTTON_ZL|WPAD_CLASSIC_BUTTON_ZR)) out |= PAD_TRIGGER_Z;
		if (wpad & WPAD_CLASSIC_BUTTON_UP)     out |= PAD_BUTTON_UP;
		if (wpad & WPAD_CLASSIC_BUTTON_DOWN)   out |= PAD_BUTTON_DOWN;
		if (wpad & WPAD_CLASSIC_BUTTON_LEFT)   out |= PAD_BUTTON_LEFT;
		if (wpad & WPAD_CLASSIC_BUTTON_RIGHT)  out |= PAD_BUTTON_RIGHT;
	}

	return out;
}

static void addJoystick(joystick_t *js, RwV2d *out)
{
	float rad = js->ang * (M_PI / 180.0f);
	out->x += js->mag * sinf(rad);
	out->y += -js->mag * cosf(rad);
}

void CapturePad(RwInt32 padID)
{
	if (padID != 0)
		return;

	uint32 connected = PAD_ScanPads();
	WPAD_ScanPads();

	uint32 buttonsTriggered = 0;
	uint32 buttonsHeld = 0;

	leftStickPos.x = 0.0f;
	leftStickPos.y = 0.0f;

	// reading both at once would double up every press
	if (connected & PAD_CHAN0_BIT)
	{
		buttonsTriggered = PAD_ButtonsDown(0);
		buttonsHeld = PAD_ButtonsHeld(0);

		leftStickPos.x = PAD_StickX(0) / 128.0f;
		leftStickPos.y = -PAD_StickY(0) / 128.0f;
	}
	else
	{
		uint32 wpadType = WPAD_EXP_NONE;
		if (WPAD_Probe(WPAD_CHAN_0, &wpadType) != WPAD_ERR_NONE)
			wpadType = WPAD_EXP_NONE;

		buttonsTriggered = remapWpadButtons(WPAD_ButtonsDown(WPAD_CHAN_0), wpadType);
		buttonsHeld      = remapWpadButtons(WPAD_ButtonsHeld(WPAD_CHAN_0), wpadType);

		if (wpadType == WPAD_EXP_NUNCHUK || wpadType == WPAD_EXP_CLASSIC)
		{
			expansion_t exp;
			WPAD_Expansion(WPAD_CHAN_0, &exp);
			if (exp.type == WPAD_EXP_NUNCHUK)
				addJoystick(&exp.nunchuk.js, &leftStickPos);
			else if (exp.type == WPAD_EXP_CLASSIC)
				addJoystick(&exp.classic.ljs, &leftStickPos);
		}
	}

	leftStickPos.x = leftStickPos.x >  1.0f ?  1.0f : leftStickPos.x;
	leftStickPos.x = leftStickPos.x < -1.0f ? -1.0f : leftStickPos.x;
	leftStickPos.y = leftStickPos.y >  1.0f ?  1.0f : leftStickPos.y;
	leftStickPos.y = leftStickPos.y < -1.0f ? -1.0f : leftStickPos.y;

	if (ControlsManager.m_bFirstCapture == false)
	{
		memcpy(&ControlsManager.m_OldState, &ControlsManager.m_NewState, sizeof(ControlsManager.m_NewState));
	}

	ControlsManager.m_NewState.buttonsTriggered = buttonsTriggered;
	ControlsManager.m_NewState.buttonsHeld = buttonsHeld;

	if (ControlsManager.m_bFirstCapture == true)
	{
		memcpy(&ControlsManager.m_OldState, &ControlsManager.m_NewState, sizeof(ControlsManager.m_NewState));

		ControlsManager.m_bFirstCapture = false;
	}

	RsPadButtonStatus bs;
	bs.padID = padID;

	RsPadEventHandler(rsPADBUTTONUP, (void *)&bs);

	{
		if (CPad::m_bMapPadOneToPadTwo)
			bs.padID = 1;

		RsPadEventHandler(rsPADBUTTONUP,   (void *)&bs);
		RsPadEventHandler(rsPADBUTTONDOWN, (void *)&bs);
	}

	{
		if (CPad::m_bMapPadOneToPadTwo)
			bs.padID = 1;

		CPad *pad = CPad::GetPad(bs.padID);

		if ( Abs(leftStickPos.x) > 0.3f )
			pad->PCTempJoyState.LeftStickX = (int32)(leftStickPos.x * 128.0f);

		if ( Abs(leftStickPos.y) > 0.3f )
			pad->PCTempJoyState.LeftStickY = (int32)(leftStickPos.y * 128.0f);
	}
}

/*
 *****************************************************************************
 */
int
main(int argc, char *argv[])
{
	RwV2d pos;

#ifdef USE_CUSTOM_ALLOCATOR
	InitMemoryMgr();
#endif

	VIDEO_Init();
	WiiRMode = VIDEO_GetPreferredMode(NULL);

	void *consoleXfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(WiiRMode));
	console_init(consoleXfb, 20, 20, WiiRMode->fbWidth, WiiRMode->xfbHeight, WiiRMode->fbWidth * VI_DISPLAY_PIX_SZ);
	VIDEO_Configure(WiiRMode);
	VIDEO_SetNextFramebuffer(consoleXfb);
	VIDEO_SetBlack(false);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (WiiRMode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();

	RsGlobal.maximumWidth = RsGlobal.width = WiiRMode->fbWidth;
	RsGlobal.maximumHeight = RsGlobal.height = WiiRMode->xfbHeight;

	bool fatOk = MountSdWithTimeout(5000);
	(void)fatOk;

	// Set working dir to the path where the assets are
	chdir("sd:/apps/re3");

	/*
	 * Initialize the platform independent data.
	 * This will in turn initialize the platform specific data...
	 */
	if( RsEventHandler(rsINITIALIZE, nil) == rsEVENTERROR )
	{
		printf("Initialize failed\n");
		return FALSE;
	}

	/*
	 * Parameters to be used in RwEngineOpen / rsRWINITIALISE event
	 */

	openParams.width = RsGlobal.maximumWidth;
	openParams.height = RsGlobal.maximumHeight;
	openParams.windowtitle = RsGlobal.appName;
	openParams.window = nil;

	ControlsManager.MakeControllerActionsBlank();
	ControlsManager.InitDefaultControlConfiguration();

	/*
	 * Initialize the 3D (RenderWare) components of the app...
	 */
	if( rsEVENTERROR == RsEventHandler(rsRWINITIALIZE, &openParams) )
	{
		RsEventHandler(rsTERMINATE, nil);
		printf("Cannot initialize RenderWare\n");
		return 0;
	}

	psPostRWinit();


	ControlsManager.InitDefaultControlConfigMouse(MousePointerStateHelper.GetMouseSetUp());



	/*
	 * Force a camera resize event...
	 */
	{
		RwRect r;

		r.x = 0;
		r.y = 0;
		r.w = RsGlobal.maximumWidth;
		r.h = RsGlobal.maximumHeight;

		RsEventHandler(rsCAMERASIZE, &r);
	}


	{
		CFileMgr::SetDirMyDocuments();

		int32 gta3set = CFileMgr::OpenFile("gta3.set", "r");

		if ( gta3set )
		{
			ControlsManager.LoadSettings(gta3set);
			CFileMgr::CloseFile(gta3set);
		}

		CFileMgr::SetDir("");
	}


	// main loop
	while ( TRUE )
	{
		RwInitialised = TRUE;

		/*
		* Set the initial mouse position...
		*/
		pos.x = RsGlobal.maximumWidth * 0.5f;
		pos.y = RsGlobal.maximumHeight * 0.5f;

		RsMouseSetPos(&pos);

		/*
		* Enter the message processing loop...
		*/

#ifndef MASTER
		if (gbModelViewer) {
			LoadingScreen("Loading the ModelViewer", NULL, GetRandomSplashScreen());
			CAnimViewer::Initialise();
			CTimer::Update();
			FrontEndMenuManager.m_bGameNotLoaded = false;
		}
#endif

		while( !RsGlobal.quit && !FrontEndMenuManager.m_bWantToRestart && WiiShouldRun )
		{
#ifndef MASTER
			if (gbModelViewer) {
				TheModelViewer();
			} else
#endif
			if ( ForegroundApp )
			{
				switch ( gGameState )
				{
					case GS_START_UP:
					{
#ifdef NO_MOVIES
						gGameState = GS_INIT_ONCE;
#else
						gGameState = GS_INIT_LOGO_MPEG;
#endif
						TRACE("gGameState = GS_INIT_ONCE");
						break;
					}

					case GS_INIT_LOGO_MPEG:
					{
						gGameState = GS_LOGO_MPEG;
						TRACE("gGameState = GS_LOGO_MPEG;");
						break;
					}

					case GS_LOGO_MPEG:
					{
						++gGameState;
						break;
					}

					case GS_INIT_INTRO_MPEG:
					{
						gGameState = GS_INTRO_MPEG;
						TRACE("gGameState = GS_INTRO_MPEG;");
						break;
					}

					case GS_INTRO_MPEG:
					{
						++gGameState;
						break;
					}

					case GS_INIT_ONCE:
					{
						LoadingScreen(nil, nil, "loadsc0");
						if ( !CGame::InitialiseOnceAfterRW() ) {
							RsGlobal.quit = TRUE;
						}

						gGameState = GS_INIT_FRONTEND;
						TRACE("gGameState = GS_INIT_FRONTEND;");
						break;
					}

					case GS_INIT_FRONTEND:
					{
						LoadingScreen(nil, nil, "loadsc0");

						FrontEndMenuManager.m_bGameNotLoaded = true;

						CMenuManager::m_bStartUpFrontEndRequested = true;


						gGameState = GS_FRONTEND;
						TRACE("gGameState = GS_FRONTEND;");
						break;
					}

					case GS_FRONTEND:
					{
						RsEventHandler(rsFRONTENDIDLE, nil);

						if ( !FrontEndMenuManager.m_bMenuActive || FrontEndMenuManager.m_bWantToLoad )
						{
							gGameState = GS_INIT_PLAYING_GAME;
							TRACE("gGameState = GS_INIT_PLAYING_GAME;");
						}

						if ( FrontEndMenuManager.m_bWantToLoad )
						{
							InitialiseGame();
							FrontEndMenuManager.m_bGameNotLoaded = false;
							gGameState = GS_PLAYING_GAME;
							TRACE("gGameState = GS_PLAYING_GAME;");
						}
						break;
					}

					case GS_INIT_PLAYING_GAME:
					{
						InitialiseGame();

						FrontEndMenuManager.m_bGameNotLoaded = false;
						gGameState = GS_PLAYING_GAME;
						TRACE("gGameState = GS_PLAYING_GAME;");
						break;
					}

					case GS_PLAYING_GAME:
					{
						float ms = (float)CTimer::GetCurrentTimeInCycles() / (float)CTimer::GetCyclesPerMillisecond();
						if ( RwInitialised )
						{
							if (!CMenuManager::m_PrefsFrameLimiter || (1000.0f / (float)RsGlobal.maxFPS) < ms)
								RsEventHandler(rsIDLE, (void *)TRUE);
						}
						break;
					}
				}
			}
			else
			{
				if ( RwCameraBeginUpdate(Scene.camera) )
				{
					RwCameraEndUpdate(Scene.camera);
					ForegroundApp = TRUE;
					RsEventHandler(rsACTIVATE, (void *)TRUE);
				}

			}
		}

		/*
		* About to shut down - block resize events again...
		*/
		RwInitialised = FALSE;

		FrontEndMenuManager.UnloadTextures();
		if ( !FrontEndMenuManager.m_bWantToRestart )
			break;

		CPad::ResetCheats();
		CPad::StopPadsShaking();

		DMAudio.ChangeMusicMode(MUSICMODE_DISABLE);

		CTimer::Stop();

		if ( FrontEndMenuManager.m_bWantToLoad )
		{
			CGame::ShutDownForRestart();
			CGame::InitialiseWhenRestarting();
			DMAudio.ChangeMusicMode(MUSICMODE_GAME);
			LoadSplash(GetLevelSplashScreen(CGame::currLevel));
			FrontEndMenuManager.m_bWantToLoad = false;
		}
		else
		{
#ifndef MASTER
			if ( gbModelViewer )
				CAnimViewer::Shutdown();
			else
#endif
			if ( gGameState == GS_PLAYING_GAME )
				CGame::ShutDown();

			CTimer::Stop();

			if ( FrontEndMenuManager.m_bFirstTime == true )
			{
				gGameState = GS_INIT_FRONTEND;
				TRACE("gGameState = GS_INIT_FRONTEND;");
			}
			else
			{
				gGameState = GS_INIT_PLAYING_GAME;
				TRACE("gGameState = GS_INIT_PLAYING_GAME;");
			}
		}

		FrontEndMenuManager.m_bFirstTime = false;
		FrontEndMenuManager.m_bWantToRestart = false;
	}

#ifdef KEEP_FRONTEND_LOADED
	// since we kept our frontend loaded free the textures now
	FrontEndMenuManager.UnloadTextures(true);
#endif

#ifndef MASTER
	if ( gbModelViewer )
		CAnimViewer::Shutdown();
	else
#endif
	if ( gGameState == GS_PLAYING_GAME )
		CGame::ShutDown();

	DMAudio.Terminate();

	_psFreeVideoModeList();

	/*
	 * Tidy up the 3D (RenderWare) components of the application...
	 */
	RsEventHandler(rsRWTERMINATE, nil);

	/*
	 * Free the platform dependent data...
	 */
	RsEventHandler(rsTERMINATE, nil);

	return 0;
}

#endif
