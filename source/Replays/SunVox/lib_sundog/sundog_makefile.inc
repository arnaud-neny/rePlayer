##
## Defined in the app MakeFile:
##   TARGET_OS - system name & build features:
##     linux_android, linux_android16,
##     linux_sdl, linux_sdl_nojack, linux_x11_opengl, linux_x11_opengles,
##     linux_armel, linux_armel_sdl_nojack, linux_armel_sdl_maemo,
##     windows, windows_opengl, windows_vst3, windowsce,
##     emscripten_sdl
##   TARGET_ARCH - architecture name: arm, arm64, x86, x86_64, mips, mips64; NOT ABI NAME! ABI can be selected by some additional options;
##   SUNDOG_DEFINES
##   APPNAME
## Paths:
##   ROOT
##   SUNDOG
## Optional:
##   LIBS - additional libraries; for example: -lGL
##   LDFLAGS - additional linker flags
##   CFLAGS - additional C/C++ flags
##

##
## Options:
## (example: MAKE_WITHOUT_SIMD=true)
##   BUILD_TYPE : Debug (default); Release;
##   SUNDOG_VER : empty string - default (v1); 2 - modified version of the SunDog;
##   MAKE_DYNAMIC_LIB
##   MAKE_STATIC_LIB
##   MAKE_MODULE (vst3)
##   MAKE_WITH_OBJDUMP
##   MAKE_WITH_CONSOLE
##   MAKE_WITH_MULTITOUCH
##   MAKE_WITH_SDL : 1 - SDL1.2; 2 - SDL2;
##   MAKE_WITHOUT_MAIN
##   MAKE_WITHOUT_GUI
##   MAKE_WITHOUT_SIMD
##   MAKE_FOR_SLOW_CPU - for example, for the latest CPU, but for its mobile version (lower frequency, reduced number of cores, etc.);
##   MAKE_WITH_SSE_VER : empty string - default (ssse3 or higher); sse3;
##

##
## Paths
##

ifneq (,$(findstring android16,$(TARGET_OS)))
    #Old NDK r23c with Android 4.1 (API level 16) support:
    ANDROID_NDK = /home/alex/soft/android-ndk-r23c
else
    #Newest NDK:
    ANDROID_NDK = /home/alex/soft/android-ndk-r28
endif
ANDROID_GDK = /home/alex/soft/android-gdk
MACOS_TOOLCHAIN_BIN = /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin
MACOS_SDK = /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.0.sdk
VST3_SDK = /home/alex/soft/VST_SDK/vst3sdk
VST3_SDK_BASE = $(VST3_SDK)/base
VST3_SDK_IF = $(VST3_SDK)/pluginterfaces
VST3_SDK_HELPER = $(VST3_SDK)/public.sdk/source
IOS_TOOLCHAIN_BIN = /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin
ifneq (,$(findstring x86,$(TARGET_ARCH)))
    IOS_SDK = /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator26.0.sdk
else
    IOS_SDK = /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS26.0.sdk
endif
RPI_SDK = /opt/vc
MAEMO_TOOLCHAIN_BIN = /home/alex/soft/maemo/new/cs2007q3-glibc2.5-arm7/bin
MAEMO_SYSROOT = /home/alex/soft/maemo/new/FREMANTLE_ARMEL

##
## Target configuration
##

CFLAGS2 = 
LDFLAGS2 = 
DEFAULT_LIBS = 

TARGET_ABI = $(TARGET_ARCH)

CPPSTD ?= -std=gnu++17

##
## Linux
##
ifneq (,$(findstring linux,$(TARGET_OS)))
    CC = gcc
    CXX = $(CC) $(CPPSTD)
    LD = $(CC)
    AR = ar
    OBJDUMP = objdump
    CFLAGS2 += -ffunction-sections -fdata-sections -Wall -Wno-unused-variable -Wno-multichar -pipe
    LDFLAGS2 += -Wl,--gc-sections
    DEFAULT_LIBS = -lpthread -lstdc++ -lasound -lm -ldl
ifneq (,$(findstring x86,$(TARGET_ARCH))) #x86 and x86_64?
ifeq (,$(findstring android,$(TARGET_OS))) #not Android?
ifneq ($(MAKE_DYNAMIC_LIB),true) #not dynamic lib?
#GCC produces PIE (Position Independent Executable) by default in Debian 9.
#14 jan 2018: It's ok, but ALL graphics file managers don't recognize such executables. So i disabled it temporarily:
    LDFLAGS2 += -no-pie
endif #not dynamic lib
endif #not android
endif #x86 and x86_64
##
## Linux: Raspberry PI (legacy)
##
ifneq (,$(findstring raspberry_pi,$(TARGET_OS)))
    CFLAGS2 += -DOS_RASPBERRY_PI -I$(RPI_SDK)/include -I$(RPI_SDK)/include/interface/vcos/pthreads -I$(RPI_SDK)/include/interface/vmcs_host/linux
    LDFLAGS2 += -L$(RPI_SDK)/lib
endif
##
## Linux: Maemo
##
ifneq (,$(findstring linux_armel,$(TARGET_OS)))
    MAEMO_ARCH = -march=armv7a -mcpu=cortex-a8 -mfpu=vfp -mfloat-abi=softfp
