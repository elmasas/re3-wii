#-------------------------------------------------------------------------------
.SUFFIXES:
#-------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)

include $(DEVKITPRO)/wut/share/wut_rules

#-------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# DATA is a list of directories containing data files
# INCLUDES is a list of directories containing header files
#-------------------------------------------------------------------------------
TARGET		:=	openal
VERSION		:=  1.19.1

BUILD		:=	build
SOURCES		:=	wiiu
DATA		:=	data
INCLUDES	:=	include \
				OpenAL32/Include \
				Alc \
				common \
				wiiu

AL_COMMON_SRC :=    common/alcomplex.c \
					common/almalloc.c \
					common/atomic.c \
					common/rwlock.c \
					common/threads.c \
					common/uintmap.c

AL_OPENAL_SRC :=    OpenAL32/alAuxEffectSlot.c \
					OpenAL32/alBuffer.c \
					OpenAL32/alEffect.c \
					OpenAL32/alError.c \
					OpenAL32/alExtension.c \
					OpenAL32/alFilter.c \
					OpenAL32/alListener.c \
					OpenAL32/alSource.c \
					OpenAL32/alState.c \
					OpenAL32/event.c \
					OpenAL32/sample_cvt.c

AL_ALC_SRC	:=      Alc/ALc.c \
					Alc/ALu.c \
					Alc/alconfig.c \
					Alc/bs2b.c \
					Alc/converter.c \
					Alc/mastering.c \
					Alc/ringbuffer.c \
					Alc/effects/autowah.c \
					Alc/effects/chorus.c \
					Alc/effects/compressor.c \
					Alc/effects/dedicated.c \
					Alc/effects/distortion.c \
					Alc/effects/echo.c \
					Alc/effects/equalizer.c \
					Alc/effects/fshifter.c \
					Alc/effects/modulator.c \
					Alc/effects/null.c \
					Alc/effects/pshifter.c \
					Alc/effects/reverb.c \
					Alc/filters/filter.c \
					Alc/filters/nfc.c \
					Alc/filters/splitter.c \
					Alc/helpers.c \
					Alc/hrtf.c \
					Alc/uhjfilter.c \
					Alc/ambdec.c \
					Alc/bformatdec.c \
					Alc/panning.c \
					Alc/mixvoice.c \
					Alc/mixer/mixer_c.c \
					Alc/backends/base.c \
					Alc/backends/null_backend.c \
					Alc/backends/loopback.c \
					Alc/backends/sdl2.c

ALSOURCEFILES_C := $(AL_COMMON_SRC) $(AL_OPENAL_SRC) $(AL_ALC_SRC)

#-------------------------------------------------------------------------------
# options for code generation
#-------------------------------------------------------------------------------
CFLAGS	:=	-Wall -O2 -ffunction-sections \
			$(MACHDEP)

CFLAGS	+=	$(INCLUDE) -D__WIIU__ -D__WUT__ \
			-DVERSION=\"$(VERSION)\" -DAL_LIBTYPE_STATIC -DAL_ALEXT_PROTOTYPES

CXXFLAGS	:= $(CFLAGS) -std=gnu++14
CFLAGS  +=  -std=gnu11

ASFLAGS	:=	$(ARCH)
LDFLAGS	=	$(ARCH) $(RPXSPECS) -Wl,-Map,$(notdir $*.map)

LIBS	:= 

#-------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level
# containing include and lib
#-------------------------------------------------------------------------------
LIBDIRS	:= $(PORTLIBS) $(WUT_ROOT)


#-------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#-------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#-------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/lib/lib$(TARGET).a
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir)) \
					$(foreach sf,$(ALSOURCEFILES_C),$(CURDIR)/$(dir $(sf)))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c))) \
				$(foreach f,$(ALSOURCEFILES_C),$(notdir $(f)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#-------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#-------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#-------------------------------------------------------------------------------
	export LD	:=	$(CC)
#-------------------------------------------------------------------------------
else
#-------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#-------------------------------------------------------------------------------
endif
#-------------------------------------------------------------------------------

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES))
export OFILES_SRC	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES 	:=	$(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN	:=	$(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all

#-------------------------------------------------------------------------------
all: $(BUILD)

lib:
	@[ -d $@ ] || mkdir -p $@

$(BUILD) : lib
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#-------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -rf build lib

#-------------------------------------------------------------------------------
else
.PHONY:	all

DEPENDS	:=	$(OFILES:.o=.d)

#-------------------------------------------------------------------------------
# main targets
#-------------------------------------------------------------------------------

$(OUTPUT)	:	$(OFILES)

$(OFILES_SRC)	: $(HFILES)
$(OFILES_SRC)	: $(HFILES_BIN)

#-------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#-------------------------------------------------------------------------------
%.bin.o	%_bin.h :	%.bin
#-------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

#-------------------------------------------------------------------------------
endif
#-------------------------------------------------------------------------------