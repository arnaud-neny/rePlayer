#pragma once

#ifdef OS_UNIX
    #include <pthread.h>
    #include <semaphore.h>
#endif

#ifdef OS_APPLE
    #include <dispatch/dispatch.h>
#endif

int sthread_global_init();
int sthread_global_deinit();

#define STHREAD_TIMEOUT_INFINITE	0x7FFFFFFF

//
// Thread
//

struct sthread
{
#ifdef OS_UNIX
    pthread_t th;
    pthread_attr_t attr;
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    HANDLE th;
#endif
    sundog_engine* sd; //may be null
    void* arg;
    void* (*proc)(void*);
    volatile bool finished;
};

int sthread_create( sthread* th, sundog_engine* sd, void* (*proc)(void*), void* arg, uint32_t flags );
int sthread_destroy( sthread* th, int timeout ); //timeout (ms): >0 - timeout; <0 - don't destroy after timeout;
int sthread_clean( sthread* th );
int sthread_is_empty( sthread* th );
int sthread_is_finished( sthread* th );

typedef uint64_t sthread_tid_t;
typedef uint64_t sthread_pid_t;

sthread_tid_t sthread_gettid();
sthread_pid_t sthread_getpid();

//
// Mutex
// (recursive if SPINLOCK mode is off)
//

/*
  SMUTEX_FLAG_ATOMIC_SPINLOCK - use spinlock instead of the mutex;
  advantages:
    * works 2 or more times faster;
    * fastest awakening when exiting lock() - useful for real-time tasks such as synths;
  disadvantages:
    * no recursion (one thread can't lock a resource twice);
    * resource starvation problem with a high density of competing tasks;
      (algorithm optimization required)
*/
#define SMUTEX_FLAG_ATOMIC_SPINLOCK	( 1 << 0 )
#define SMUTEX_FLAG_INITIALIZED		( 1 << 31 )

struct smutex
{
    uint32_t flags;
#ifndef NO_BUILTIN_ATOMIC_OPS
    std::atomic_int lock;
#endif
#ifdef OS_UNIX
    pthread_mutex_t mutex;
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    CRITICAL_SECTION mutex;
#endif
};

int smutex_init( smutex* mutex, uint32_t flags ); //recursive by default
int smutex_destroy( smutex* mutex );
int smutex_lock( smutex* mutex ); //Return 0 if successful
int smutex_trylock( smutex* mutex ); // ...
int smutex_unlock( smutex* mutex );
int smutex_test( sundog_engine* sd );
/*
[*] - new tests (2023)

  lock/unlock latency (~ms) between the threads (normal ; with spinlock):
*   Linux x86_64 (Ryzen 7 3700X): 	0.06 ; 0.0004
*   Linux x86_64 (Core i5; Zenbook14X):	0.1 ; 0
*   Windows x86_64: 			0.02-0.04 ; 0.0005-0.0016
*   macOS x86_64 (Mac mini 2018):       0.03 ; 0.00003
*   iOS ARM64 (iPad Pro 2021):          0.03 ; 0.0007
    iOS ARM64 (iPad mini 2):            0.04 ; 0-3.32
*   Android ARM64 (Samsung A51): 	0.4 ; 0.01

  length (~ms) of idle lock+unlock (normal ; with spinlock):
*   Linux x86_64 (Ryzen 7 3700X):	0.00001 ; 0.000004
*   Linux x86_64 (Core i5; Zenbook14X):	0.000013 ; 0.000005
    Linux x86_64 (Core i3): 		0.000038 ; 0.000016
*   Windows x86_64: 			0.000017 ; 0.000008
*   macOS x86_64 (Mac mini 2018):       0.00003  ; 0.000007
*   iOS ARM64 (iPad Pro 2021):          0.00001  ; 0.000006
    iOS ARM64 (iPad mini 2): 		0.00015  ; 0.00005
*   Android ARM64 (Samsung A51): 	0.00004 ; 0.00001
*/

