/*
    thread.cpp - thread management
    This file is part of the SunDog engine.
    Copyright (C) 2004 - 2025 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"

#ifdef OS_ANDROID
    #include "main/android/sundog_bridge.h"
#endif

#ifdef OS_UNIX
    #include <errno.h>
    #include <unistd.h>
    #include <sys/syscall.h>
    #include <sys/stat.h> //mode constants
    #include <fcntl.h> // O_* constants
#endif

#ifdef OS_EMSCRIPTEN
    #ifndef __EMSCRIPTEN_PTHREADS__
	sem_t* sem_open( const char* name, int oflag, mode_t mode, unsigned int value ) { return NULL; }
	int sem_close( sem_t* sem ) { return 0; }
	int sem_timedwait( sem_t* sem, const struct timespec* abs_timeout ) { return 0; }
    #endif
#endif

int sthread_global_init()
{
    return 0;
}

int sthread_global_deinit()
{
    return 0;
}

//
// Thread
//

#ifdef OS_UNIX
void* sthread_handler( void* arg )
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
DWORD __stdcall sthread_handler( void* arg )
#endif
{
    sundog_denormal_numbers_check();
    sthread* th = (sthread*)arg;
    th->proc( th->arg );
    th->finished = true;
#if defined(OS_ANDROID) && !defined(NOMAIN)
    if( th->sd ) android_sundog_release_jni( th->sd );
#endif
#ifdef OS_UNIX
    pthread_exit( NULL );
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    ExitThread( 0 );
#endif
    return 0;
}

int sthread_create( sthread* th, sundog_engine* sd, void* (*proc)(void*), void* arg, uint32_t flags )
{
    int rv = 0;
    th->sd = sd;
    th->arg = arg;
    th->proc = proc;
    th->finished = false;
#ifdef OS_UNIX
    pthread_attr_init( &th->attr );
    int err;
    err = pthread_create( &th->th, &th->attr, &sthread_handler, (void*)th );
    if( err )
    {
	slog( "pthread_create error %d\n", err );
	return 1;
    }
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    th->th = CreateThread( NULL, 0, &sthread_handler, (void*)th, 0, NULL );
#endif
    return rv;
}

static void sthread_cleanup( sthread* th )
{
#ifdef OS_UNIX
    pthread_attr_destroy( &th->attr );
#endif
    th->proc = nullptr;
}

int sthread_destroy( sthread* th, int timeout )
{
    int rv = 2; //0 - successful; 1 - timeout; 2 - some error

    if( !th ) return 2;
    if( !th->proc ) return 0;

    int err;

    bool dont_destroy_after_timeout = false;
    if( timeout < 0 ) 
    {
	timeout = -timeout;
	dont_destroy_after_timeout = true;
    }

    if( timeout == STHREAD_TIMEOUT_INFINITE )
    {
	//Infinite:
#ifdef OS_UNIX
	err = pthread_join( th->th, 0 );
	if( err )
	{
	    slog( "pthread_join() error %d\n", err ); 
	}
	else rv = 0;
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
	err = WaitForSingleObject( th->th, INFINITE );
	if( err == WAIT_OBJECT_0 )
	{
	    //Thread terminated:
	    CloseHandle( th->th );
	    rv = 0;
	}
	else
	{
	    slog( "WaitForSingleObject() error %d\n", err );
	}
#endif
	if( rv < 2 ) sthread_cleanup( th );
	return rv;
    }

    int timeout_counter = timeout; //Timeout in milliseconds
    int step = 10; //ms
    while( timeout_counter > 0 )
    {
	if( th->finished )
	{
#ifdef OS_UNIX
	    err = pthread_join( th->th, 0 );
	    if( err ) slog( "pthread_join() error %d\n", err );
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
	    CloseHandle( th->th );
#endif
	    rv = 0;
	    break;
	}
#ifdef OS_UNIX
	struct timespec delay;
	delay.tv_sec = 0;
	delay.tv_nsec = 1000000 * step;
	nanosleep( &delay, NULL ); //Sleep for delay time
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
	err = WaitForSingleObject( th->th, step );
	if( err == WAIT_OBJECT_0 )
	{
	    //Thread terminated:
	    CloseHandle( th->th );
	    rv = 0;
	    break;
	}
	if( err == (int)WAIT_FAILED )
	{
	    rv = 2;
	}
#endif
	timeout_counter -= step;
    }
    if( timeout_counter <= 0 )
    {
	if( dont_destroy_after_timeout )
	{
	    return 1;
	}
#ifdef OS_UNIX
#ifndef OS_ANDROID
	err = pthread_cancel( th->th );
	if( err ) slog( "pthread_cancel() error %d\n", err );
#endif
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
	TerminateThread( th->th, 0 );
	CloseHandle( th->th );
#endif
	rv = 1;
    }

    if( rv < 2 ) sthread_cleanup( th );

    return rv;
}

int sthread_clean( sthread* th )
{
    if( !th ) return -1;
    smem_clear( th, sizeof( sthread ) );
    return 0;
}

int sthread_is_empty( sthread* th )
{
    if( !th ) return -1;
    if( !th->proc ) return 1;
    return 0;
}

int sthread_is_finished( sthread* th )
{
    if( !th ) return -1;
    if( th->finished )
	return 1;
    return 0;
}

sthread_tid_t sthread_gettid()
{
#ifdef OS_LINUX
    return syscall( SYS_gettid );
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    return GetCurrentThreadId();
#endif
#ifdef OS_APPLE
    uint64_t id = 0;
    pthread_threadid_np( NULL, &id );
    return id;
#endif
    return 0;
}

sthread_pid_t sthread_getpid()
{
#ifdef OS_LINUX
    return syscall( SYS_getpid );
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    return GetCurrentProcessId();
#endif
#ifdef OS_APPLE
    //[[NSProcessInfo processInfo] processIdentifier]
#endif
    return 0;
}

//
// Mutex
//

int smutex_init( smutex* mutex, uint32_t flags )
{
    int retval = 0;
    if( !mutex ) return -1;
    mutex->flags = flags;
    while( 1 )
    {
#ifdef NO_BUILTIN_ATOMIC_OPS
	mutex->flags &= ~SMUTEX_FLAG_ATOMIC_SPINLOCK;
#else
	if( mutex->flags & SMUTEX_FLAG_ATOMIC_SPINLOCK )
	{
	    atomic_init( &mutex->lock, 0 );
	    break;
	}
#endif
#ifdef OS_UNIX
	pthread_mutexattr_t mutexattr;
	pthread_mutexattr_init( &mutexattr );
	pthread_mutexattr_settype( &mutexattr, PTHREAD_MUTEX_RECURSIVE );
	retval = pthread_mutex_init( &mutex->mutex, &mutexattr );
	pthread_mutexattr_destroy( &mutexattr );
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
	InitializeCriticalSection( &mutex->mutex );
#endif
	break;
    }
    if( retval == 0 ) mutex->flags |= SMUTEX_FLAG_INITIALIZED;
    return retval;
}

int smutex_destroy( smutex* mutex )
{
    int retval = 0;
    if( !mutex ) return -1;
    if( mutex->flags & SMUTEX_FLAG_INITIALIZED )
    {
	mutex->flags &= ~SMUTEX_FLAG_INITIALIZED;
    }
    else
    {
	return -1;
    }
    if( mutex->flags & SMUTEX_FLAG_ATOMIC_SPINLOCK )
    {
	return retval;
    }
#ifdef OS_UNIX
    retval = pthread_mutex_destroy( &mutex->mutex );
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    DeleteCriticalSection( &mutex->mutex );
#endif
    return retval;
}

//Return 0 if successful
int smutex_lock( smutex* mutex )
{
    int retval = 0;
    if( !mutex ) return -1;
#ifndef NO_BUILTIN_ATOMIC_OPS
    if( mutex->flags & SMUTEX_FLAG_ATOMIC_SPINLOCK )
    {
	while( 1 )
	{
	    int expected = 0;
    	    if( atomic_compare_exchange_strong_explicit( &mutex->lock, &expected, 1, std::memory_order_acq_rel, std::memory_order_relaxed ) ) break;
    	    while( atomic_load_explicit( &mutex->lock, std::memory_order_relaxed ) )
    	    {
    		//https://en.wikipedia.org/wiki/Test_and_test-and-set
    		//__builtin_ia32_pause() ?
    		//yield() ?
    	    }
    	}
	return retval;
    }
#endif
#ifdef OS_UNIX
    retval = pthread_mutex_lock( &mutex->mutex );
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    EnterCriticalSection( &mutex->mutex );
#endif
    return retval;
}

//Return 0 if successful
int smutex_trylock( smutex* mutex )
{
    int retval = 0;
    if( !mutex ) return -1;
#ifndef NO_BUILTIN_ATOMIC_OPS
    if( mutex->flags & SMUTEX_FLAG_ATOMIC_SPINLOCK )
    {
	int expected = 0;
        if( atomic_compare_exchange_strong_explicit( &mutex->lock, &expected, 1, std::memory_order_acq_rel, std::memory_order_relaxed ) )
    	    retval = 0;
    	else
    	    retval = 1;
	return retval;
    }
#endif
#ifdef OS_UNIX
    retval = pthread_mutex_trylock( &mutex->mutex );
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    if( TryEnterCriticalSection( &mutex->mutex ) != 0 )
	retval = 0;
    else 
	retval = 1;
#endif
    return retval;
}

int smutex_unlock( smutex* mutex )
{
    int retval = 0;
    if( !mutex ) return -1;
#ifndef NO_BUILTIN_ATOMIC_OPS
    if( mutex->flags & SMUTEX_FLAG_ATOMIC_SPINLOCK )
    {
	atomic_store_explicit( &mutex->lock, 0, std::memory_order_release );
	return retval;
    }
#endif
#ifdef OS_UNIX
    retval = pthread_mutex_unlock( &mutex->mutex );
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    LeaveCriticalSection( &mutex->mutex );
#endif
    return retval;
}

#ifdef SUNDOG_TEST
stime_ns_t g_t1, g_t2;
void* smutex_test_thread( void* user_data )
{
    slog( "T. SMUTEX TEST THREAD\n" );
    smutex* m = (smutex*)user_data;
    smutex_lock( m );
    g_t2 = stime_ns();
    smutex_unlock( m );
    slog( "T. SMUTEX TEST THREAD FINISHED\n" );
    return NULL;
}
int smutex_test( sundog_engine* sd )
{
    slog( "SMUTEX TEST...\n" );

    /*stime_ns_t t = stime_ns();
    stime_ns_t pt = stime_ns();
    int max = 1;
    int min = 99999999;
    while( 1 )
    {
        stime_ns_t tt = stime_ns();
        if( tt - t > 1000000000 * 2 ) break;
        int dt = tt - pt;
        if( dt > max ) max = dt;
        if( dt < min ) min = dt;
        pt = tt;
    }
    slog( "MIN: %f ms (%d); MAX: %f ms (%d);\n", (double)min*1000.0/1000000000.0, min, (double)max*1000.0/1000000000.0, max );*/

    smutex m;
    sthread th;
    int num_ops;
    stime_ns_t t1, t2;

    smutex_init( &m, 0 );
    smutex_lock( &m );

    sthread_create( &th, sd, smutex_test_thread, &m, 0 );
    stime_sleep( 500 );
    g_t1 = stime_ns();
    smutex_unlock( &m );
    stime_sleep( 100 );
    sthread_destroy( &th, 5000 );
    slog( "%f ms (%d)\n", (double)(g_t2-g_t1)/1000000000*1000, g_t2 - g_t1 );

    smutex_destroy( &m );

    slog( "[ WITH SPINLOCK ]\n" );

    smutex_init( &m, SMUTEX_FLAG_ATOMIC_SPINLOCK );
    smutex_lock( &m );

    sthread_create( &th, sd, smutex_test_thread, &m, 0 );
    stime_sleep( 500 );
    g_t1 = stime_ns();
    smutex_unlock( &m );
    stime_sleep( 100 );
    sthread_destroy( &th, 5000 );
    slog( "%f ms (%d)\n", (double)(g_t2-g_t1)/1000000000*1000, g_t2 - g_t1 );
    uint64_t d = g_t2 - g_t1;
    if( g_t1 > g_t2 ) d = g_t1 - g_t2;
    slog( "ns delta = " PRINTF_SIZET "\n", PRINTF_SIZET_CONV d );

    smutex_destroy( &m );

    slog( "[ IDLE LOCK/UNLOCK ]\n" );

    num_ops = 1024 * 1024 * 64;

    smutex_init( &m, 0 );
    t1 = stime_ns();
    for( int i = 0; i < num_ops; i++ )
    {
        smutex_lock( &m );
        smutex_unlock( &m );
    }
    t2 = stime_ns();
    smutex_destroy( &m );
    slog( "%f ms;\n", (double)(t2-t1)/1000000000*1000 );
    slog( "%f ms per op\n", (double)(t2-t1)/num_ops/1000000000*1000 );

    slog( "[ IDLE LOCK/UNLOCK WITH SPINLOCK ]\n" );

    smutex_init( &m, SMUTEX_FLAG_ATOMIC_SPINLOCK );
    t1 = stime_ns();
    for( int i = 0; i < num_ops; i++ )
    {
        smutex_lock( &m );
        smutex_unlock( &m );
    }
    t2 = stime_ns();
    smutex_destroy( &m );
    slog( "%f ms;\n", (double)(t2-t1)/1000000000*1000 );
    slog( "%f ms per op\n", (double)(t2-t1)/num_ops/1000000000*1000 );

    slog( "SMUTEX TEST FINISHED\n" );
    return 0;
}
#endif