#softfp: hardware FPU + soft floating point ABI; SLOW! Compiler can make smart choices about when and if it generates emulated or real FPU instructions;
    CC = $(MAEMO_TOOLCHAIN_BIN)/arm-none-linux-gnueabi-gcc $(MAEMO_ARCH)
    CXX = $(CC) $(CPPSTD)
    LD = $(CC)
    AR = $(MAEMO_TOOLCHAIN_BIN)/arm-none-linux-gnueabi-ar $(MAEMO_ARCH)
    OBJDUMP = $(MAEMO_TOOLCHAIN_BIN)/arm-none-linux-gnueabi-objdump
    CFLAGS2 += --sysroot=$(MAEMO_SYSROOT)
    LDFLAGS2 += --sysroot=$(MAEMO_SYSROOT)
    DEFAULT_LIBS = -lpthread -lstdc++ -lasound -ldl
endif
ifneq (,$(findstring maemo,$(TARGET_OS)))
    CFLAGS2 += -DOS_MAEMO
endif
##
## Linux: Android
##
ifneq (,$(findstring android,$(TARGET_OS)))
    SYSROOT = $(ANDROID_NDK)/sysroot
    CFLAGS2 += \
	-I$(SYSROOT)/usr/include \
	-I$(ANDROID_NDK)/sources/android/native_app_glue \
	-Wno-gnu-inline-cpp-without-extern \
	-fno-exceptions \
	-fno-rtti
    DEFAULT_LIBS = -llog -landroid -lOpenSLES -lm -ldl -lc++_static -lc++abi
ifeq ($(SUNDOG_VER),)
    CFLAGS2 += -DGL_GLEXT_PROTOTYPES=1
endif
ifeq ($(SUNDOG_VER),2)
    CFLAGS2 += \
	-I$(ANDROID_GDK)/game-activity/prefab-src/modules/game-activity/include \
	-I$(ANDROID_GDK)/src/common
