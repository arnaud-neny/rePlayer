#pragma once

typedef uint32_t stime_ticks_t; //Current system time in hires ticks
typedef uint32_t stime_ms_t; //Current system time in milliseconds
typedef uint64_t stime_ns_t; //Current system time in nanoseconds

#if defined(OS_WIN) || defined(OS_WINCE)
    stime_ticks_t stime_ticks_per_second();
#else
    inline stime_ticks_t stime_ticks_per_second() { return 50000; } //23 hours max
#endif

#if defined(OS_APPLE)
    stime_ticks_t convert_mach_absolute_time_to_sundog_ticks( uint64_t mt, int tick_type ); //0 - lowres (1000 TPS); 1 - highres;
    uint64_t convert_sundog_ticks_to_mach_absolute_time( stime_ticks_t t, int tick_type ); //0 - lowres (1000 TPS); 1 - highres;
#endif

int stime_global_init();
int stime_global_deinit();
stime_ticks_t stime_ticks(); //Current system time in hires ticks
stime_ms_t stime_ms(); //Current system time in milliseconds
stime_ns_t stime_ns(); //Current system time in nanoseconds
int stime_test( sundog_engine* sd );

//The following functions can be used outside the SunDog engine:

int64_t stime_time(); //Current calendar time in seconds (since Jan 1, 1970 UTC)
uint32_t stime_year();
uint32_t stime_month(); //from 1
const char* stime_month_string();
uint32_t stime_day(); //from 1
uint32_t stime_hours(); //0-23
uint32_t stime_minutes(); //0-59
uint32_t stime_seconds(); //0-59, 0-60 or 0-61 (the extra range is to accommodate for leap seconds in certain systems)
void stime_sleep( int milliseconds );
#define STIME_WAIT_FOR( STOPCOND, TIMEOUT_MS, STEP_MS, TIMEOUT_CODE ) \
    { \
	int t = 0; \
        while( !(STOPCOND) ) \
        { \
    	    stime_sleep( STEP_MS ); \
    	    t += STEP_MS; \
    	    if( t > TIMEOUT_MS ) \
    	    { \
    		TIMEOUT_CODE; \
    		break; \
    	    } \
    	} \
    }