//
// Semaphore
//

int ssemaphore_create( ssemaphore* sem, const char* name, uint32_t val, uint32_t flags )
{
    int rv = -1;
    int err;

    while( 1 )
    {
	smem_clear( sem, sizeof( ssemaphore ) );
#ifdef OS_UNIX
	if( name )
	{
	    sem->named_sem = sem_open( name, O_CREAT, S_IRUSR | S_IWUSR, val );
	    if( sem->named_sem == SEM_FAILED )
	    {
	        slog( "sem_open() error %d %s\n", errno, strerror( errno ) );
		break;
	    }
	}
#ifdef OS_APPLE
	if( !name )
	{
	    sem->sem = dispatch_semaphore_create( val );
            if( sem->sem == NULL )
            {
                slog( "dispatch_semaphore_create() error\n" );
                break;
            }
	}
#else
	if( !name )
	{
	    if( sem_init( &sem->sem, 0, val ) != 0 )
	    {
	        slog( "sem_init() error %d %s\n", errno, strerror( errno ) );
		break;
	    }
	}
#endif
#endif
#ifdef OS_WIN
	sem->sem = CreateSemaphoreA( NULL, val, 32767, name );
	if( sem->sem == NULL ) break;
#endif
#ifdef OS_WINCE
	WCHAR name16[ 32 ];
	utf8_to_utf16( (uint16_t*)name16, 32, name );
	sem->sem = CreateSemaphore( NULL, val, 32767, name16 );
	if( sem->sem == NULL ) break;
#endif
	rv = 0;
	break;
    }

    return rv;
}

