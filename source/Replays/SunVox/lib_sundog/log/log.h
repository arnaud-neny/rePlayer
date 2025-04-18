#pragma once

int slog_global_init( const char* filename );
int slog_global_deinit( void );
void slog( const char* format, ... );
void slog_disable( bool console, bool file );
void slog_enable( bool console, bool file );
const char* slog_get_file( void );
char* slog_get_latest( sundog_engine* s, size_t size );
void slog_show_error_report( sundog_engine* s );

extern bool g_slog_no_cout;
extern bool g_slog_no_fout;
