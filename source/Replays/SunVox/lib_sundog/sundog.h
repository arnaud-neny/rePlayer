#pragma once

/*
SunDog configuration (possible defines)
Window Manager:
  COLOR8BITS		//8 bit color
  COLOR16BITS		//16 bit color
  COLOR32BITS		//32 bit color
  RGB565		//Default for 16bit color
  RGB555		//16bit color in Windows GDI
  RGB556		//16bit color in OSX Quartz
  GRAYSCALE		//Used with COLOR8BITS for grayscale palette
  X11			//X11 support
  OPENGL		//OpenGL support
  OPENGLES		//OpenGL ES
  GDI			//GDI support
  DIRECTDRAW		//DirectDraw (Windows), GAPI (WinCE), SDL (Linux)
  FRAMEBUFFER		//Use framebuffer (may be virtual, without DIRECTDRAW)
System: (most of these defines will be set automatically)
  OS_UNIX		//OS = some UNIX variation (Linux, FreeBSD...); POSIX-compatible
  OS_LINUX		//OS = Linux
  OS_WIN		//OS = Windows
  OS_WINCE		//OS = WindowsCE
  OS_MACOS		//OS = macOS
  OS_IOS		//OS = iOS
  OS_ANDROID		//OS = Android
  OS_EMSCRIPTEN		//OS = JavaScript-based system
  OS_NAME		//"linux","win32","win64"...
  ARCH_NAME		//"x86_64","x86"...
  ARCH_xxxx		//xxxx: X86_64,X86...
  CPUMARK		//0..10: 0 - slow CPU (e.g. 100 MHz ARM without FPU); 10 - fast modern CPU;
  HEAPSIZE		//Average memory size (in MB) available for application on the weakest device: 16,32,64,256...
Other:
  ONLY44100		//Sample rate other then 44100 is not allowed
  NOLOG			//No log messages
*/

//ANDROID ARM (OLD) : __arm__ __ARM_ARCH __ARM_ARCH < 7
//ANDROID ARM SOFTFP (NEW) : __arm__ __ARM_ARCH __ARM_ARCH >= 7
//WINCE ARM : __ARM_ARCH_4__ UNDER_CE
//MAEMO : __ARM_ARCH_7A__
//RPI : __ARM_ARCH_6__ __ARM_ARCH=6
//HARDFP : (32bit only) __ARM_PCS_VFP

#ifndef ARCH_NAME
    #if defined(i386) || defined(__i386) || defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__) || defined(_M_X86) || defined(_X86_) || defined(_M_IX86) // rePlayer
	#define ARCH_NAME "x86"
        #define ARCH_X86
    #endif
    #if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64) || defined(_M_X64) || defined(_M_AMD64)
        #define ARCH_NAME "x86_64"
        #define ARCH_X86_64
    #endif
    #ifdef __arm__
	#ifdef __ARM_ARCH_7__
	    #define ARCH_NAME "armv7"
	#endif
	#ifdef __ARM_ARCH_7A__
	    #define ARCH_NAME "armv7-a"
	#endif
	#ifdef __ARM_ARCH_7R__
	    #define ARCH_NAME "armv7-r"
	#endif
	#ifdef __ARM_ARCH_7M__
	    #define ARCH_NAME "armv7-m"
	#endif
	#ifdef __ARM_ARCH_7S__
	    #define ARCH_NAME "armv7-s"
	#endif
	#ifndef ARCH_NAME
	    //Older ARM architecture:
    	    #define ARCH_NAME "arm"
    	#endif
        #define ARCH_ARM
        #if ( defined(__ARM_ARCH) && __ARM_ARCH <= 5 ) || defined(__ARM_ARCH_4__)
    	    //No VFP
        #else
    	    //VFP:
	    #define ARM_VFP
        #endif
	#ifdef __ARM_PCS_VFP
	    //VFP + FPU-specific calling convention:
	    #define HARDFP 
	#endif
    #endif
    #if defined(__aarch64__) || defined(__arm64__) || defined(__ARM_ARCH_ISA_A64) || defined(_M_ARM64)
        #define ARCH_NAME "arm64"
        #define ARCH_ARM64
	#define ARM_VFP
    #endif
    #ifdef __mips__
	#define ARCH_NAME "mips"
	#define ARCH_MIPS
    #endif
    #ifdef __EMSCRIPTEN__
	#define ARCH_EMSCRIPTEN 
	#define ARCH_NAME "emscripten"
    #endif
#endif