int ssemaphore_destroy( ssemaphore* sem )
{
    int rv = -1;

    while( 1 )
    {
#ifdef OS_UNIX
	if( sem->named_sem )
	{
	    sem_close( sem->named_sem );
	    sem->named_sem = NULL;
	}
#ifdef OS_APPLE
	if( !sem->named_sem )
	{
	}
#else
	if( !sem->named_sem )
	{
	    sem_destroy( &sem->sem );
	}
#endif
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
	CloseHandle( sem->sem );
	sem->sem = NULL;
#endif
	rv = 0;
	break;
    }

    return rv;
}

int ssemaphore_wait( ssemaphore* sem, int timeout )
{
    int rv = -2; //0 - successful; -1 - timeout; -2 - some error
    int err = 0;

    while( 1 )
    {
#ifdef OS_UNIX
	sem_t* s = sem->named_sem;
#ifdef OS_APPLE
	if( !sem->named_sem )
	{
	    dispatch_time_t t = DISPATCH_TIME_NOW;
	    if( timeout )
	    {
		if( timeout == STHREAD_TIMEOUT_INFINITE )
		    t = DISPATCH_TIME_FOREVER;
		else
		    t = dispatch_time( DISPATCH_TIME_NOW, timeout * 1000000 );
	    }
	    if( dispatch_semaphore_wait( sem->sem, t ) )
	    {
		//timeout:
		rv = -1;
		break;
	    }
            rv = 0;
            break;
	}
#else
	if( !sem->named_sem ) s = &sem->sem;
#endif
	if( timeout == STHREAD_TIMEOUT_INFINITE )
	{
	    err = sem_wait( s );
	}
	if( timeout == 0 )
	{
	    err = sem_trywait( s );
	}
	if( timeout > 0 && timeout != STHREAD_TIMEOUT_INFINITE )
	{
#ifdef OS_APPLE
            //sem_timedwait() is not implemented:
            stime_ticks_t start_t = 0;
            while( 1 )
            {
                err = sem_trywait( s );
                if( err == 0 ) break;
                stime_ticks_t t = stime_ticks();
                if( start_t == 0 ) start_t = t;
                if( t - start_t >= (int64_t)timeout * stime_ticks_per_second() / 1000 ) break;
                stime_sleep( 1 );
            }
#else
	    struct timespec t;
	    clock_gettime( CLOCK_REALTIME, &t );
	    t.tv_sec += timeout / 1000;
	    t.tv_nsec += ( timeout % 1000 ) * 1000000;
	    if( t.tv_nsec >= 1000000000 )
	    {
	        t.tv_sec++;
	        t.tv_nsec -= 1000000000;
	    }
	    err = sem_timedwait( s, &t );
#endif
	}
	if( err )
	{
	    if( errno == ETIMEDOUT || errno == EAGAIN )
	        rv = -1;
	    else
		slog( "sem_wait() error %d %s\n", errno, strerror( errno ) );
	    break;
	}
#endif //OS_UNIX
#if defined(OS_WIN) || defined(OS_WINCE)
	int ms = timeout;
	if( timeout == STHREAD_TIMEOUT_INFINITE ) ms = INFINITE;
	err = WaitForSingleObject( sem->sem, ms );
	if( err == WAIT_TIMEOUT || err == WAIT_ABANDONED )
	{
	    rv = -1;
	    break;
	}
	if( err != WAIT_OBJECT_0 )
	{
	    rv = -2;
	    slog( "WaitForSingleObject() error %d\n", err );
	    break;
	}
#endif
	rv = 0;
	break;
    }

    return rv;
}

