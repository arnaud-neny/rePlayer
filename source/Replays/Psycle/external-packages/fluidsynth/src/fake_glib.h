#pragma once
#include <malloc.h>
#include <windows.h>

typedef char   gchar;
typedef short  gshort;
typedef long   glong;
typedef int    gint;
typedef gint   gboolean;

typedef unsigned char   guchar;
typedef unsigned short  gushort;
typedef unsigned long   gulong;
typedef unsigned int    guint;
typedef guint	gsize;

typedef float   gfloat;
typedef double  gdouble;

typedef void* gpointer;
typedef const void *gconstpointer;

typedef guchar guint8;
typedef gint gint32;
typedef guint guint32;

typedef gpointer (*GThreadFunc) (gpointer data);

/* Provide definitions for some commonly used macros.
 *  Some of them are only provided if they haven't already
 *  been defined. It is assumed that if they are already
 *  defined then the current definition is correct.
 */
#ifndef	FALSE
#define	FALSE	(0)
#endif

#ifndef	TRUE
#define	TRUE	(!FALSE)
#endif

#define GPOINTER_TO_INT(p)	 ((gint) (glong)  (p))
#define GINT_TO_POINTER(i)   ((gpointer) (glong) (i))
#define GPOINTER_TO_UINT(p)  ((guint)  (glong) (p))
#define GUINT_TO_POINTER (i) ((gpointer) (glong) (i))


#define g_return_val_if_fail(cond, val) if (!(cond)) {return val;}
#define g_return_if_fail(cond) if (!(cond)) {return;}

/* Provide simple macro statement wrappers (adapted from Perl):
 *  G_STMT_START { statements; } G_STMT_END;
 *  can be used as a single statement, as in
 *  if (x) G_STMT_START { ... } G_STMT_END; else ...
 *
 *  For gcc we will wrap the statements within `({' and `})' braces.
 *  For SunOS they will be wrapped within `if (1)' and `else (void) 0',
 *  and otherwise within `do' and `while (0)'.
 */
#if !(defined (G_STMT_START) && defined (G_STMT_END))
#  if defined (__GNUC__) && !defined (__STRICT_ANSI__) && !defined (__cplusplus)
#    define G_STMT_START	(void)(
#    define G_STMT_END		)
#  else
#    if (defined (sun) || defined (__sun__))
#      define G_STMT_START	if (1)
#      define G_STMT_END	else (void)0
#    else
#      define G_STMT_START	do
#      define G_STMT_END	while (0)
#    endif
#  endif
#endif


/* Basic bit swapping functions
 */
#define G_BIG_ENDIAN 1
#define G_BYTE_ORDER G_BIG_ENDIAN

#define GUINT16_SWAP_LE_BE_CONSTANT(val)	((unsigned short) ( \
    (((unsigned short) (val) & (unsigned short) 0x00ffU) << 8) | \
    (((unsigned short) (val) & (unsigned short) 0xff00U) >> 8)))
#define GUINT32_SWAP_LE_BE_CONSTANT(val)	((unsigned int) ( \
    (((unsigned int) (val) & (unsigned int) 0x000000ffU) << 24) | \
    (((unsigned int) (val) & (unsigned int) 0x0000ff00U) <<  8) | \
    (((unsigned int) (val) & (unsigned int) 0x00ff0000U) >>  8) | \
    (((unsigned int) (val) & (unsigned int) 0xff000000U) >> 24)))

#define GUINT16_SWAP_LE_BE(val) (GUINT16_SWAP_LE_BE_CONSTANT (val))
#define GUINT32_SWAP_LE_BE(val) (GUINT32_SWAP_LE_BE_CONSTANT (val))

#define GINT16_TO_LE(val)	((signed short) (val))
#define GUINT16_TO_LE(val)	((unsigned short) (val))
#define GINT16_TO_BE(val)	((signed short) GUINT16_SWAP_LE_BE (val))
#define GUINT16_TO_BE(val)	(GUINT16_SWAP_LE_BE (val))
#define GINT32_TO_LE(val)	((signed int) (val))
#define GUINT32_TO_LE(val)	((unsigned int) (val))
#define GINT32_TO_BE(val)	((signed int) GUINT32_SWAP_LE_BE (val))
#define GUINT32_TO_BE(val)	(GUINT32_SWAP_LE_BE (val))

/* The G*_TO_?E() macros are defined in glibconfig.h.
 * The transformation is symmetric, so the FROM just maps to the TO.
 */
#define GINT16_FROM_LE(val)	(GINT16_TO_LE (val))
#define GUINT16_FROM_LE(val)	(GUINT16_TO_LE (val))
#define GINT16_FROM_BE(val)	(GINT16_TO_BE (val))
#define GUINT16_FROM_BE(val)	(GUINT16_TO_BE (val))
#define GINT32_FROM_LE(val)	(GINT32_TO_LE (val))
#define GUINT32_FROM_LE(val)	(GUINT32_TO_LE (val))
#define GINT32_FROM_BE(val)	(GINT32_TO_BE (val))
#define GUINT32_FROM_BE(val)	(GUINT32_TO_BE (val))