#if defined(ARCH_X86_64) || defined(ARCH_X86)
    #if !defined(__SSE__) && !defined(NOSIMD)
        //MSVC:
	#define __SSE__ 1
	#define __SSE2__ 1
	#if !defined(SIMDLIMIT) || SIMDLIMIT>=30
	    #define __SSE3__ 1
	    #if !defined(SIMDLIMIT) || SIMDLIMIT>=31
	        #define __SSSE3__ 1
	        #if !defined(SIMDLIMIT) || SIMDLIMIT>=41
	    	    #define __SSE4_1__ 1
		    #define __SSE4_2__ 1
		#endif
	    #endif
	#endif
    #endif
#endif

#ifdef OS_MAEMO
    #define OS_NAME "maemo linux"
    #ifdef OPENGL
	#define OPENGLES
    #endif
    #define HEAPSIZE 32
#endif

#ifdef OS_RASPBERRY_PI
    #define OS_NAME "raspberry pi linux (legacy)"
    #ifdef OPENGL
	#define OPENGLES
    #endif
    #define HEAPSIZE 64
#endif

#ifdef __ANDROID__
    #define OS_ANDROID
    #define OS_NAME "android linux"
    #ifdef OPENGL
	#define OPENGLES
    #endif
    #if defined(ARCH_ARM64) || defined(ARCH_X86_64)
	#define HEAPSIZE 128
    #else
	#define HEAPSIZE 64
    #endif
    #define VIRTUALKEYBOARD
    #define MULTITOUCH
#endif

#ifdef __linux__
    #define OS_LINUX
    #ifndef OS_NAME
	//Common Linux:
	#define OS_NAME "linux"
	#define HEAPSIZE 512
    #endif
#endif

#ifdef __APPLE__
    #include "TargetConditionals.h"
    #if TARGET_OS_IPHONE
	#define OS_IOS
	#define OS_NAME "ios"
        #ifdef OPENGL
    	    #define OPENGLES
        #endif
	#if defined(ARCH_ARM64) || defined(ARCH_X86_64)
	    #define HEAPSIZE 128
	#else
	    #define HEAPSIZE 64
        #endif
        #define VIRTUALKEYBOARD
        #define MULTITOUCH
    #else
	#define OS_MACOS
	#define OS_NAME "macos (osx)"
	#define HEAPSIZE 512
    #endif
    #define OS_APPLE
    #define HANDLE_THREAD_EVENTS CFRunLoopRunInMode( kCFRunLoopDefaultMode, 0, 1 );
#endif

#ifndef UNDER_CE
#if defined(_WIN32) || defined(_WIN64)
    #define OS_WIN
    #ifdef _WIN64
	#define OS_NAME "win64"
    #else
	#define OS_NAME "win32"
    #endif
    #define HEAPSIZE 512
    #ifdef MULTITOUCH
	#define _WIN32_WINNT 0x0601 //Windows7 API support
    #endif
#endif
#endif

#ifdef UNDER_CE
    #define OS_WINCE
    #define OS_NAME "wince"
    #define HEAPSIZE 16
    #define ONLY44100
    #define VIRTUALKEYBOARD
    #ifndef NOMIDI
	#define NOMIDI
    #endif
#endif

#ifdef __EMSCRIPTEN__
    #define OS_EMSCRIPTEN
    #define OS_NAME "emscripten"
    #define HEAPSIZE 16
    #ifndef NOMIDI
	#define NOMIDI
    #endif
#endif

#ifndef HANDLE_THREAD_EVENTS
    #define HANDLE_THREAD_EVENTS /**/
#endif

#if defined(OS_LINUX) || defined(OS_FREEBSD) || defined(OS_APPLE) || defined(OS_EMSCRIPTEN)
    #define OS_UNIX
#endif

#ifndef OS_NAME
    #error OS_NAME not defined
#endif

#ifndef ARCH_NAME
    #error ARCH_NAME not defined
#endif

#ifndef CPUMARK
    #define CPUMARK 10
#endif

#ifndef HEAPSIZE
    #define HEAPSIZE 512
#endif

/*
RESTRICT type qualifier
Usage example: void fn( void* RESTRICT ptr1, void* RESTRICT ptr2 );
in this case the compiler is allowed to assume that ptr1 and ptr2 point to different locations and updating one pointer will not affect the other pointers.
*/
#if defined(__ICC) || defined(__INTEL_COMPILER)
    #define RESTRICT		restrict
#elif defined(__GNUC__) || defined(__llvm__) || defined(__clang__)
    #define RESTRICT		__restrict__
#elif defined(_MSC_VER)
    #define RESTRICT		__restrict
#else
    #define RESTRICT		__restrict__
#endif