int ssemaphore_release( ssemaphore* sem )
{
    int rv = -1;

    while( 1 )
    {
#ifdef OS_UNIX
	sem_t* s = sem->named_sem;
#ifdef OS_APPLE
	if( !sem->named_sem )
	{
	    dispatch_semaphore_signal( sem->sem );
            rv = 0;
            break;
	}
#else
	if( !sem->named_sem ) s = &sem->sem;
#endif
	if( sem_post( s ) ) break;
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
	if( ReleaseSemaphore( sem->sem, 1, NULL ) == 0 ) break;
#endif
	rv = 0;
	break;
    }

    return rv;
}

#ifdef SUNDOG_TEST
#define SEMNAME "testsem4"
ssemaphore g_test_sem;
void* ssemaphore_test_thread( void* user_data )
{
    int err;
    ssemaphore named_sem;
    ssemaphore* sem = (ssemaphore*)user_data;
    if( !sem ) sem = &named_sem;
    if( user_data )
    {
	slog( "T. UNNAMED SEMAPHORE TEST THREAD\n" );
    }
    else
    {
	slog( "T. NAMED SEMAPHORE TEST THREAD\n" );
        err = ssemaphore_create( sem, SEMNAME, 1, 0 );
        if( err ) slog( "T. ssemaphore_create() error %d\n", err );
    }
    err = ssemaphore_wait( sem, 200 );
    if( err == -1 )
    {
	slog( "T. Timeout. Semaphore is locked in the parent thread. OK!\n" );
    }
    else
    {
	slog( "T. ssemaphore_wait() error %d\n", err );
    }
    slog( "T. waiting...\n" );
    err = ssemaphore_wait( sem, STHREAD_TIMEOUT_INFINITE ); if( err ) slog( "T1. (2) ssemaphore_wait() error %d\n", err );
    g_t2 = stime_ns();
    slog( "RESUMED T2 (thread 2)\n" );
    err = ssemaphore_release( sem ); if( err ) slog( "T1. (2) ssemaphore_release() error %d\n", err );
    if( !user_data )
	ssemaphore_destroy( sem );
    slog( "T. FINISHED\n" );
    return NULL;
}
int ssemaphore_test( sundog_engine* sd )
{
    int err;
    int err_cnt = 0;

    slog( "NAMED SEMAPHORE TEST...\n" );
    err = ssemaphore_create( &g_test_sem, SEMNAME, 1, 0 );
    if( err ) { slog( "ssemaphore_create() error %d\n", err ); err_cnt += err; }
    slog( "wait/release without waiting...\n" );
    err = ssemaphore_wait( &g_test_sem, 0 ); if( err ) { slog( "ssemaphore_wait() error %d\n", err ); err_cnt += err; }
    err = ssemaphore_release( &g_test_sem ); if( err ) { slog( "ssemaphore_release() error %d\n", err ); err_cnt += err; }
    slog( "wait/release with waiting...\n" );
    err = ssemaphore_wait( &g_test_sem, 5 ); if( err ) { slog( "(2) ssemaphore_wait() error %d\n", err ); err_cnt += err; }
    err = ssemaphore_release( &g_test_sem ); if( err ) { slog( "(2) ssemaphore_release() error %d\n", err ); err_cnt += err; }
    slog( "two threads...\n" );
    err = ssemaphore_wait( &g_test_sem, 0 ); //lock
    if( err ) { slog( "(3) ssemaphore_wait() error %d\n", err ); err_cnt += err; }
    sthread th;
    sthread_create( &th, sd, ssemaphore_test_thread, NULL, 0 );
    stime_sleep( 3000 );
    g_t1 = stime_ns();
    err = ssemaphore_release( &g_test_sem ); //unlock: the second thread (waiting) will resume...
    slog( "UNLOCK T (thread 1)\n" );
    if( err ) { slog( "(3) ssemaphore_release() error %d\n", err ); err_cnt += err; }
    sthread_destroy( &th, 5000 );
    ssemaphore_destroy( &g_test_sem );
    slog( "%f ms (%d)\n", (double)(g_t2-g_t1)/1000000000*1000, g_t2 - g_t1 );
    slog( "NAMED SEMAPHORE TEST FINISHED\n\n" );

    slog( "UNNAMED SEMAPHORE TEST...\n" );
    err = ssemaphore_create( &g_test_sem, NULL, 1, 0 );
    if( err ) { slog( "ssemaphore_create() error %d\n", err ); err_cnt += err; }
    slog( "wait/release without waiting...\n" );
    err = ssemaphore_wait( &g_test_sem, 0 ); if( err ) { slog( "ssemaphore_wait() error %d\n", err ); err_cnt += err; }
    err = ssemaphore_release( &g_test_sem ); if( err ) { slog( "ssemaphore_release() error %d\n", err ); err_cnt += err; }
    slog( "wait/release with waiting...\n" );
    err = ssemaphore_wait( &g_test_sem, 5 ); if( err ) { slog( "(2) ssemaphore_wait() error %d\n", err ); err_cnt += err; }
    err = ssemaphore_release( &g_test_sem ); if( err ) { slog( "(2) ssemaphore_release() error %d\n", err ); err_cnt += err; }
    slog( "two threads...\n" );
    err = ssemaphore_wait( &g_test_sem, 0 ); //lock
    if( err ) { slog( "(3) ssemaphore_wait() error %d\n", err ); err_cnt += err; }
    sthread_create( &th, sd, ssemaphore_test_thread, &g_test_sem, 0 );
    stime_sleep( 3000 );
    g_t1 = stime_ns();
    err = ssemaphore_release( &g_test_sem ); //unlock: the second thread (waiting) will resume...
    slog( "UNLOCK T1 (thread 1)\n" );
    if( err ) { slog( "(3) ssemaphore_release() error %d\n", err ); err_cnt += err; }
    sthread_destroy( &th, 5000 );
    ssemaphore_destroy( &g_test_sem );
    slog( "%f ms (%d)\n", (double)(g_t2-g_t1)/1000000000*1000, g_t2 - g_t1 );
    slog( "UNNAMED SEMAPHORE TEST FINISHED\n" );
    return err_cnt;
}
#endif