typedef enum
{
  G_THREAD_PRIORITY_LOW,
  G_THREAD_PRIORITY_NORMAL,
  G_THREAD_PRIORITY_HIGH,
  G_THREAD_PRIORITY_URGENT
} GThreadPriority;

typedef struct  _GThread {
  GThreadFunc func;
  gpointer data;
  gboolean joinable;
  GThreadPriority priority;
}GThread;

typedef struct _GMutex {
	int i;
}GMutex;
typedef struct _GCond {
	int i;
}GCond;
typedef struct _GStaticPrivate {
  gpointer var;
}GStaticPrivate;
typedef struct _GStaticMutex {
	int i;
}GStaticMutex;
typedef struct _GStaticRecMutex {
	int i;
}GStaticRecMutex;

typedef struct _GTimeVal {
  glong tv_sec;
  glong tv_usec;
}GTimeVal;
typedef struct _GError {
  gint         code;
  gchar       *message;
}GError;
typedef struct _GDestroyNotify {
	int i;
} GDestroyNotify;

typedef struct _GThreadFunctions {
	int i;
}GThreadFunctions;

#define g_thread_supported()    (g_threads_got_initialized)
extern gboolean               g_threads_got_initialized;


#define g_alloca(size)		 alloca (size)
#define g_newa(struct_type, n_structs)	((struct_type*) g_alloca (sizeof (struct_type) * (gsize) (n_structs)))

#if defined __linux__
	#define ALIGNED_MEMORY_ALLOC(alignment, type, val, amount) { \
		void * address; \
		if(!posix_memalign(&address, alignment, amount * sizeof(type))) val = (type*)(address); }
#elif defined _WIN32 && defined _MSC_VER
	#define ALIGNED_MEMORY_ALLOC(alignment, type, val, amount) \
		val = (type*)(_aligned_malloc(amount * sizeof(type), alignment));
#else
	#define ALIGNED_MEMORY_ALLOC(alignment, type, val, amount) \
		val = new type[amount];
#endif

#if defined __linux__
	#define ALIGNED_MEMORY_DEALLOC(type, address) std::free(address);
#elif defined _WIN32 && defined _MSC_VER
	#define ALIGNED_MEMORY_DEALLOC(type, address) _aligned_free(address);
#else
	#define ALIGNED_MEMORY_DEALLOC(type, address) delete[] address;
#endif



void g_usleep (gulong  microseconds);

void g_get_current_time(GTimeVal* timeval);
void g_clear_error();

gint     g_atomic_int_exchange_and_add         (volatile gint *atomic, gint val);
void     g_atomic_int_add                      (volatile gint *atomic,gint val);
gboolean g_atomic_int_compare_and_exchange     (volatile gint *atomic,gint oldval,gint  newval);
gboolean g_atomic_pointer_compare_and_exchange (volatile gpointer *atomic, gpointer  oldval, gpointer  newval);

# define g_atomic_int_get(atomic) 		((gint)*(atomic))
# define g_atomic_int_set(atomic, newval) 	((void) (*(atomic) = (newval)))
# define g_atomic_pointer_get(atomic) 		((gpointer)*(atomic))
# define g_atomic_pointer_set(atomic, newval)	((void) (*(atomic) = (newval)))

#define g_atomic_int_inc(atomic) (g_atomic_int_add ((atomic), 1))
#define g_atomic_int_dec_and_test(atomic)				\
  (g_atomic_int_exchange_and_add ((atomic), -1) == 1)


void     g_static_private_init           (GStaticPrivate   *private_key);
gpointer g_static_private_get            (GStaticPrivate   *private_key);
void     g_static_private_set            (GStaticPrivate   *private_key,
					  gpointer          data,
					  GDestroyNotify*    notify);
void     g_static_private_free           (GStaticPrivate   *private_key);



GMutex* g_mutex_new ();
void g_mutex_free(GMutex* a);
void g_mutex_lock(GMutex* a);
void g_mutex_unlock(GMutex* a);

void g_static_mutex_init(GStaticMutex* a);
void g_static_mutex_free(GStaticMutex* a);
void g_static_mutex_lock(GStaticMutex* a);
void g_static_mutex_unlock(GStaticMutex* a);

void g_static_rec_mutex_init(GStaticRecMutex* a);
void g_static_rec_mutex_free(GStaticRecMutex* a);
void g_static_rec_mutex_lock(GStaticRecMutex* a);
void g_static_rec_mutex_unlock(GStaticRecMutex* a);


GCond* g_cond_new();
void g_cond_free(GCond* a);
void g_cond_signal(GCond* a);
void g_cond_broadcast(GCond* a);
void g_cond_wait(GCond* a, GMutex* b);



void    g_thread_init   (GThreadFunctions       *vtable);

#define g_thread_create(func, data, joinable, error)			\
  (g_thread_create_full (func, data, 0, joinable, FALSE, 		\
                         G_THREAD_PRIORITY_NORMAL, error))

GThread* g_thread_create_full  (GThreadFunc            func,
                                gpointer               data,
                                gulong                 stack_size,
                                gboolean               joinable,
                                gboolean               bound,
                                GThreadPriority        priority,
                                GError               **error);
GThread* g_thread_self         (void);
void     g_thread_exit         (gpointer               retval);
gpointer g_thread_join         (GThread               *thread);