endif
    OBJDUMP = $(ANDROID_NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-objdump
    AR = $(ANDROID_NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-ar
ifeq ($(TARGET_ARCH),x86) 
    ifneq (,$(findstring android16,$(TARGET_OS)))
	CC = $(ANDROID_NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin/i686-linux-android16-clang
    else
	CC = $(ANDROID_NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin/i686-linux-android24-clang
    endif
    CXX = $(CC)++ $(CPPSTD)
    LD = $(CC)
    TARGET_ABI = x86
endif
ifeq ($(TARGET_ARCH),x86_64)
    ifneq (,$(findstring android16,$(TARGET_OS)))
	CC = $(ANDROID_NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin/x86_64-linux-android21-clang
	LDFLAGS2 += -Wl,-z,max-page-size=16384
    else
	CC = $(ANDROID_NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin/x86_64-linux-android24-clang
    endif
    CXX = $(CC)++ $(CPPSTD)
    LD = $(CC)
    TARGET_ABI = x86_64
endif
ifeq ($(TARGET_ARCH),arm)
    ifneq (,$(findstring android16,$(TARGET_OS)))
	CC = $(ANDROID_NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin/armv7a-linux-androideabi16-clang
    else
	CC = $(ANDROID_NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin/armv7a-linux-androideabi24-clang
    endif
    CXX = $(CC)++ $(CPPSTD)
    LD = $(CC)
    TARGET_ABI = armeabi-v7a
endif
ifeq ($(TARGET_ARCH),arm64)
    ifneq (,$(findstring android16,$(TARGET_OS)))
	CC = $(ANDROID_NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android21-clang
	LDFLAGS2 += -Wl,-z,max-page-size=16384
    else
	CC = $(ANDROID_NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang
    endif
    CXX = $(CC)++ $(CPPSTD)
    LD = $(CC)
    TARGET_ABI = arm64-v8a
endif
    MAKE_DYNAMIC_LIB = true
endif #android
endif #linux
##
## macOS
##
ifeq ($(TARGET_OS),macos)
    CC = $(MACOS_TOOLCHAIN_BIN)/clang
    CXX = $(CC) -x c++ $(CPPSTD)
    CMM = $(CC) -x objective-c++
    LD = $(CC)
    AR = $(MACOS_TOOLCHAIN_BIN)/ar
    OBJDUMP = objdump
    CFLAGS2 += \
	-target $(TARGET_ARCH)-apple-macos10.12 \
	-Wno-multichar \
	-isysroot $(MACOS_SDK) \
	-pipe
    LDFLAGS2 += \
	-target $(TARGET_ARCH)-apple-macos10.12 \
	-isysroot $(MACOS_SDK)
    DEFAULT_LIBS = -lpthread -lstdc++ -framework Cocoa -framework Carbon -framework AudioUnit -framework CoreAudio
endif
##
## iOS
##
ifeq ($(TARGET_OS),ios)
    CC = $(IOS_TOOLCHAIN_BIN)/clang
    CXX = $(CC) $(CPPSTD)
    CMM = $(CC) -x objective-c++
    LD = $(CC)
    AR = $(IOS_TOOLCHAIN_BIN)/ar
    OBJDUMP = $(IOS_TOOLCHAIN_BIN)/objdump
    LIBTOOL = $(IOS_TOOLCHAIN_BIN)/libtool -syslibroot $(IOS_SDK)
    CFLAGS2 += \
	-Wno-multichar \
	-isysroot $(IOS_SDK) \
	-miphoneos-version-min=11.0 \
	-fembed-bitcode \
	-pipe
    DEFAULT_LIBS = -framework Foundation -framework AudioToolbox -framework AVFoundation
endif
##
## Windows
##
ifneq (,$(findstring windows,$(TARGET_OS)))
ifneq ($(TARGET_OS),windowsce) #not WinCE?
ifeq ($(TARGET_ARCH),x86_64) 
    CC = x86_64-w64-mingw32-gcc
    CXX = $(CC) $(CPPSTD)
#   LD = x86_64-w64-mingw32-g++ - this is required for option -static-libstdc++ to work!
    LD = x86_64-w64-mingw32-g++
    AR = x86_64-w64-mingw32-ar
    OBJDUMP = x86_64-w64-mingw32-objdump
    WINDRES = x86_64-w64-mingw32-windres
    CFLAGS2 += \
	-ffunction-sections -fdata-sections \
	-Wall -Wno-unused-variable -Wno-multichar \
	-fno-exceptions \
	-pipe \
	-Iresources/
endif
ifeq ($(TARGET_ARCH),x86)
    CC = i686-w64-mingw32-gcc
    CXX = $(CC) $(CPPSTD)
    LD = i686-w64-mingw32-g++
    AR = i686-w64-mingw32-ar
    OBJDUMP = i686-w64-mingw32-objdump
    WINDRES = i686-w64-mingw32-windres
    CFLAGS2 += \
	-ffunction-sections -fdata-sections \
	-malign-double \
	-fno-exceptions \
	-Wall -Wno-unused-variable -Wno-multichar \
	-pipe \
	-Iresources/
endif
    FILE_EXT = .exe
    LDFLAGS2 += -Wl,--gc-sections -static-libgcc -static-libstdc++
    DEFAULT_LIBS = -ldsound -ldxguid -lwinmm -lole32
#   -static-libgcc will remove the libgcc_s_sjlj-1 dependency (we don't use exceptions at all)
#   -static-libstdc++ will remove the libstdc++ dependency
endif #not WinCE?
endif #windows
##
## WindowsCE
##
ifeq ($(TARGET_OS),windowsce)
    CC = arm-mingw32ce-gcc
    CXX = arm-mingw32ce-g++ $(CPPSTD)
    LD = $(CC)
    AR = arm-mingw32ce-ar
    OBJDUMP = arm-mingw32ce-objdump
    WINDRES = arm-mingw32ce-windres
    FILE_EXT = .exe
    CFLAGS2 += \
	-ffunction-sections -fdata-sections \
	-fno-exceptions \
	-Wno-multichar \
	-pipe \
	-Iresources/ \
	-I$(SUNDOG)main/wince/
    LDFLAGS2 += -Wl,--gc-sections
    DEFAULT_LIBS = -laygshell -lceshell -lstdc++ -static
endif
##
## Emscripten
##
ifneq (,$(findstring emscripten,$(TARGET_OS)))
    CC = emcc
    CXX = $(CC) $(CPPSTD)
    LD = $(CC)
    FILE_EXT = .html
    CFLAGS2 += \
	-fno-exceptions \
	-fno-rtti \
	-DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0 \
	-Wall -Wno-unused-variable -Wno-multichar -pipe
    LDFLAGS2 += -s ALLOW_MEMORY_GROWTH=1
    DEFAULT_LIBS = 
endif

##
## x86
##
ifeq ($(TARGET_ARCH),x86)
ifneq (,$(findstring android,$(TARGET_OS)))
    CFLAGS2 += -DCPUMARK=0 
else
endif #not android
    CFLAGS2 += -m32 -ffast-math -march=i686
    LDFLAGS2 += -m32
    LIBTOOL_FLAGS = -static -arch_only i686
ifneq ($(MAKE_WITHOUT_SIMD),true)
ifneq (,$(findstring windows,$(TARGET_OS)))
#Filter Pro + ffast-math + march=pentium3 (min) crashes the Windows 10 machines; (i686-w64-mingw32-gcc 4.9.1)
#also it seems there is no big difference between the pure i686 and i686+sse+vectorize :)
#fix 15 jan 2018: works now with the i686-w64-mingw32-gcc 6.3.0
#                 but still there is no difference; furthermore, i686+sse+vectorize works slower sometimes (??)
else
ifeq ($(MAKE_WITH_SSE_VER),sse3)
    CFLAGS2 += -mfpmath=sse -msse3 -ftree-vectorize
else
    CFLAGS2 += -mfpmath=sse -mssse3 -ftree-vectorize
endif
endif #not windows
endif #with SIMD
endif #x86
##
## x86_64
##
ifeq ($(TARGET_ARCH),x86_64) 
    CFLAGS2 += -m64 -ffast-math
ifneq ($(MAKE_WITHOUT_SIMD),true)
ifeq ($(MAKE_WITH_SSE_VER),sse3)
    CFLAGS2 += -msse3 -ftree-vectorize
else
    CFLAGS2 += -mssse3 -ftree-vectorize
endif
endif
    LDFLAGS2 += -m64
endif #x86_64
##
## ARM
##
## Some of these defines may be defined automatically below:
##   ARM_VFP - VFP used;
##   HARDFP - VFP + FPU-specific calling convention;
##
ifneq (,$(findstring arm,$(TARGET_ARCH)))
    CFLAGS2 += -DCPUMARK=0 -ffast-math
ifneq (,$(findstring android,$(TARGET_OS)))
ifeq ($(TARGET_ARCH),arm)
    ARM_DEFINES = -march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16
    LDFLAGS2 += -march=armv7-a -Wl,--fix-cortex-a8
endif #arm
endif #android
ifneq (,$(findstring ios,$(TARGET_OS)))
    ARM_DEFINES = -arch $(TARGET_ARCH)
    LIBTOOL_FLAGS = -static -arch_only $(TARGET_ARCH)
endif #iOS
ifneq ($(MAKE_WITHOUT_SIMD),true)
    CFLAGS2 += -ftree-vectorize
endif
endif #arm
CFLAGS2 += $(ARM_DEFINES)
##
## Emscripten
##
ifeq ($(TARGET_ARCH),emscripten) 
    CFLAGS2 += -ffast-math
endif

##
## Handling additional MAKE_* options
##

##
## All systems
##
ifeq ($(BUILD_TYPE),) #default (debug)
    BUILD_TYPE = Debug
endif
ifneq ($(SUNDOG_VER),)
    CFLAGS2 += -DSUNDOG_VER=2
endif
ifeq ($(MAKE_MODULE),vst3)
    MAKE_DYNAMIC_LIB = true
    CFLAGS2 += \
	-I$(VST3_SDK) \
	-I$(VST3_SDK)/vstgui4 \
	-DSUNDOG_MODULE \
	-DSUNDOG_MODULE_VST3
    ifneq (,$(findstring Release,$(BUILD_TYPE))) #Release?
	CFLAGS2 += -DRELEASE=1
    endif
    ifneq (,$(findstring Debug,$(BUILD_TYPE))) #Debug?
	CFLAGS2 += -DDEVELOPMENT=1
    endif
endif #VST3
ifeq ($(MAKE_WITH_MULTITOUCH),true)
    CFLAGS2 += -DMULTITOUCH
    ifneq (,$(findstring linux,$(TARGET_OS))) #linux?
	ifeq (,$(findstring sdl,$(TARGET_OS))) #sdl?
	    DEFAULT_LIBS += -lXi
	endif
    endif
endif
ifeq ($(MAKE_WITH_SDL),1)
    CFLAGS2 += -DSDL=1 -DSDL12
    DEFAULT_LIBS += -lSDL
endif
ifeq ($(MAKE_WITH_SDL),2)
    CFLAGS2 += -DSDL=2
    DEFAULT_LIBS += -lSDL2
endif
ifeq ($(MAKE_WITHOUT_MAIN),true)
    CFLAGS2 += -DNOMAIN
endif
ifeq ($(MAKE_WITHOUT_GUI),true)
    CFLAGS2 += -DNOGUI
endif
ifeq ($(MAKE_WITHOUT_SIMD),true)
    CFLAGS2 += -DNOSIMD
endif
ifeq ($(MAKE_FOR_SLOW_CPU),true)
    CFLAGS2 += -DCPUMARK=0
endif
ifeq ($(MAKE_WITH_OBJDUMP),true)
    CFLAGS2 += -g
    POSTLD = $(OBJDUMP) -S $(APPNAME)$(FILE_EXT) >$(APPNAME)$(FILE_EXT)_$(TARGET_OS)_$(TARGET_ARCH).objdump
endif
ifeq ($(BUILD_TYPE),Release)
    CFLAGS2 += -O3 -DNDEBUG
    LDFLAGS2 += -O3
    ifeq (,$(findstring -g,$(CFLAGS))) #no -g in CFLAGS?
	ifeq (,$(findstring -g,$(CFLAGS2))) #no -g in CFLAGS2?
	    ifneq ($(MAKE_WITH_OBJDUMP),true) #no OBJDUMP?
		LDFLAGS2 += -s
	    endif #no OBJDUMP
	endif #no -g in CFLAGS2
    endif #no -g in CFLAGS
endif
ifeq ($(BUILD_TYPE),Debug)
    CFLAGS2 += -g -DSMEM_USE_NAMES
    LDFLAGS2 += -g
endif
ifeq ($(MAKE_STATIC_LIB),true)
    FILE_EXT = .a
endif
##
## Linux
##
ifneq (,$(findstring linux,$(TARGET_OS)))
ifeq ($(MAKE_DYNAMIC_LIB),true)
    CFLAGS2 += -fPIC
    LDFLAGS2 += -shared -Wl,-soname,$(APPNAME).so
    FILE_EXT = .so
endif
endif
##
## MacOS
##
ifeq ($(TARGET_OS),macos)
ifeq ($(MAKE_DYNAMIC_LIB),true)
    CFLAGS2 += -fPIC
    LDFLAGS2 += -dynamiclib
    FILE_EXT = .dylib
endif
endif
##
## iOS
##
ifeq ($(TARGET_OS),ios)
endif
##
## Windows
##
ifneq (,$(findstring windows,$(TARGET_OS)))
ifneq ($(TARGET_OS),windowsce)
ifneq ($(MAKE_WITH_CONSOLE),true)
    LDFLAGS2 += -mwindows
endif
ifeq ($(MAKE_DYNAMIC_LIB),true)
    FILE_EXT = .dll
    LDFLAGS2 += -shared -Wl,--kill-at
endif
endif
endif
##
## Emscripten
##
ifneq (,$(findstring emscripten,$(TARGET_OS)))
ifeq ($(MAKE_DYNAMIC_LIB),true)
    FILE_EXT = .js
    LDFLAGS2 += -s NO_EXIT_RUNTIME=1
endif
ifeq ($(MAKE_WITH_SDL),2)
    CFLAGS2 += -s USE_SDL=2
    LDFLAGS2 += -s USE_SDL=2
endif
endif

FINAL_CFLAGS = $(CFLAGS) $(CFLAGS2) $(SUNDOG_DEFINES) $(ADD_SUNDOG_DEFINES) -I./ -I$(SUNDOG) -DAPPNAME="\"$(APPNAME)\""
FINAL_LDFLAGS = $(LDFLAGS) $(LDFLAGS2)
FINAL_LIBS = $(LIBS) $(DEFAULT_LIBS)

##
## Object files and dependencies
##

SD_CONTAINER = $(SUNDOG)container/
SD_FILE = $(SUNDOG)file/
SD_LOG = $(SUNDOG)log/
SD_MEM = $(SUNDOG)memory/
SD_MISC = $(SUNDOG)misc/
SD_NET = $(SUNDOG)net/
SD_SOUND = $(SUNDOG)sound/
SD_THREAD = $(SUNDOG)thread/
SD_TIME = $(SUNDOG)time/
SD_VIDEO = $(SUNDOG)video/
SD_VULKAN = $(SUNDOG)vulkan/
SD_GPU = $(SUNDOG)gpu/
SD_WM = $(SUNDOG)wm/
SD_GUI = $(SUNDOG)gui/
SD_MAIN = $(SUNDOG)main/

SD_CONTAINER_OBJS = sd_container.o
SD_FILE_OBJS = \
    sd_file.o \
    sd_file_format.o \
    sd_file_format_rw.o
SD_LOG_OBJS = sd_log.o
SD_MAIN_OBJS = sd_main.o
SD_MEM_OBJS = sd_memory.o
SD_MISC_OBJS = sd_misc.o
SD_NET_OBJS = sd_net.o
SD_SOUND_OBJS = sd_sound.o
SD_THREAD_OBJS = sd_thread.o
SD_TIME_OBJS = sd_time.o
SD_VIDEO_OBJS = sd_video.o
SD_VULKAN_OBJS = sd_vulkan.o
SD_GPU_OBJS = sd_gpu.o sd_gpu_shaders.o
SD_WM_OBJS = \
    wm.o \
    wm_fonts.o \
    wm_image.o \
    wm_keymap.o \
    wm_wbd.o \
    wm_framebuffer.o \
    wm_hnd.o \
    wm_hnd_color_theme.o \
    wm_hnd_dialog.o \
    wm_hnd_fdialog.o \
    wm_hnd_keymap.o \
    wm_hnd_list.o \
    wm_hnd_popup.o \
    wm_hnd_preferences.o \
    wm_hnd_scale.o \
    wm_hnd_virtual_keyboard.o \
    wm_hnd_webserver.o \
    devrgn.o
SD_GUI_OBJS = \
    sd_gui.o \
    sd_gui_widgets_button.o \
    sd_gui_widgets_native.o \
    sd_gui_widgets_root.o
OBJS = \
    $(SD_FILE_OBJS) \
    $(SD_LOG_OBJS) \
    $(SD_MAIN_OBJS) \
    $(SD_MEM_OBJS) \
    $(SD_MISC_OBJS) \
    $(SD_NET_OBJS) \
    $(SD_SOUND_OBJS) \
    $(SD_THREAD_OBJS) \
    $(SD_TIME_OBJS) \
    $(SD_VIDEO_OBJS)
ifneq ($(MAKE_WITHOUT_GUI),true)
ifeq ($(SUNDOG_VER),)
    OBJS += $(SD_WM_OBJS)
endif
ifeq ($(SUNDOG_VER),2)
    OBJS += $(SD_GUI_OBJS)
endif
endif
ifeq ($(SUNDOG_VER),2)
    OBJS += $(SD_CONTAINER_OBJS) $(SD_VULKAN_OBJS) $(SD_GPU_OBJS)
endif

ifeq ($(TARGET_OS),ios)
    OBJS += sd_sound_ios.o sd_file_apple.o
endif

ifeq ($(TARGET_OS),macos)
    OBJS += sd_file_apple.o
endif

ifneq (,$(findstring android,$(TARGET_OS)))
    OBJS += sd_android_bridge.o
ifneq ($(MAKE_WITHOUT_MAIN),true)
    OBJS += android_native_app_glue.o
ifeq ($(SUNDOG_VER),2)
    OBJS += GameActivity.o GameActivityEvents.o gametextinput.o system_utils.o
endif
endif
endif

ifeq ($(MAKE_MODULE),vst3)
    OBJS += \
	sd_main_vst3.o
    OBJS += \
	baseiids.o \
	fbuffer.o \
	fdebug.o \
	fdynlib.o \
	fobject.o \
	fstreamer.o \
	fstring.o \
	timer.o \
	updatehandler.o \
	fcondition.o \
	flock.o
    OBJS += \
	conststringtable.o \
	coreiids.o \
	funknown.o \
	ustring.o
    OBJS += \
	commoniids.o \
	memorystream.o \
	openurl.o \
	pluginview.o \
	systemclipboard_win32.o \
	threadchecker_win32.o \
	dllmain.o \
	pluginfactory.o \
	vstbus.o \
	vstcomponentbase.o \
	vstinitiids.o \
	vstparameters.o \
	vstsinglecomponenteffect.o
endif

##
## Dependencies
##

SD_DEPS = \
    $(SUNDOG)sundog.h \
    $(SD_FILE)file.h \
    $(SD_LOG)log.h \
    $(SD_MAIN)main.h \
    $(SD_MEM)memory.h \
    $(SD_MISC)misc.h \
    $(SD_NET)net.h \
    $(SD_SOUND)sound.h \
    $(SD_THREAD)thread.h \
    $(SD_TIME)time.h \
    $(SD_VIDEO)video.h
ifeq ($(SUNDOG_VER),)
    SD_DEPS += $(SD_WM)wm.h
endif
ifeq ($(SUNDOG_VER),2)
    SD_DEPS += \
    $(SD_MAIN)main2.h \
    $(SD_CONTAINER)sd_container.h \
    $(SD_VULKAN)sd_vulkan.h \
    $(SD_GPU)sd_gpu.h \
    $(SD_GUI)sd_gui.h
endif
SD_MAIN_DEPS = 
ifeq ($(SUNDOG_VER),2)
    SD_MAIN_DEPS += \
    $(SD_MAIN)main_unix_x11.hpp
endif
SD_SOUND_DEPS = \
    $(SD_SOUND)sound.h \
    $(SD_SOUND)sound_common.hpp \
    $(SD_SOUND)sound_common_jack.hpp \
    $(SD_SOUND)sound_linux.hpp \
    $(SD_SOUND)sound_linux_midi.hpp \
    $(SD_SOUND)sound_win.hpp \
    $(SD_SOUND)sound_win_midi.hpp \
    $(SD_SOUND)sound_android.hpp \
    $(SD_SOUND)sound_android_midi.hpp \
    $(SD_SOUND)sound_macos.hpp \
    $(SD_SOUND)sound_apple_midi.hpp \
    $(SD_SOUND)sound_emscripten.hpp
SD_WM_DEPS = \
    $(SD_WM)wm.h \
    $(SD_WM)wm_struct.h \
    $(SD_WM)wm_android.hpp \
    $(SD_WM)wm_win.hpp \
    $(SD_WM)wm_wince.hpp \
    $(SD_WM)wm_opengl.hpp \
    $(SD_WM)wm_win_ddraw.hpp \
    $(SD_WM)wm_x11.hpp \
    $(SD_WM)wm_sdl.hpp
SD_VIDEO_DEPS = \
    $(SD_VIDEO)video.h \
    $(SD_VIDEO)video_linux.hpp \
    $(SD_VIDEO)video_android.hpp \
    $(SD_VIDEO)video_ios.hpp
SD_GUI_DEPS = \
    $(SD_GUI)sd_gui.h \
    $(SD_GUI)sd_os_window.h \
    $(SD_GUI)sd_os_window_desc_pairs.h

##
## Final build rules
##

ifneq (,$(findstring linux,$(TARGET_OS)))
.SECONDEXPANSION:
all: $$(OBJS)
ifeq ($(MAKE_STATIC_LIB),true)
	$(AR) rcs $(APPNAME)$(FILE_EXT) $(OBJS)
	$(POSTLD)
else
	$(LD) $(FINAL_LDFLAGS) -o $(APPNAME)$(FILE_EXT) $(OBJS) $(FINAL_LIBS)
	$(POSTLD)
endif
endif

ifeq ($(TARGET_OS),macos)
.SECONDEXPANSION:
all: $$(OBJS)
ifeq ($(MAKE_STATIC_LIB),true)
	$(LIBTOOL) $(LIBTOOL_FLAGS) -o $(APPNAME)$(FILE_EXT) $(OBJS)
	$(POSTLD)
else
	$(LD) $(FINAL_LDFLAGS) -o $(APPNAME)$(FILE_EXT) $(OBJS) $(FINAL_LIBS)
	$(POSTLD)
endif
endif

ifeq ($(TARGET_OS),ios)
.SECONDEXPANSION:
all: $$(OBJS)
ifeq ($(MAKE_STATIC_LIB),true)
	$(LIBTOOL) $(LIBTOOL_FLAGS) -o $(APPNAME)$(FILE_EXT) $(OBJS)
	$(POSTLD)
else
	$(LD) $(FINAL_LDFLAGS) -o $(APPNAME)$(FILE_EXT) $(OBJS) $(FINAL_LIBS)
	$(POSTLD)
endif
endif

ifneq (,$(findstring windows,$(TARGET_OS)))
ifneq ($(TARGET_OS),windowsce)
.SECONDEXPANSION:
all: $$(OBJS)
ifeq ($(MAKE_STATIC_LIB),true)
	$(AR) rcs $(APPNAME)$(FILE_EXT) $(OBJS)
else
ifneq ($(MAKE_WITHOUT_GUI),true)
	$(WINDRES) resources/win_res.rc win_res.o
	$(LD) $(FINAL_LDFLAGS) -o $(APPNAME)$(FILE_EXT) $(OBJS) win_res.o $(FINAL_LIBS)
else
	$(LD) $(FINAL_LDFLAGS) -o $(APPNAME)$(FILE_EXT) $(OBJS) $(FINAL_LIBS)
endif
	$(POSTLD)
endif
endif
endif

ifeq ($(TARGET_OS),windowsce)
.SECONDEXPANSION:
all: $$(OBJS)
ifeq ($(MAKE_STATIC_LIB),true)
	$(AR) rcs $(APPNAME)$(FILE_EXT) $(OBJS)
	$(POSTLD)
else
	$(WINDRES) resources/win_res.rc win_res.o
	$(LD) $(FINAL_LDFLAGS) -o $(APPNAME)$(FILE_EXT) $(OBJS) win_res.o $(FINAL_LIBS)
	$(POSTLD)
endif
endif

ifneq (,$(findstring emscripten,$(TARGET_OS)))
.SECONDEXPANSION:
all: $$(OBJS)
	$(LD) $(FINAL_LDFLAGS) -o $(APPNAME)$(FILE_EXT) $(OBJS) $(FINAL_LIBS)
	$(POSTLD)
endif

#SUNDOG BASE
sd_container.o: $(SD_CONTAINER)sd_container.cpp $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $< -o $@
sd_file.o: $(SD_FILE)file.cpp $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $< -o $@
sd_file_format.o: $(SD_FILE)file_format.cpp $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $< -o $@
sd_file_format_rw.o: $(SD_FILE)file_format_rw.cpp $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $< -o $@
sd_log.o: $(SD_LOG)log.cpp $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $< -o $@
sd_main.o: $(SD_MAIN)main.cpp $(SD_MAIN_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $< -o $@
sd_memory.o: $(SD_MEM)memory.cpp $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $< -o $@
sd_misc.o: $(SD_MISC)misc.cpp $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $< -o $@
sd_net.o: $(SD_NET)net.cpp $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $< -o $@
sd_sound.o: $(SD_SOUND)sound.cpp $(SD_SOUND_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $< -o $@
sd_thread.o: $(SD_THREAD)thread.cpp $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $< -o $@
sd_time.o: $(SD_TIME)time.cpp $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $< -o $@
sd_video.o: $(SD_VIDEO)video.cpp $(SD_VIDEO_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $< -o $@
sd_vulkan.o: $(SD_VULKAN)sd_vulkan.cpp $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<
sd_gpu.o: $(SD_GPU)sd_gpu.cpp $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<
sd_gpu_shaders.o: $(SD_GPU)sd_gpu_shaders.cpp $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<
wm.o: $(SD_WM)wm.cpp $(SD_WM_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<
wm_fonts.o: $(SD_WM)wm_fonts.cpp $(SD_WM_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<
wm_framebuffer.o: $(SD_WM)wm_framebuffer.cpp $(SD_WM_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<
wm_image.o: $(SD_WM)wm_image.cpp $(SD_WM_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<
wm_keymap.o: $(SD_WM)wm_keymap.cpp $(SD_WM_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<
wm_wbd.o: $(SD_WM)wm_wbd.cpp $(SD_WM_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<
wm_hnd.o: $(SD_WM)wm_hnd.cpp $(SD_WM_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<
wm_hnd_color_theme.o: $(SD_WM)wm_hnd_color_theme.cpp $(SD_WM_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<
wm_hnd_dialog.o: $(SD_WM)wm_hnd_dialog.cpp $(SD_WM_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<
wm_hnd_fdialog.o: $(SD_WM)wm_hnd_fdialog.cpp $(SD_WM_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<
wm_hnd_keymap.o: $(SD_WM)wm_hnd_keymap.cpp $(SD_WM_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<
wm_hnd_list.o: $(SD_WM)wm_hnd_list.cpp $(SD_WM_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<
wm_hnd_popup.o: $(SD_WM)wm_hnd_popup.cpp $(SD_WM_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<
wm_hnd_preferences.o: $(SD_WM)wm_hnd_preferences.cpp $(SD_WM_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<
wm_hnd_scale.o: $(SD_WM)wm_hnd_scale.cpp $(SD_WM_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<
wm_hnd_virtual_keyboard.o: $(SD_WM)wm_hnd_virtual_keyboard.cpp $(SD_WM_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<
wm_hnd_webserver.o: $(SD_WM)wm_hnd_webserver.cpp $(SD_WM_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<
devrgn.o: $(SD_WM)regions/devrgn.cpp $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<
sd_gui.o: $(SD_GUI)sd_gui.cpp $(SD_GUI_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<
sd_gui_widgets_button.o: $(SD_GUI)sd_gui_widgets_button.cpp $(SD_GUI_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<
sd_gui_widgets_native.o: $(SD_GUI)sd_gui_widgets_native.cpp $(SD_GUI_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<
sd_gui_widgets_root.o: $(SD_GUI)sd_gui_widgets_root.cpp $(SD_GUI_DEPS) $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $<

#ANDROID
ifeq ($(SUNDOG_VER),)
android_native_app_glue.o: $(ANDROID_NDK)/sources/android/native_app_glue/android_native_app_glue.c
	$(CC) $(FINAL_CFLAGS) -c $<
sd_android_bridge.o: $(SUNDOG)main/android/sundog_bridge.cpp $(SUNDOG)main/android/sundog_bridge1.hpp $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $< -o $@
endif
ifeq ($(SUNDOG_VER),2)
android_native_app_glue.o: $(ANDROID_GDK)/game-activity/prefab-src/modules/game-activity/src/game-activity/native_app_glue/android_native_app_glue.c
	$(CC) $(FINAL_CFLAGS) -c $<
GameActivity.o: $(ANDROID_GDK)/game-activity/prefab-src/modules/game-activity/src/game-activity/GameActivity.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
GameActivityEvents.o: $(ANDROID_GDK)/game-activity/prefab-src/modules/game-activity/src/game-activity/GameActivityEvents.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
gametextinput.o: $(ANDROID_GDK)/game-activity/prefab-src/modules/game-activity/src/game-text-input/gametextinput.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
system_utils.o: $(ANDROID_GDK)/src/common/system_utils.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
sd_android_bridge.o: $(SUNDOG)main/android/sundog_bridge.cpp $(SUNDOG)main/android/sundog_bridge2.hpp $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $< -o $@
endif

#IOS
sd_sound_ios.o: $(SD_SOUND)sound_ios.mm $(SD_SOUND_DEPS) $(SD_DEPS)
	$(CMM) $(FINAL_CFLAGS) -c $< -o $@
sd_file_apple.o: $(SD_FILE)file_apple.mm $(SD_DEPS)
	$(CMM) $(FINAL_CFLAGS) -c $< -o $@

#VST3
sd_main_vst3.o: $(SD_MAIN)main_vst3.cpp $(SD_MAIN)main_vst3.h $(SD_DEPS)
	$(CXX) $(FINAL_CFLAGS) -c $< -o $@
#VST3 BASE
baseiids.o: $(VST3_SDK_BASE)/source/baseiids.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
fbuffer.o: $(VST3_SDK_BASE)/source/fbuffer.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
fdebug.o: $(VST3_SDK_BASE)/source/fdebug.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
fdynlib.o: $(VST3_SDK_BASE)/source/fdynlib.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
fobject.o: $(VST3_SDK_BASE)/source/fobject.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
fstreamer.o: $(VST3_SDK_BASE)/source/fstreamer.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
fstring.o: $(VST3_SDK_BASE)/source/fstring.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
timer.o: $(VST3_SDK_BASE)/source/timer.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
updatehandler.o: $(VST3_SDK_BASE)/source/updatehandler.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
fcondition.o: $(VST3_SDK_BASE)/thread/source/fcondition.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
flock.o: $(VST3_SDK_BASE)/thread/source/flock.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
#VST3 INTERFACES
conststringtable.o: $(VST3_SDK_IF)/base/conststringtable.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
coreiids.o: $(VST3_SDK_IF)/base/coreiids.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
funknown.o: $(VST3_SDK_IF)/base/funknown.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
ustring.o: $(VST3_SDK_IF)/base/ustring.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
#VST3 HELPER CLASSES
commoniids.o: $(VST3_SDK_HELPER)/common/commoniids.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
memorystream.o: $(VST3_SDK_HELPER)/common/memorystream.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
openurl.o: $(VST3_SDK_HELPER)/common/openurl.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
pluginview.o: $(VST3_SDK_HELPER)/common/pluginview.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
systemclipboard_win32.o: $(VST3_SDK_HELPER)/common/systemclipboard_win32.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
threadchecker_win32.o: $(VST3_SDK_HELPER)/common/threadchecker_win32.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
dllmain.o: $(VST3_SDK_HELPER)/main/dllmain.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
pluginfactory.o: $(VST3_SDK_HELPER)/main/pluginfactory.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
vstbus.o: $(VST3_SDK_HELPER)/vst/vstbus.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
vstcomponentbase.o: $(VST3_SDK_HELPER)/vst/vstcomponentbase.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
vstinitiids.o: $(VST3_SDK_HELPER)/vst/vstinitiids.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
vstparameters.o: $(VST3_SDK_HELPER)/vst/vstparameters.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<
vstsinglecomponenteffect.o: $(VST3_SDK_HELPER)/vst/vstsinglecomponenteffect.cpp
	$(CXX) $(FINAL_CFLAGS) -c $<

clean:
	rm -f *.o *.a *.optrpt