//
// RW Lock
//

static int rw_acquire( std::atomic_int* lock, std::atomic_int* w_wait, bool rw, int timeout )
{
    int rv = -1; //0 - successful; -1 - timeout; 
    stime_ticks_t start_t = 0;
    while( 1 )
    {
        int v = atomic_load_explicit( lock, std::memory_order_relaxed );
        if( rw == false )
        {
    	    //read:
    	    int ww = atomic_load_explicit( w_wait, std::memory_order_relaxed );
    	    if( v != 32768 && ww == 0 )
    	    {
		//if( lock == v ) { lock = v + 1; return 1; } else { v = lock; return 0; }
    		if( atomic_compare_exchange_weak_explicit( lock, &v, v + 1, std::memory_order_acq_rel, std::memory_order_relaxed ) )
    		{
    		    rv = 0;
    		    break;
    		}
    	    }
	}
	else
	{
	    //write:
	    if( v == 0 )
	    {
		//if( lock == v ) { lock = 32768; return 1; } else { v = lock; return 0; }
    		if( atomic_compare_exchange_weak_explicit( lock, &v, 32768, std::memory_order_acq_rel, std::memory_order_relaxed ) )
    		{
    		    rv = 0;
    		    break;
    		}
	    }
	    atomic_store( w_wait, 1 );
	}
        if( timeout == 0 ) break;
        if( timeout != STHREAD_TIMEOUT_INFINITE )
        {
    	    stime_ticks_t t = stime_ticks();
    	    if( start_t == 0 ) start_t = t;
    	    if( t - start_t >= (int64_t)timeout * stime_ticks_per_second() / 1000 ) break;
    	    stime_sleep( 1 );
    	}
    }
    if( rw )
    {
	//write:
	atomic_store_explicit( w_wait, 0, std::memory_order_release );
    }
    return rv;
}
static void rw_release( std::atomic_int* lock, bool rw )
{
    if( rw == false )
    {
	//read:
	atomic_fetch_sub_explicit( lock, 1, std::memory_order_acq_rel );
    }
    else
    {
	//write:
	atomic_store( lock, 0 );
    }
}