/*
COMPILER_MEMORY_BARRIER() limits the compiler optimizations that can remove or reorder memory accesses across the point of the call.
This barrier does not affect permutations at the CPU/cache level of the modern processors: use atomic_thread_fence() and atomics instead.
*/
#if defined(__ICC) || defined(__INTEL_COMPILER)
    #define COMPILER_MEMORY_BARRIER() __memory_barrier()
#elif defined(__GNUC__) || defined(__llvm__) || defined(__clang__)
    #define COMPILER_MEMORY_BARRIER() asm volatile( "" ::: "memory" )
#elif defined(_MSC_VER)
    #define COMPILER_MEMORY_BARRIER() _ReadWriteBarrier()
#else
    #define COMPILER_MEMORY_BARRIER()
#endif

#ifndef SUNDOG_NO_DEFAULT_INCLUDES

#ifndef SUNDOG_NO_DEFAULT_INCLUDES2

#include <stdbool.h>
#ifdef  __cplusplus
    #if 0 // rePlayer __cplusplus < 201103L
	#define __STDC_LIMIT_MACROS //to get UINT32_MAX, SIZE_MAX, etc. from stdint.h
	#include <stdint.h>
    #else
	#include <cstdint>
    #endif
#else
    #include <stdint.h>
#endif
typedef unsigned int uint;
//int and uint are at least 16 bits in size; usually it is 32 on 32/64-bit systems;
//BUT сarefully use this type on 8/16-bit systems!
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <new>
#ifdef OS_EMSCRIPTEN
    #include <emscripten.h>
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    #include <windows.h>
#endif

/*#ifndef SIZE_MAX
    #if defined(ARCH_X86_64) || defined(ARCH_ARM64) || defined(_LP64) || defined(__LP64__)
        #define SIZE_MAX 0xFFFFFFFFFFFFFFFF
    #else
        #define SIZE_MAX 0xFFFFFFFF
    #endif
#endif*/

#if 1 // rePlayer __cplusplus >= 201103L
    #define CPP_NOEXCEPT noexcept
    #define CPP_OVERRIDE override
#else
    #define CPP_OVERRIDE
    #define CPP_NOEXCEPT
    #define nullptr NULL
#endif

#if 1 // rePlayer __cplusplus >= 201103L || defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4)
    #include <atomic>
    typedef std::atomic<void*> atomic_vptr;
    typedef std::atomic<char*> atomic_cptr;
#else
    //For old compilers without atomic support
    //Not fully atomic, of course... If possible, don't use this code :)
    #define NO_BUILTIN_ATOMIC_OPS 1
    typedef void* atomic_vptr;
    typedef char* atomic_cptr;
    namespace std
    {
	typedef volatile int atomic_int;
	typedef volatile uint atomic_uint;
	typedef volatile size_t atomic_size_t;
	enum memory_order
	{
	    memory_order_relaxed,
	    memory_order_consume,
	    memory_order_acquire,
	    memory_order_release,
	    memory_order_acq_rel,
	    memory_order_seq_cst
	};
    }
    inline void atomic_thread_fence( int order ) {};
    #define atomic_init( V, I ) { *(V) = I; }
    template <typename T> inline bool compare_and_write( volatile T* v, T old_val, T new_val )
    {
	if( *v != old_val )
	    return false;
	*v = new_val;
	return true;
    }
    template <typename T> inline bool atomic_compare_exchange_strong( volatile T* obj, T* expected, T val )
    {
	COMPILER_MEMORY_BARRIER();
	if( *obj == *expected )
	{
	    *obj = val;
	    if( *obj == val )
		return true;
	}
	*expected = *obj;
	COMPILER_MEMORY_BARRIER();
	return false;
    }
    template <typename T> inline bool atomic_compare_exchange_weak( volatile T* obj, T* expected, T val )
    {
	return atomic_compare_exchange_strong( obj, expected, val );
    }
    template <typename T> inline bool atomic_compare_exchange_weak_explicit( volatile T* obj, T* expected, T val, int m1, int m2 )
    {
	return atomic_compare_exchange_strong( obj, expected, val );
    }
    template <typename T> inline T atomic_fetch_add( volatile T* v, T i )
    {
	COMPILER_MEMORY_BARRIER();
	T rv;
	while( 1 )
	{
	    rv = *v;
	    if( compare_and_write(v,rv,rv+i) ) break;
	}
	COMPILER_MEMORY_BARRIER();
	return rv;
    }
    template <typename T> inline T atomic_fetch_add_explicit( volatile T* v, T i, int m ) { return atomic_fetch_add( v, i ); }
    template <typename T> inline T atomic_fetch_sub( volatile T* v, T i )
    {
	COMPILER_MEMORY_BARRIER();
	T rv;
	while( 1 )
	{
	    rv = *v;
	    if( compare_and_write(v,rv,rv-i) ) break;
	}
	COMPILER_MEMORY_BARRIER();
	return rv;
    }
    template <typename T> inline T atomic_fetch_sub_explicit( volatile T* v, T i, int m ) { return atomic_fetch_sub( v, i ); }
    template <typename T> inline T atomic_exchange( volatile T* v, T i )
    {
	COMPILER_MEMORY_BARRIER();
	T rv;
	while( 1 )
	{
	    rv = *v;
	    if( compare_and_write(v,rv,i) ) break;
	}
	COMPILER_MEMORY_BARRIER();
	return rv;
    }
    template <typename T> inline void atomic_store( volatile T* v, T i )
    {
	COMPILER_MEMORY_BARRIER();
	*v = i;
    }
    template <typename T> inline void atomic_store_explicit( volatile T* v, T i, int m ) { atomic_store( v, i ); }
    template <typename T> inline T atomic_load( volatile T* v )
    {
	T rv = *v;
	COMPILER_MEMORY_BARRIER();
	return rv;
    }
    template <typename T> inline T atomic_load_explicit( volatile T* v, int m ) { return atomic_load( v ); }