//
// Semaphore
//

struct ssemaphore
{
#ifdef OS_UNIX
    sem_t* named_sem;
#ifdef OS_APPLE
    dispatch_semaphore_t sem;
#else
    sem_t sem;
#endif
#endif
#if defined(OS_WIN) || defined(OS_WINCE)
    HANDLE sem;
#endif
};

/*
  ssemaphore_create():
    name: NULL - unnamed semaphore; "name" - named semaphore (inter-process);
    val - initial value; must be >= 0;
*/
int ssemaphore_create( ssemaphore* sem, const char* name, uint32_t val, uint32_t flags );
int ssemaphore_destroy( ssemaphore* sem );
/*
  ssemaphore_wait() - acquire (wait for positive value):
    {
      val--;
      //if val < 0, then abs(val) = number of waiting threads
      while( val < 0 ) wait();
      if( val >= 0 ) return 0; //successful
      val++; return -1; //can't acquire or timeout
    }
    timeout in ms;
    retval: 0 - successful; -1 - timeout; -2 - some error;
*/
int ssemaphore_wait( ssemaphore* sem, int timeout ); //acquire
int ssemaphore_release( ssemaphore* sem ); //val++
int ssemaphore_test( sundog_engine* sd );
/*
[*] - new tests (2023)

  lock/unlock latency (~ms) between the threads (named ; unnamed):
*   Linux x86_64 (Ryzen 7 3700X):	0.2 ; 0.02
*   Linux x86_64 (Core i5; Zenbook14X):	0.1
*   Windows x86_64:			0.18 ; 0.03
*   macOS x86_64 (Mac mini 2018):       0.04 ; 0.02
*   iOS ARM64 (iPad Pro 2021):          0.1 ; 0.09
    iOS ARM64 (iPad mini 2):            1.2 ; 0.02-0.24
*   Android ARM64 (Samsung A51): 	NOT IMPLEMENTED ; 0.3-0.4
*/

//
// RW Lock
//

/*
  Possible resource starvation problem with a high density of competing tasks
  (algorithm optimization required)
  Если плотность команд записи слишком высокая - читающий поток будет ждать свободный временной слот почти бесконечно. И наоборот.
  Текущая реализация простая и быстрая, но подходит не для всех задач. В дальнейшем стоит добавить поддержку pthread_rwlock_t и SRWLock?
*/

//Write (low frequency): exclusive - only one writer can modify the data at the same time;
//Read (high frequency): concurrent - multiple readers reading the data at the same time;
//Multiple threads can read the data in parallel but an exclusive lock is needed for writing or modifying data.

//#define NATIVE_RWLOCK

struct srwlock
{
#ifdef NATIVE_RWLOCK
#else
    std::atomic_int lock;
    std::atomic_int w_wait; //partial solution to the problem of resource starvation (for cases with frequent reading)
#endif
};

int srwlock_init( srwlock* rw, uint32_t flags );
int srwlock_destroy( srwlock* rw );
int srwlock_r_lock( srwlock* rw, int timeout ); //timeout in ms; retval: 0 - successful; -1 - timeout; -2 - some error
int srwlock_r_unlock( srwlock* rw );
int srwlock_w_lock( srwlock* rw, int timeout );
int srwlock_w_unlock( srwlock* rw );
int srwlock_test( sundog_engine* sd );

//
// Thread-safe lazy initialization helper
//

namespace sundog
{

class lazy_init_helper
{
    std::atomic_int cnt; //number of users; 0->1: init; 1->0: deinit;
    std::atomic_int ready;
public:
    //init_begin() retval:
    // 0 - already initialized;
    // -1 - timeout;
    // 1 - ready for initialization;
    int init_begin( const char* fn_name, int timeout_ms, int step_ms );
    void init_end();
    //deinit_begin() retval:
    // 0 - do nothing;
    // 1 - ready for deinitialization;
    int deinit_begin();
    void deinit_end();
};

} //namespace sundog