int srwlock_init( srwlock* rw, uint32_t flags )
{
    int rv = -1;

    while( 1 )
    {
	smem_clear( rw, sizeof( srwlock ) );
#ifdef NATIVE_RWLOCK
#else
	atomic_init( &rw->lock, 0 );
	atomic_init( &rw->w_wait, 0 );
	rv = 0;
#endif
	break;
    }

    return rv;
}

int srwlock_destroy( srwlock* rw )
{
    int rv = -1;

    while( 1 )
    {
#ifdef NATIVE_RWLOCK
#else
	rv = 0;
#endif
	break;
    }

    return rv;
}

int srwlock_r_lock( srwlock* rw, int timeout )
{
    int rv = -2; //0 - successful; -1 - timeout; -2 - some error

    while( 1 )
    {
#ifdef NATIVE_RWLOCK
#else
	rv = rw_acquire( &rw->lock, &rw->w_wait, 0, timeout );
#endif
	break;
    }

    return rv;
}

int srwlock_r_unlock( srwlock* rw )
{
    int rv = -1;

    while( 1 )
    {
#ifdef NATIVE_RWLOCK
#else
	rw_release( &rw->lock, 0 );
	rv = 0;
#endif
	break;
    }

    return rv;
}

int srwlock_w_lock( srwlock* rw, int timeout )
{
    int rv = -2; //0 - successful; -1 - timeout; -2 - some error

    while( 1 )
    {
#ifdef NATIVE_RWLOCK
#else
	rv = rw_acquire( &rw->lock, &rw->w_wait, 1, timeout );
#endif
	break;
    }

    return rv;
}

