extern "C" {
#include "fake_glib.h"

gboolean               g_threads_got_initialized = FALSE;

void    g_thread_init   (GThreadFunctions       *vtable){g_threads_got_initialized= TRUE;}

GThread* g_thread_create_full  (GThreadFunc            func,
                                gpointer               data,
                                gulong                 stack_size,
                                gboolean               joinable,
                                gboolean               bound,
                                GThreadPriority        priority,
								GError               **error){return 0;}
GThread* g_thread_self         (void){return 0;}
void     g_thread_exit         (gpointer               retval){}
gpointer g_thread_join         (GThread               *thread){return 0;}




gint     g_atomic_int_exchange_and_add         (volatile gint *atomic, gint val){
	gint prev = *atomic;
	*atomic += val; return prev;
}
void     g_atomic_int_add                      (volatile gint *atomic,gint val){
	*atomic += val;
}
gboolean g_atomic_int_compare_and_exchange     (volatile gint *atomic,gint oldval,gint  newval) {
	if(*atomic == oldval) { *atomic = newval; return TRUE; }
	return FALSE;
}
gboolean g_atomic_pointer_compare_and_exchange (volatile gpointer *atomic, gpointer  oldval, gpointer  newval){
	if(*atomic == oldval) { *atomic = newval; return TRUE; }
	return FALSE;
}


void     g_static_private_init           (GStaticPrivate   *private_key) { private_key->var = 0; }
gpointer g_static_private_get            (GStaticPrivate   *private_key){ return private_key->var; }
void     g_static_private_set            (GStaticPrivate   *private_key,
					  gpointer          data,
					  GDestroyNotify*    notify){ private_key->var = data;}
void     g_static_private_free           (GStaticPrivate   *private_key){}


GMutex* g_mutex_new (){ return 0;}
void g_mutex_free(GMutex* a){}
void g_mutex_lock(GMutex* a){}
void g_mutex_unlock(GMutex* a){}

void g_static_mutex_init(GStaticMutex* _m){}
void g_static_mutex_free(GStaticMutex* _m){}
void g_static_mutex_lock(GStaticMutex* _m){}
void g_static_mutex_unlock(GStaticMutex* _m){}

void g_static_rec_mutex_init(GStaticRecMutex* a){}
void g_static_rec_mutex_free(GStaticRecMutex* a){}
void g_static_rec_mutex_lock(GStaticRecMutex* a){}
void g_static_rec_mutex_unlock(GStaticRecMutex* a){}


GCond* g_cond_new(){return 0;}
void g_cond_free(GCond* a){}
void g_cond_signal(GCond* a){}
void g_cond_broadcast(GCond* a){}
void g_cond_wait(GCond* a, GMutex* b){}


void g_usleep (gulong       microseconds)  {
	Sleep(microseconds*0.001);
}
void g_get_current_time(GTimeVal* timeval){
	long ticks = GetTickCount();
	timeval->tv_sec = (glong)(ticks*0.001);
	timeval->tv_usec = ticks*1000;
}
void g_clear_error(){}
}