#endif

#if defined(OS_ANDROID) && __ANDROID_API__ < 18
    #define log2( x ) ( log( x ) / log( 2 ) )
    #define log2f( x ) ( logf( x ) / logf( 2 ) )
#endif
#if defined(OS_WINCE)
    //Broken log2() on WinCE (math library bug?)
    #define LOG2( x ) ( log( x ) / log( 2 ) )
    #define LOG2F( x ) ( logf( x ) / logf( 2 ) )
#else
    #define LOG2( x ) log2( x )
    #define LOG2F( x ) log2f( x )
#endif
#if defined(__SSE__)
    //Disable denormal numbers to speed up the DSP code:
    #define DISABLE_DENORMAL_NUMBERS
#endif
#if defined(ARM_VFP) && ( defined(ARCH_ARM64) || defined(ARCH_ARM) ) && ( !defined(OS_APPLE) )
    //Disable denormal numbers to speed up the DSP code:
    //(the result depends on the system and CPU)
    //iOS: already disabled?
    //Android: no positive results (sometimes it works slower) on the tested devices.
    //Linux: ARM1176JZF-S (old RPi): denormal results are trapped to slow "support code" (software emulation);
    //       ARM Cortex-A7 (new RPi): fast hardware support for denormalised numbers.
    //#define DISABLE_DENORMAL_NUMBERS
#endif
#if !defined(DISABLE_DENORMAL_NUMBERS) && ( defined(ARCH_X86) || defined(ARCH_X86_64) )
    /*
    Denormal numbers (subnormal numbers) fill the underflow gap around zero in floating-point arithmetic.
    Some systems handle denormal values in hardware, in the same way as normal values.
    Others leave the handling of denormal values to system software, only handling normal values and zero in hardware.
    Handling denormal values in software always leads to a significant decrease in performance.
    We can avoid denormals by adding an extremely quiet noise in some places (where very small numbers can appear).
    */
    #define DENORMAL_NUMBERS
    extern uint32_t g_denorm_rand_state;
    #define denorm_add_white_noise( val ) \
    { \
        g_denorm_rand_state = g_denorm_rand_state * 1234567UL + 890123UL; \
        uint32_t mantissa = g_denorm_rand_state & 0x807F0000; /* Keep only most significant bits */ \
        union { uint32_t i; float f; } dnv; dnv.i = mantissa | 0x1E000000; /* Set exponent -67 */ \
        val += dnv.f; \
    }
#else
    #define denorm_add_white_noise( val ) /**/
#endif

enum //SunDog result codes
{
    SD_RES_SUCCESS = 0, //DON'T CHANGE THIS VALUE!
    SD_RES_TIMEOUT,
    SD_RES_ERR, //unknown error
    SD_RES_ERR_MALLOC, //memory allocation error
    SD_RES_ERR_FOPEN, //can't open the file
    SD_RES_ERR_FREAD, //can't read the file
};

#endif //SUNDOG_NO_DEFAULT_INCLUDES2

#ifndef SUNDOG_NO_DEFAULT_INCLUDES1

struct sundog_engine;

//Most used SunDog functions:
#include "memory/memory.h"
#include "file/file.h"
#include "time/time.h"
#include "thread/thread.h"
#include "misc/misc.h"
#include "sound/sound.h"
#include "log/log.h"
#ifndef SUNDOG_VER
    #include "wm/wm.h"
#endif
#include "main/main.h"

#endif //SUNDOG_NO_DEFAULT_INCLUDES1

#endif //SUNDOG_NO_DEFAULT_INCLUDES