int srwlock_w_unlock( srwlock* rw )
{
    int rv = -1;

    while( 1 )
    {
#ifdef NATIVE_RWLOCK
#else
	rw_release( &rw->lock, 1 );
	rv = 0;
#endif
	break;
    }

    return rv;
}

#ifdef SUNDOG_TEST
srwlock g_test_rwlock;
void* srwlock_test_thread( void* user_data )
{
    int err;
    slog( "T. RWLOCK TEST THREAD\n" );
    stime_sleep( 1000 );
    slog( "T. W LOCK...\n" );
    err = srwlock_w_lock( &g_test_rwlock, STHREAD_TIMEOUT_INFINITE );
    slog( "T. W LOCK = %d\n", err );
    if( err ) slog( "T. W ERROR1 %d\n", err );
    stime_sleep( 1000 );
    err = srwlock_w_unlock( &g_test_rwlock );
    slog( "T. W UNLOCKED\n" );
    slog( "T. FINISHED\n" );
    return NULL;
}
int srwlock_test( sundog_engine* sd )
{
    int err;
    int err_cnt = 0;

    slog( "RWLOCK TEST...\n" );
    err = srwlock_init( &g_test_rwlock, 0 );
    if( err ) { slog( "srwlock_init() error %d\n", err ); err_cnt++; }

    slog( "Multiple R lock/unlock...\n" );
    err = srwlock_r_lock( &g_test_rwlock, 0 ); if( err ) { slog( "R ERROR1 %d\n", err ); err_cnt++; }
    err = srwlock_r_lock( &g_test_rwlock, 0 ); if( err ) { slog( "R ERROR2 %d\n", err ); err_cnt++; }
    err = srwlock_r_lock( &g_test_rwlock, 0 ); if( err ) { slog( "R ERROR3 %d\n", err ); err_cnt++; }
    srwlock_r_unlock( &g_test_rwlock );
    srwlock_r_unlock( &g_test_rwlock );
    srwlock_r_unlock( &g_test_rwlock );

    slog( "WR...\n" );
    err = srwlock_w_lock( &g_test_rwlock, 0 ); if( err ) { slog( "W ERROR1 %d\n", err ); err_cnt++; }
    err = srwlock_r_lock( &g_test_rwlock, 0 ); if( err ) slog( "R OK\n" ); else { slog( "R ERROR!\n" ); err_cnt++; }
    srwlock_w_unlock( &g_test_rwlock );

    slog( "RW...\n" );
    err = srwlock_r_lock( &g_test_rwlock, 0 ); if( err ) { slog( "R ERROR1 %d\n", err ); err_cnt++; }
    err = srwlock_w_lock( &g_test_rwlock, 0 ); if( err ) slog( "W OK\n" ); else { slog( "W ERROR!\n" ); err_cnt++; }
    srwlock_r_unlock( &g_test_rwlock );

    slog( "R + W in other thread...\n" );
    sthread th;
    sthread_create( &th, sd, srwlock_test_thread, &g_test_rwlock, 0 );
    for( int i = 0; i < 5 * 3; i++ )
    {
	err = srwlock_r_lock( &g_test_rwlock, 200 );
	slog( "R LOCK %d = %d\n", i, err );
	if( err == 0 )
	{
	    stime_sleep( 200 );
	    srwlock_r_unlock( &g_test_rwlock );
	}
    }
    sthread_destroy( &th, 5000 );

    srwlock_destroy( &g_test_rwlock );
    slog( "RWLOCK TEST FINISHED\n" );
    return err_cnt;
}
#endif

//
// Thread-safe lazy initialization helper
//

int sundog::lazy_init_helper::init_begin( const char* fn_name, int timeout_ms, int step_ms )
{
    //Global init once per process
    if( atomic_fetch_add( &cnt, 1 ) != 0 )
    {
        STIME_WAIT_FOR( atomic_load( &ready ), timeout_ms, step_ms, { slog( "%s singleton init timeout\n", fn_name ); return -1; } );
        return 0; //already initialized
    }
    STIME_WAIT_FOR( !atomic_load( &ready ), timeout_ms, step_ms, { slog( "%s singleton init timeout2\n", fn_name ); return -1; } );
    return 1; //ready for initialization
}

void sundog::lazy_init_helper::init_end()
{
    atomic_store( &ready, 1 );
}

int sundog::lazy_init_helper::deinit_begin()
{
    //Global deinit once per process
    if( atomic_fetch_sub( &cnt, 1 ) != 1 ) return 0;
    return 1; //ready for deinitialization
}

void sundog::lazy_init_helper::deinit_end()
{
    atomic_store( &ready, 0 );
}
