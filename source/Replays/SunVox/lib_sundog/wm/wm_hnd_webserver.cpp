/*
    wm_hnd_webserver.cpp - Web Server with File Browser
    This file is part of the SunDog engine.
    Copyright (C) 2009 - 2024 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

//Modularity: not yet (remove global variables!)

#include "sundog.h"
#include "net/net.h"

#ifdef SUNDOG_NET

bool g_webserver_available = 1;

#ifdef OS_ANDROID
    #include "main/android/sundog_bridge.h"
#endif

#include <sys/socket.h> //socket definitions
#include <sys/types.h> //socket types
#include <sys/stat.h>
#include <arpa/inet.h> //inet (3) funtions
#include <unistd.h> //misc. UNIX functions
#include <dirent.h> //working with directories
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>

#define MAX_REQ_LINE 2048

const char* g_webserver_name = "SunDog webserver " __DATE__;

char g_webserver_root[ MAX_DIR_LEN ] = { '.', 0 };
int g_webserver_port = 8080;
pthread_t g_webserver_pth;
const char* g_webserver_status = NULL;
volatile int g_webserver_finished = 0;
volatile int g_webserver_exit_request = 0;
volatile int g_webserver_pth_work = 0;
volatile bool g_webserver_window_closed = false;
WINDOWPTR g_webserver = NULL;

enum req_method { GET, HEAD, POST, UNSUPPORTED };
enum res_type	{ RES_FILE, RES_DIR };

struct req_info 
{
    enum req_method method;
    char* referer;
    char* useragent;
    char* resource;
    char* resource_name; //without first part of the address
    char* resource_parameters;
    int8_t res_type;
    int post_size;
    char* post_boundary;
    int root;
    int status;
};

enum
{
    STR_WEBSERV_IMPORT_EXPORT,
    STR_WEBSERV_ERROR_CONN,
    STR_WEBSERV_STATUS,
    STR_WEBSERV_STATUS_STARTING,
    STR_WEBSERV_STATUS_WORKING,
    STR_WEBSERV_HEADER,
    STR_WEBSERV_TITLE,
    STR_WEBSERV_REMOVE,
    STR_WEBSERV_REMOVE2,
    STR_WEBSERV_YES,
    STR_WEBSERV_NO,
    STR_WEBSERV_HOWTO,
    STR_WEBSERV_SEND,
    STR_WEBSERV_CREATE_DIR,
    STR_WEBSERV_HOME,
    STR_WEBSERV_PARENT,
    STR_WEBSERV_MSG,
    STR_WEBSERV_OTHER_ADDR,
    STR_WEBSERV_FNAME,
    STR_WEBSERV_FSIZE,
    STR_WEBSERV_FMTIME,
    STR_WEBSERV_LOCALHOST_DESCR
};

#define HTMLBEGIN "<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"

static const char* webserv_get_string( int str_id )
{
    const char* str = 0;
    const char* lang = slocale_get_lang();
    while( 1 )
    {
        if( smem_strstr( lang, "ru_" ) )
        {
            switch( str_id )
            {
		case STR_WEBSERV_IMPORT_EXPORT: str = "Wi-Fi Экспорт/Импорт"; break;
		case STR_WEBSERV_ERROR_CONN: str = "ОШИБКА:\nнет подключения к сети"; break;
		case STR_WEBSERV_STATUS: str = "Статус:"; break;
		case STR_WEBSERV_STATUS_STARTING: str = "Запуск вебсервера ..."; break;
		case STR_WEBSERV_STATUS_WORKING: str = "Активен ..."; break;
		case STR_WEBSERV_HEADER: str = HTMLBEGIN "<title>Хранилище файлов приложения</title></head><body>\r\n"; break;
    		case STR_WEBSERV_TITLE: str = "Хранилище файлов приложения: "; break;
    		case STR_WEBSERV_REMOVE: str = "<center><h1>Удалить \""; break;
		case STR_WEBSERV_REMOVE2: str = "\"><font color=\"red\">(удалить)</font></a>"; break;
		case STR_WEBSERV_YES: str = "\"><font color=\"red\">ДА</font></a> "; break;
		case STR_WEBSERV_NO: str = "\"><font color=\"red\">НЕТ</font></a>"; break;
		case STR_WEBSERV_HOWTO: str = "<p>Укажите один или несколько файлов, которые необходимо загрузить в хранилище:<br>\r\n"; break;
		case STR_WEBSERV_SEND: str = "<input type=\"submit\" value=\"Загрузить\">\r\n"; break;
		case STR_WEBSERV_CREATE_DIR: str = "<input type=\"submit\" value=\"Создать папку\">\r\n"; break;
		case STR_WEBSERV_HOME: str = "<a href=\"/\"><b>.. [корневой каталог (основная папка)]</b></a>"; break;
		case STR_WEBSERV_PARENT: str = "<a href=\"../\"><b>.. [предыдущая папка]</b></a>"; break;
		case STR_WEBSERV_MSG: str = "Для передачи файлов откройте веб-браузер на другом компьютере. Компьютер должен быть в одной Wi-Fi сети с вашим устройством.\nВ браузере введите следующий адрес:"; break;
		case STR_WEBSERV_OTHER_ADDR: str = "Другие возможные адреса:"; break;
		case STR_WEBSERV_FNAME: str = "Имя"; break;
		case STR_WEBSERV_FSIZE: str = "Размер"; break;
		case STR_WEBSERV_FMTIME: str = "Изменен"; break;
		case STR_WEBSERV_LOCALHOST_DESCR: str = "в браузере на этом устройстве"; break;
		default: break;
            }
            if( str ) break;
        }
        //Default:
        switch( str_id )
        {
	    case STR_WEBSERV_IMPORT_EXPORT: str = "Wi-Fi Export/Import"; break;
	    case STR_WEBSERV_ERROR_CONN: str = "ERROR:\nno network connection detected"; break;
	    case STR_WEBSERV_STATUS: str = "Status:"; break;
	    case STR_WEBSERV_STATUS_STARTING: str = "Starting webserver ..."; break;
	    case STR_WEBSERV_STATUS_WORKING: str = "Working ..."; break;
    	    case STR_WEBSERV_HEADER: str = HTMLBEGIN "<title>App File Storage</title></head><body>\r\n"; break;
    	    case STR_WEBSERV_TITLE: str = "App File Storage: "; break;
	    case STR_WEBSERV_REMOVE: str = "<center><h1>Remove \""; break;
	    case STR_WEBSERV_REMOVE2: str = "\"><font color=\"red\">(remove)</font></a>"; break;
	    case STR_WEBSERV_YES: str = "\"><font color=\"red\">YES</font></a> "; break;
	    case STR_WEBSERV_NO: str = "\"><font color=\"red\">NO</font></a>"; break;
	    case STR_WEBSERV_HOWTO: str = "<p>Please specify a file, or a set of files for upload on your device:<br>\r\n"; break;
	    case STR_WEBSERV_SEND: str = "<input type=\"submit\" value=\"Send\">\r\n"; break;
	    case STR_WEBSERV_CREATE_DIR: str = "<input type=\"submit\" value=\"Create directory\">\r\n"; break;
	    case STR_WEBSERV_HOME: str = "<a href=\"/\"><b>.. [home]</b></a>"; break;
	    case STR_WEBSERV_PARENT: str = "<a href=\"../\"><b>.. [parent directory]</b></a>"; break;
	    case STR_WEBSERV_MSG: str = "To transfer the files, launch a web browser on your computer. It must be on the same Wi-Fi network as this device.\nThen enter the following address:"; break;
	    case STR_WEBSERV_OTHER_ADDR: str = "Other possible addresses:"; break;
	    case STR_WEBSERV_FNAME: str = "Name"; break;
	    case STR_WEBSERV_FSIZE: str = "Size"; break;
	    case STR_WEBSERV_FMTIME: str = "Mod.time"; break;
    	    case STR_WEBSERV_LOCALHOST_DESCR: str = "in browser on this device"; break;
	    default: break;
        }
        break;
    }
    return str;
}

static int wait_socket_read( int s, int timeout_sec )
{
    struct timeval tv;
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;
    fd_set fds;
    FD_ZERO( &fds );
    FD_SET( s, &fds );
    return( select( s + 1, &fds, NULL, NULL, &tv ) );
}

//-----------------------
//Buffered sockets. Begin
//-----------------------

#define BS_BUF_SIZE (4096)

struct bs_data
{
    int s; //socket
    uint8_t buf[ BS_BUF_SIZE ]; //buffer
    int64_t buf_size;
    int buf_p;
    int p; //global ptr
    int size; //global size
    int eof;
    int err;
};

static bs_data* bs_open( int s, int size )
{
    bs_data* bs = (bs_data*)smem_new( sizeof( bs_data ) );
    smem_clear( bs, sizeof( bs_data ) );
    bs->s = s;
    bs->size = size;
    return bs;
}

static void bs_close( bs_data* bs )
{
    if( bs )
	smem_free( bs );
}

static int64_t bs_next_buf( bs_data* bs )
{
    int size = BS_BUF_SIZE;
    if( bs->p + size > bs->size )
	size = bs->size - bs->p;
    bs->buf_p = 0;
    bs->buf_size = read( bs->s, bs->buf, size );
    if( bs->buf_size <= 0 )
	bs->err = 1;
    return bs->buf_size;
}

static int bs_getc( bs_data* bs )
{
    if( bs->p >= bs->size )
    {
	bs->eof = 1;
	return -1;
    }
    if( bs->buf_p >= bs->buf_size )
    {
	int64_t res = bs_next_buf( bs );
	if( res <= 0 )
	{
	    return -1;
	}
    }
    int rv = bs->buf[ bs->buf_p ];
    bs->buf_p++;
    bs->p++;
    return rv;
}

//Read a line from a buffered socket
static size_t bs_read_line( bs_data* bs, void* vptr, size_t maxlen ) 
{
    size_t n;
    int c;
    uint8_t* buffer;

    buffer = (uint8_t*)vptr;

    for( n = 1; n < maxlen; n++ ) 
    {
	c = bs_getc( bs );
	if( c >= 0 ) 
	{
	    *buffer++ = (uint8_t)c;
	    if( c == '\n' )
		break;
	}
	else
	{
	    g_webserver_status = "ERROR in bs_read_line()";
	    slog( "%s\n", g_webserver_status );
	    n = 0;
	    break;
	}
    }

    *buffer = 0;
    return n;
}

//-----------------------
//Buffered sockets. End
//-----------------------

//Read a line from a socket
static size_t read_line( int sockd, void* vptr, size_t maxlen ) 
{
    size_t n, rc;
    char c, *buffer;

    buffer = (char*)vptr;

    for( n = 1; n < maxlen; n++ ) 
    {
	if( ( rc = read( sockd, &c, 1 ) ) == 1 ) 
	{
	    *buffer++ = c;
	    if( c == '\n' )
		break;
	}
	else if( rc == 0 ) 
	{
	    if( n == 1 )
		return 0;
	    else
		break;
	}
	else
	{
	    if( errno == EINTR )
		continue;
	    g_webserver_status = "ERROR in read_line()";
	    slog( "%s\n", g_webserver_status );
	    break;
	}
    }

    *buffer = 0;
    return n;
}

//Write a line to a socket
static ssize_t write_line( int sockd, const void* vptr, size_t n ) 
{
    size_t nleft;
    ssize_t nwritten;
    const char* buffer;

    buffer = (const char*)vptr;
    nleft = n;
    if( nleft == 0 ) nleft = strlen( (const char*)vptr );

    while( nleft > 0 ) 
    {
	if( ( nwritten = write( sockd, buffer, nleft ) ) <= 0 ) 
	{
	    if( errno == EINTR )
		nwritten = 0;
	    else
	    {
		g_webserver_status = "ERROR in write_line()";
		slog( "%s\n", g_webserver_status );
		break;
	    }
	}
	nleft -= nwritten;
	buffer += nwritten;
    }

    return n;
}

//Removes trailing whitespace from a string
static int trim( char* buffer ) 
{
    int64_t n = strlen( buffer ) - 1;

    //while( !isalnum( buffer[ n ] ) && n >= 0 )
    while( n >= 0 && (uint8_t)buffer[ n ] <= 0x20 )
	buffer[ n-- ] = '\0';

    return 0;
}

//Cleans up url-encoded string
static void clean_URL( char* buffer ) 
{
    char asciinum[ 3 ] = { 0 };
    int i = 0, c;
    
    while( buffer[ i ] ) 
    {
	if( buffer[ i ] == '+' )
	    buffer[ i ] = ' ';
	else if( buffer[ i ] == '%' ) 
	{
	    asciinum[ 0 ] = buffer[ i + 1 ];
	    asciinum[ 1 ] = buffer[ i + 2 ];
	    buffer[ i ] = strtol( asciinum, NULL, 16 );
	    c = i + 1;
	    do {
		buffer[ c ] = buffer[ c + 2 ];
	    } while( buffer[ 2 + ( c++ ) ] );
	}
	++i;
    }
}

//Initialises a request information structure
static void init_req_info( req_info* reqinfo ) 
{
    reqinfo->useragent = NULL;
    reqinfo->referer = NULL;
    reqinfo->resource = NULL;
    reqinfo->resource_name = NULL;
    reqinfo->resource_parameters = NULL;
    reqinfo->res_type = RES_FILE;
    reqinfo->post_size = 0;
    reqinfo->post_boundary = 0;
    reqinfo->method = UNSUPPORTED;
    reqinfo->root = 0;
    reqinfo->status = 200;
}

//Frees memory allocated for a request information structure
static void free_req_info( req_info* reqinfo ) 
{
    smem_free( reqinfo->useragent );
    smem_free( reqinfo->referer );
    smem_free( reqinfo->resource );
    smem_free( reqinfo->resource_name );
    smem_free( reqinfo->resource_parameters );
    smem_free( reqinfo->post_boundary );
}

static int parse_HTTP_header( int line_num, char* buffer, req_info* reqinfo ) 
{
    char* endptr;
    int64_t len;

    if( line_num == 0 ) 
    {
	//Get the request method, which is case-sensitive. This
	//version of the server only supports the GET and HEAD
	//request methods.

	if( !strncmp( buffer, "GET ", 4 ) ) 
	{
	    reqinfo->method = GET;
	    buffer += 4;
	}
	else if( !strncmp( buffer, "HEAD ", 5 ) ) 
	{
	    reqinfo->method = HEAD;
	    buffer += 5;
	}
	else if( !strncmp( buffer, "POST ", 5 ) ) 
	{
	    reqinfo->method = POST;
	    buffer += 5;
	}
	else 
	{
	    reqinfo->method = UNSUPPORTED;
	    reqinfo->status = 501;
	    return -1;
	}

	//Skip to start of resource:
	while( *buffer && isspace( *buffer ) )
	    buffer++;

	//Get resource name...
	endptr = strchr( buffer, ' ' );
	if( endptr != NULL )
	{
	    *endptr = 0;
	}
	clean_URL( buffer );
	{
	    //check for parameters in the filename:
	    int p = 0;
	    while( 1 )
	    {
		if( buffer[ p ] == '?' )
		{
		    buffer[ p ] = 0;
		    int64_t pars_len = strlen( &buffer[ p + 1 ] );
		    if( pars_len > 0 )
		    {
			reqinfo->resource_parameters = smem_strdup( &buffer[ p + 1 ] );
			break;
		    }
		}
		if( buffer[ p ] == 0 ) break;
		p++;
	    }
	}
	len = strlen( buffer );
	if( len == 1 && buffer[ 0 ] == '/' )
	    reqinfo->root = 1;

	//...and store it in the request information structure:
	int64_t root_len = strlen( g_webserver_root );
	reqinfo->resource = (char*)smem_new( root_len + len + 1 );
	reqinfo->resource_name = (char*)smem_new( len + 1 );
	memcpy( reqinfo->resource, g_webserver_root, root_len + 1 );
	strcat( reqinfo->resource, buffer );
	memcpy( reqinfo->resource_name, buffer, len + 1 );
    }
    else
    {
	if( !strncmp( buffer, "Content-Length: ", 16 ) ) 
	{
	    buffer += 16;
	    reqinfo->post_size = string_to_int( buffer );
	}
	else if( !strncmp( buffer, "Content-Type: ", 14 ) ) 
	{
	    buffer += 14;
	    //Get boundary...
	    char* bp = strstr( buffer, "boundary=" );
	    if( bp != NULL )
	    {
		bp += 9;
		char* bp2 = strchr( bp, ' ' );
		if( bp2 != NULL )
		{
		    *bp2 = 0;
		}
		len = strlen( bp );
		reqinfo->post_boundary = (char*)smem_new( len + 1 );
		memcpy( reqinfo->post_boundary, bp, len + 1 );
	    }
	}
    }

    return 0;
}

/*
    Gets request headers. A CRLF terminates a HTTP header line,
    but if one is never sent we would wait forever. Therefore,
    we use select() to set a maximum length of time we will
    wait for the next complete header. If we timeout before
    this is received, we terminate the connection.
*/
static int get_request( int conn, req_info* reqinfo ) 
{
    char* buffer = (char*)smem_new( MAX_REQ_LINE );
    if( buffer == 0 ) return -3;
    buffer[ 0 ] = 0;
    int rv = 0;

    int line_num = 0;
    while( 1 )
    {
	//Wait until the timeout to see if input is ready:
	int rval = wait_socket_read( conn, 5 );

	//Take appropriate action based on return from select():
	if( rval < 0 ) 
	{
	    g_webserver_status = "ERROR: select() failed in get_request()";
	    slog( "%s\n", g_webserver_status );
	    rv = -2;
	    break;
	}
	else
	{
	    if( rval == 0 ) 
	    {
		//Input not ready after timeout:
		rv = -1;
		break;
	    }
	    else 
	    {
		//We have an input line waiting, so retrieve it:
		read_line( conn, buffer, MAX_REQ_LINE - 1 );
		trim( buffer );

		if( buffer[ 0 ] == '\0' )
		    break;

		if( parse_HTTP_header( line_num, buffer, reqinfo ) )
		    break;

		line_num++;
	    }
	}
    }
    
    smem_free( buffer );
    
    return rv;
}

//Returns a resource
static int return_resource( int conn, int resource, req_info* reqinfo ) 
{
    int rv = 0;
    void* buf = smem_new( 32000 );
    if( buf == 0 ) return -4;

    while( 1 )
    {
	int64_t size = read( resource, buf, 32000 );
	if( size < 0 )
	{
	    g_webserver_status = "ERROR reading from file";
	    slog( "%s\n", g_webserver_status );
	    rv = -1;
	    break;
	}
	if( size <= 0 ) break;
	if( write( conn, buf, size ) < size )
	{
	    g_webserver_status = "ERROR sending file";
	    slog( "%s\n", g_webserver_status );
	    rv = -3;
	    break;
	}
    }
    
    smem_free( buf );
    return rv;
}

//Returns an error message
static int return_error_msg( int conn, req_info* reqinfo ) 
{
    char buffer[ 256 ];

    sprintf( 
	buffer, 
	"<HTML>\n<HEAD>\n<TITLE>Server Error %d</TITLE>\n"
	"</HEAD>\n\n", reqinfo->status 
    );
    write_line( conn, buffer, 0 );

    sprintf(
	buffer,
	"<BODY>\n<H1>Server Error %d</H1>\n", 
	reqinfo->status
    );
    write_line( conn, buffer, 0 );

    sprintf( 
	buffer, 
	"<P>The request could not be completed.</P>\n"
        "</BODY>\n</HTML>\n"
    );
    write_line( conn, buffer, 0 );

    slog( "Server error %d: the request could not be completed\n", reqinfo->status );

    return 0;
}

//Outputs HTTP response headers
static int output_HTTP_headers( int conn, req_info* reqinfo, size_t res_size ) 
{
    char ts[ 256 ];

    sprintf( ts, "HTTP/1.0 %d OK\r\n", reqinfo->status );
    write_line( conn, ts, 0 );

    sprintf( ts, "Server: %s\r\n", g_webserver_name );
    write_line( conn, ts, 0 );
    int html = 1;
    int64_t len = strlen( reqinfo->resource );
    if( len > 5 &&
	reqinfo->resource[ len - 1 ] == 'm' &&
	reqinfo->resource[ len - 2 ] == 't' &&
	reqinfo->resource[ len - 3 ] == 'h' )
    {
	html = 1;
    }
    else
    {
	if( reqinfo->res_type == RES_FILE )
	    html = 0;
    }
    if( html )
	write_line( conn, "Content-Type: text/html; charset=utf-8\r\n", 0 );
    else
    {
	write_line( conn, "Content-Type: application/octet-stream\r\n", 0 );
	sprintf( ts, "Content-Length: %lld\r\n", (unsigned long long)res_size );
	write_line( conn, ts, 0 );
    }
    write_line( conn, "\r\n", 0 );

    return 0;
}

//Service an HTTP request
static int service_request( int conn ) 
{
    int rv = 0;
    req_info reqinfo;
    int res = -1;
    size_t res_size = 0;
    DIR* res_dir = 0;

    init_req_info( &reqinfo );
    
    //Get HTTP request:
    if( get_request( conn, &reqinfo ) < 0 )
    {
	rv = -1;
	goto service_end;
    }

    printf( "http req %d %s\n", (int)reqinfo.method, reqinfo.resource_name );

    if( reqinfo.method == POST )
    {
	bs_data* bs;
	bs = bs_open( conn, reqinfo.post_size );
	if( bs )
	{
	    //0 - data;
	    //1 - boundary.
	    int status = 0;
	    
	    int64_t bound_size = smem_strlen( reqinfo.post_boundary ) + 4;
	    uint8_t* bound = (uint8_t*)smem_new( bound_size );
	    bound[ 0 ] = '\r';
	    bound[ 1 ] = '\n';
	    bound[ 2 ] = '-';
	    bound[ 3 ] = '-';
	    smem_copy( bound + 4, reqinfo.post_boundary, bound_size - 4 );
	    int bound_p = 2;
	    
	    uint8_t* temp_buf = (uint8_t*)smem_new( bound_size );
	    char* temp_str_buf = (char*)smem_new( MAX_REQ_LINE );
	    
	    char* filename = 0;
	    sfs_file fd = 0;
	    
	    while( bs->eof == 0 && bs->err == 0 )
	    {
		int c = bs_getc( bs );
		if( c < 0 ) break;
		//if( status < 2 )
		{
		    //data or boundary
		    if( c == (uint8_t)bound[ bound_p ] )
		    {
			status = 1;
			temp_buf[ bound_p ] = (uint8_t)c;
			bound_p++;
			if( bound_p == bound_size )
			{
			    //Boundary found.

			    //Skip CRLF:
			    int c1 = bs_getc( bs ); 
			    int c2 = bs_getc( bs );
			    if( c1 == '-' && c2 == '-' ) break;

			    //Close previous files:
			    if( fd ) sfs_close( fd );
			    fd = 0;
			
			    //Get fields:
			    char* temp_str = temp_str_buf;
			    temp_str[ 0 ] = 0;
			    while( 1 )
			    {
				bs_read_line( bs, temp_str, MAX_REQ_LINE - 1 );
				trim( temp_str );
				
				if( temp_str[ 0 ] == '\0' )
				    break;
				    
				if( !strncmp( temp_str, "Content-Disposition: ", 21 ) ) 
				{
				    temp_str += 21;
				    char* name_ptr = strstr( temp_str, "name=\"" );
				    if( name_ptr != NULL )
				    {
					name_ptr += 6;
					char* np = name_ptr;
					while( 1 ) 
					{
					    if( *np == '"' )
					    {
						temp_str = np + 2;
						*np = 0;
					    }
					    if( *np == 0 ) break;
					    np++;
					}
				    }
				    char* filename_ptr = strstr( temp_str, "filename=\"" );
				    if( filename_ptr != NULL )
				    {
					filename_ptr += 10;
					char* np = filename_ptr;
					while( 1 ) 
					{
					    if( *np == '"' )
					    {
						*np = 0;
					    }
					    if( *np == 0 ) break;
					    np++;
					}
					int64_t root_len = strlen( reqinfo.resource );
					int64_t filename_len = strlen( filename_ptr );
					if( filename_len > 2 )
					{
					    //Clean filename:
					    for( int64_t i = filename_len - 1; i >= 0; i-- )
					    {
						if( filename_ptr[ i ] == 0x5C )
						{
						    filename_ptr = filename_ptr + i + 1;
						    filename_len = strlen( filename_ptr );
						    break;
						}
					    }
					}
					if( filename ) smem_free( filename );
					filename = (char*)smem_new( root_len + filename_len + 1 );
					filename[ 0 ] = 0;
					smem_strcat_resize( filename, reqinfo.resource );
					smem_strcat_resize( filename, filename_ptr );
					fd = sfs_open( filename, "wb" );
				    }
				}
			    }
			    
			    status = 0;
			    bound_p = 0;
			}
		    }
		    else 
		    {
			if( status == 1 )
			{
			    //No boundary.
			    //We need to get last parsed bytes from the temp_buf[ 0 ... ( bound_p - 1 ) ].
			    if( fd )
			    {
				sfs_write( temp_buf, 1, bound_p, fd );
			    }
			    status = 0;
			    bound_p = 0;
			}
			if( fd )
			{
			    sfs_write( &c, 1, 1, fd );
			}
		    }
		}
	    }
	    
	    if( bound ) smem_free( bound );
	    if( temp_buf ) smem_free( temp_buf );
	    if( temp_str_buf ) smem_free( temp_str_buf );
	    if( filename ) smem_free( filename );
	    if( fd ) sfs_close( fd );
	    
	    bs_close( bs );
	}
    }

    if( reqinfo.status == 200 )
    {
        res_dir = opendir( reqinfo.resource );
        if( res_dir == 0 )
        {
    	    if( errno == EACCES )
		reqinfo.status = 401;
	    res_size = sfs_get_file_size( reqinfo.resource );
	    res = open( reqinfo.resource, O_RDONLY );
	    if( res < 0 )
	    {
	        if( errno == EACCES )
	            reqinfo.status = 401;
	    }
	}
    }
    if( reqinfo.status == 200 )
    {
        if( res == -1 && res_dir == 0 )
        {
    	    reqinfo.status = 404;
	}
	else
	{
	    if( res >= 0 ) reqinfo.res_type = RES_FILE;
	    if( res_dir ) reqinfo.res_type = RES_DIR;
	}
    }
    
    //Service the HTTP request:
    if( reqinfo.status == 200 ) 
    {
	output_HTTP_headers( conn, &reqinfo, res_size );

	bool answer_ready = false;

	if( reqinfo.resource_parameters )
	{
	    if( reqinfo.resource_parameters[ 0 ] == 'r' )
	    {
		//Remove?
		write_line( conn, webserv_get_string( STR_WEBSERV_HEADER ), 0 );
		write_line( conn, webserv_get_string( STR_WEBSERV_REMOVE ), 0 );
		write_line( conn, reqinfo.resource_parameters + 1, 0 );
		write_line( conn, "\"?<br>", 0 );
		write_line( conn, "<a href=\"", 0 );
		write_line( conn, reqinfo.resource_name, 0 );
		write_line( conn, "?", 0 );
		reqinfo.resource_parameters[ 0 ] = 'R';
		write_line( conn, reqinfo.resource_parameters, 0 );
		write_line( conn, webserv_get_string( STR_WEBSERV_YES ), 0 );
		write_line( conn, "<a href=\"", 0 );
		write_line( conn, reqinfo.resource_name, 0 );
		write_line( conn, webserv_get_string( STR_WEBSERV_NO ), 0 );
		write_line( conn, "</h1></center>", 0 );
		write_line( conn, "</body></html>\r\n", 0 );
		answer_ready = true;
	    }
            else
	    if( reqinfo.resource_parameters[ 0 ] == 'R' )
	    {
		//Remove :(
		int64_t root_len = smem_strlen( reqinfo.resource );
		int64_t rem_len = smem_strlen( reqinfo.resource_parameters + 1 );
		char* rem_name = (char*)smem_new( root_len + rem_len + 1 );
		smem_copy( rem_name, reqinfo.resource, root_len + 1 );
		smem_strcat_resize( rem_name, reqinfo.resource_parameters + 1 );
		sfs_remove( rem_name );
		smem_free( rem_name );
	    }
            if( reqinfo.resource_parameters[ 0 ] == 'd' )
	    {
                //Create directory:
                int64_t root_len = smem_strlen( reqinfo.resource );
		int64_t dir_len = smem_strlen( reqinfo.resource_parameters + 2 );
		char* dir_name = (char*)smem_new( root_len + dir_len + 2 );
		smem_copy( dir_name, reqinfo.resource, root_len + 2 );
		smem_strcat_resize( dir_name, reqinfo.resource_parameters + 2 );
		sfs_mkdir( dir_name, S_IRWXU | S_IRWXG | S_IRWXO );
		smem_free( dir_name );
            }
	}
	
	if( answer_ready )
	    goto service_end;
	
	if( res >= 0 )
	{
	    int rr = return_resource( conn, res, &reqinfo );
	    if( rr )
	    {
		g_webserver_status = "ERROR in return_resource()";
		slog( "%s: %d\n", g_webserver_status, rr );
		rv = -1;
		goto service_end;
	    }
	}
	if( res_dir )
	{
	    char ts[ 256 ];
	    char* fname_d = (char*)smem_new( 1 );
	    fname_d[ 0 ] = 0;
	    
	    slist_data ld;
	    slist_init( &ld );

	    sfs_find_struct fs;
	    smem_clear_struct( fs );
	    fs.start_dir = reqinfo.resource;
	    fs.mask = 0;
	    int fs_res = sfs_find_first( &fs );
	    while( fs_res )
	    {
		int ignore = 0;
		int64_t len = strlen( fs.name );
		if( len == 1 && fs.name[ 0 ] == '.' ) ignore = 1;
		if( len == 2 && fs.name[ 0 ] == '.' && fs.name[ 1 ] == '.' ) ignore = 1;
		if( ignore == 0 )
		    slist_add_item( fs.name, fs.type, &ld );
		fs_res = sfs_find_next( &fs );
	    }
	    sfs_find_close( &fs );
	    
	    slist_sort( &ld );

	    write_line( conn, "<h1>", 0 );
	    write_line( conn, webserv_get_string( STR_WEBSERV_HEADER ), 0 );
	    write_line( conn, webserv_get_string( STR_WEBSERV_TITLE ), 0 );
	    write_line( conn, reqinfo.resource_name, 0 );
	    write_line( conn, "</h1>\r\n", 0 );
	    write_line( conn, "<form action=\"", 0 );
	    write_line( conn, reqinfo.resource_name, 0 );
	    write_line( conn, "\" enctype=\"multipart/form-data\" method=\"post\" accept-charset=\"utf-8\"> \r\n", 0 );
	    write_line( conn, webserv_get_string( STR_WEBSERV_HOWTO ), 0 );
	    write_line( conn, "<input type=\"file\" name=\"f1\" size=\"40\"><br>\r\n", 0 );
	    write_line( conn, "<input type=\"file\" name=\"f2\" size=\"40\"><br>\r\n", 0 );
	    write_line( conn, "<input type=\"file\" name=\"f3\" size=\"40\"><br>\r\n", 0 );
	    write_line( conn, "<input type=\"file\" name=\"f4\" size=\"40\"><br>\r\n", 0 );
	    write_line( conn, webserv_get_string( STR_WEBSERV_SEND ), 0 );
	    write_line( conn, "</p>\r\n", 0 );
	    write_line( conn, "</form>\r\n", 0 );
            
            write_line( conn, "<form action=\"", 0 );
	    write_line( conn, reqinfo.resource_name, 0 );
	    write_line( conn, "\" method=\"get\" accept-charset=\"utf-8\"> \r\n", 0 );
            write_line( conn, "<input type=\"text\" name=\"d\" size=\"40\"> \r\n", 0 );
            write_line( conn, webserv_get_string( STR_WEBSERV_CREATE_DIR ), 0 );
	    write_line( conn, "</form>\r\n", 0 );
            
	    write_line( conn, "<table>\r\n", 0 );
	    write_line( conn, "<tr><th>", 0 );
	    write_line( conn, webserv_get_string( STR_WEBSERV_FNAME ), 0 );
	    write_line( conn, "<th>", 0 );
	    write_line( conn, webserv_get_string( STR_WEBSERV_FSIZE ), 0 );
	    write_line( conn, "<th>", 0 );
	    write_line( conn, webserv_get_string( STR_WEBSERV_FMTIME ), 0 );
	    write_line( conn, "<th>", 0 );

	    if( reqinfo.root == 0 )
	    {
		write_line( conn, "<tr><td>", 0 );
		write_line( conn, webserv_get_string( STR_WEBSERV_HOME ), 0 );
		write_line( conn, "<tr><td>", 0 );
		write_line( conn, webserv_get_string( STR_WEBSERV_PARENT ), 0 );
	    }
	    for( uint i = 0; i < ld.items_num; i++ )
	    {
		write_line( conn, "<tr><td>", 0 );

		char* name = slist_get_item( i, &ld );
		int type = slist_get_attr( i, &ld );
		write_line( conn, "<a href=\"", 0 );
		write_line( conn, name, 0 );
		if( type == 1 ) write_line( conn, "/", 0 );
		write_line( conn, "\">", 0 );
		if( type == 1 ) write_line( conn, "<b>", 0 );
		write_line( conn, name, 0 );
		if( type == 1 ) write_line( conn, "/</b>", 0 );

		if( type == 0 )
		{
		    fname_d[ 0 ] = 0;
		    smem_strcat_resize( fname_d, reqinfo.resource );
	    	    smem_strcat_resize( fname_d, name );
		    size_t fsize = 0;
		    const char* fsize_unit = "";
		    struct tm* t = NULL;
		    struct stat file_info;
		    if( stat( fname_d, &file_info ) == 0 )
		    {
		        fsize = file_info.st_size;
		        t = localtime( &file_info.st_mtime );
			if( fsize > 1024 ) { fsize /= 1024; fsize_unit = "K"; }
			if( fsize > 1024 ) { fsize /= 1024; fsize_unit = "M"; }
			if( fsize > 1024 ) { fsize /= 1024; fsize_unit = "G"; }
		    }
		    write_line( conn, "<td>", 0 );
		    if( fsize )
		    {
			sprintf( ts, "%d%s", (int)fsize, fsize_unit );
			write_line( conn, ts, 0 );
		    }
		    write_line( conn, "<td>", 0 );
		    if( t )
		    {
			sprintf( ts, "%04d.%02d.%02d %02d:%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min );
			write_line( conn, ts, 0 );
		    }
		}
		else
		{
		    write_line( conn, "<td><td>", 0 );
		}

		write_line( conn, "<td>", 0 );
		write_line( conn, "</a> ", 0 );
		write_line( conn, "<a href=\"?r", 0 );
		write_line( conn, name, 0 );
		write_line( conn, webserv_get_string( STR_WEBSERV_REMOVE2 ), 0 );
	    }

	    write_line( conn, "</table>\r\n", 0 );

	    write_line( conn, "</body></html>\r\n", 0 );

	    slist_deinit( &ld );
	    
	    smem_free( fname_d );
	}
    }
    else
    {
	return_error_msg( conn, &reqinfo );
    }

service_end:

    if( res >= 0 )
    {
	if( close( res ) < 0 )
	{
	    g_webserver_status = "ERROR in closing resource";
	    slog( "%s\n", g_webserver_status );
	}
    }
    if( res_dir )
    {
	if( closedir( res_dir ) < 0 )
	{
	    g_webserver_status = "ERROR in closing resource (dir)";
	    slog( "%s\n", g_webserver_status );
	}
    }

    free_req_info( &reqinfo );

    return rv;
}

//Main webserver thread
static void* webserver_thread( void* arg )
{
    int listener = -1;
    int conn = -1;

    struct sockaddr_in servaddr;
    int tval;

    g_webserver_pth_work = 1;
    
    g_webserver_status = webserv_get_string( STR_WEBSERV_STATUS_STARTING );

    //Create socket:
    if( ( listener = socket( AF_INET, SOCK_STREAM, 0 ) ) < 0 )
    {
	g_webserver_status = "ERROR: Couldn't create listening socket";
	slog( "%s\n", g_webserver_status );
	goto tend;
    }

    tval = 1;
    if( setsockopt( listener, SOL_SOCKET, SO_REUSEADDR, &tval, sizeof( int ) ) != 0 )
    {
	g_webserver_status = "ERROR in setcoskopt()";
	slog( "%s\n", g_webserver_status );
	goto tend;
    }

    //Populate socket address structure:
    memset( &servaddr, 0, sizeof( servaddr ) );
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl( INADDR_ANY );
    servaddr.sin_port = htons( g_webserver_port );

    //Assign socket address to socket:
    if( bind( listener, (struct sockaddr *) &servaddr, sizeof( servaddr ) ) < 0 )
    {
	g_webserver_status = "ERROR: Couldn't bind listening socket";
	slog( "%s %s\n", g_webserver_status, strerror( errno ) );
	goto tend;
    }

    //Make socket a listening socket:
    if( listen( listener, 4 ) < 0 )
    {
	g_webserver_status = "ERROR in listen()";
	slog( "%s\n", g_webserver_status );
	goto tend;
    }

    //Loop infinitely to accept and service connections:
    while( g_webserver_exit_request == 0 ) 
    {
	//Wait for connection:
	
	g_webserver_status = webserv_get_string( STR_WEBSERV_STATUS_WORKING );

	int select_result = wait_socket_read( listener, 1 );
	if( select_result == 0 )
	{
	    //Select timeout:
	    continue;
	}
	if( select_result < 0 )
	{
	    g_webserver_status = "ERROR in select()";
	    slog( "%s\n", g_webserver_status );
	    goto tend;
	}

	if( ( conn = accept( listener, NULL, NULL ) ) < 0 )
	{
	    g_webserver_status = "ERROR in accept()";
	    slog( "%s\n", g_webserver_status );
	    goto tend;
	}

	service_request( conn );

        //Close connected socket:
	shutdown( conn, SHUT_RDWR );
	if( close( conn ) < 0 )
	{
	    g_webserver_status = "ERROR in closing connection socket";
	    slog( "%s\n", g_webserver_status );
	    goto tend;
	}
    }

tend:

    if( listener >= 0 )
    {
	shutdown( listener, SHUT_RDWR );
	if( close( listener ) < 0 )
	{
	    g_webserver_status = "ERROR in closing listener socket";
	    slog( "%s\n", g_webserver_status );
	}
    }

    g_webserver_finished = 1;

#if defined(OS_ANDROID) && !defined(NOMAIN)
    android_sundog_release_jni( g_webserver->wm->sd );
#endif
    pthread_exit( NULL );
}

struct webserver_data
{
    int timer;
    char* host_addr;
    char* addr_list;
    WINDOWPTR close;
};

static int webserver_close_button_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    remove_window( g_webserver, wm );
    recalc_regions( wm );
    draw_window( wm->root_win, wm );
    return 0;
}

static void webserver_timer( void* user_data, sundog_timer* t, window_manager* wm )
{
    if( g_webserver )
    {
	draw_window( g_webserver, wm );
    }
}

static int webserver_handler( sundog_event* evt, window_manager* wm )
{
    int retval = 0;
    WINDOWPTR win = evt->win;
    webserver_data* data = (webserver_data*)win->data;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( webserver_data );
	    break;
	case EVT_AFTERCREATE:
	    {
		g_webserver = win;
		
		data->host_addr = NULL;
		data->addr_list = NULL;
		snet_get_host_info( wm->sd, &data->host_addr, &data->addr_list );
		signal( SIGPIPE, SIG_IGN ); //Ignore SIGPIPE (write to the socket, when the receiver is closed)
		
		g_webserver_root[ 0 ] = 0;
		smem_strcat( g_webserver_root, sizeof( g_webserver_root ), sfs_get_work_path() );
                
		data->close = new_window( wm_get_string( STR_WM_CLOSE ), 0, 0, 1, 1, wm->button_color, win, button_handler, wm );
        	set_handler( data->close, webserver_close_button_handler, data, wm );
        	set_window_controller( data->close, 0, wm, (WCMD)wm->interelement_space, CEND );
		set_window_controller( data->close, 1, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
		set_window_controller( data->close, 2, wm, (WCMD)wm->interelement_space + wm->button_xsize, CEND );
		set_window_controller( data->close, 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->button_ysize + wm->interelement_space, CEND );
		
		//Create the timer:
		data->timer = add_timer( webserver_timer, data, 1000, wm );
	    
		//Start webserver thread:
                g_webserver_window_closed = 0;
		g_webserver_finished = 0;
		g_webserver_exit_request = 0;
		g_webserver_pth_work = 0;
		int err = pthread_create( &g_webserver_pth, 0, &webserver_thread, (void*)0 );
		if( err == 0 )
		{
    		    //The pthread_detach() function marks the thread identified by thread as
    		    //detached. When a detached thread terminates, its resources are
    		    //automatically released back to the system.
		    err = pthread_detach( g_webserver_pth );
    		    if( err != 0 )
    		    {
        		g_webserver_status = "ERROR: (webserver init) pthread_detach failed";
			slog( "%s\n", g_webserver_status );
    		    }
		}
		else
		{
		    g_webserver_status = "ERROR: (webserver init) pthread_create failed";
		    slog( "%s\n", g_webserver_status );
		}
	    }
	    retval = 1;
	    break;
	case EVT_DRAW:
	    {
		wbd_lock( win );
                
                int ychar = char_y_size( wm );
                int x = wm->interelement_space;
                int y = wm->interelement_space;
		int txt_ysize = 0;
                int scale = 512;
                COLOR color_red = blend( wm->red, wm->color3, 200 );
                COLOR color_blue = blend( wm->blue, wm->color3, 200 );
		
		draw_frect( 0, 0, win->xsize, win->ysize, win->color, wm );

		wm->cur_font_color = wm->color2;
                
                const char* msg_str = webserv_get_string( STR_WEBSERV_MSG );
                const char* status_name = webserv_get_string( STR_WEBSERV_STATUS );
                const char* other_addr = webserv_get_string( STR_WEBSERV_OTHER_ADDR );

		char port_str[ 16 ];
		int_to_string( g_webserver_port, port_str );
                
		if( !data->host_addr || smem_strcmp( data->host_addr, "127.0.0.1" ) == 0 )
		{
		    wm->cur_font_scale = scale;
		    wm->cur_font_color = color_red;
		    txt_ysize = 0;
		    draw_string_wordwrap( 
			webserv_get_string( STR_WEBSERV_ERROR_CONN ),
			wm->interelement_space, 
			wm->interelement_space,
			win->xsize - wm->interelement_space * 2,
			0, &txt_ysize, false,
			wm );
		    y += txt_ysize;
		}
		else
		{
		    txt_ysize = 0;
		    draw_string_wordwrap( msg_str, x, y, win->xsize - wm->interelement_space * 2, 0, &txt_ysize, false, wm );
		    y += txt_ysize;
		
		    //Draw the address:
	    	    {
	    		wm->cur_font_scale = scale;
			wm->cur_font_color = color_red;
			char addr[ 256 ];
			addr[ 0 ] = 0;
			smem_strcat( addr, sizeof( addr ), "http://" );
			smem_strcat( addr, sizeof( addr ), data->host_addr );
			smem_strcat( addr, sizeof( addr ), ":" );
			smem_strcat( addr, sizeof( addr ), port_str );
			if( string_x_size( addr, wm ) >= win->xsize )
	    		    wm->cur_font_scale = 256;
			draw_string( addr, x, y, wm );
			y += ychar * wm->cur_font_scale / 256;
			wm->cur_font_scale = 256;
		    }
		
		    //Draw the status:
		    if( g_webserver_status )
		    {
			y += ychar / 2;
			wm->cur_font_color = color_blue;
			draw_string( status_name, x, y, wm );
			y += ychar;
			draw_string( g_webserver_status, x, y, wm );
			y += ychar;
		    }
		}
		
		if( 1 ) //data->addr_list )
		{
		    wm->cur_font_scale = 256;
		    char addr[ 256 ];
		    wm->cur_font_color = blend( wm->color2, win->color, 128 );
		    y += ychar;
		    draw_string( other_addr, x, y, wm );
		    y += ychar;
		    sprintf( addr, "http://localhost:%s (%s)", port_str, webserv_get_string( STR_WEBSERV_LOCALHOST_DESCR ) );
		    draw_string( addr, x, y, wm );
		    y += ychar;
		    char* str = data->addr_list;
		    while( str )
		    {
			str = (char*)smem_split_str( addr, sizeof( addr ), str, ' ', 0 );
			smem_strcat( addr, sizeof( addr ), ":" );
			smem_strcat( addr, sizeof( addr ), port_str );
			draw_string( addr, x, y, wm );
			y += ychar;
		    }
		}

		wbd_draw( wm );
		wbd_unlock( wm );
	    }
	    retval = 1;
	    break;
	case EVT_BEFORECLOSE:
	    {
		//Close the thread:
		if( g_webserver_pth_work )
		{
		    //Stop the thread:
		    g_webserver_exit_request = 1;
		    int timeout_counter = 1000 * 5; //Timeout in milliseconds
		    while( timeout_counter > 0 )
		    {
    			struct timespec delay;
    			delay.tv_sec = 0;
    			delay.tv_nsec = 1000000 * 20; //20 milliseconds
    			if( g_webserver_finished ) break;
    			nanosleep( &delay, NULL ); //Sleep for delay time
    			timeout_counter -= 20;
		    }
		    if( timeout_counter <= 0 )
		    {
#ifndef OS_ANDROID
    			pthread_cancel( g_webserver_pth );
#else
			slog( "Can't close the webserver thread\n" );
#endif
		    }
		}
	
		//Close the timer:
		if( data->timer >= 0 )
    		{
        	    remove_timer( data->timer, wm );
        	    data->timer = -1;
    		}
    		
    		smem_free( data->host_addr );
    		smem_free( data->addr_list );
		
		g_webserver = NULL;
                g_webserver_window_closed = 1;
	    }
	    retval = 1;
	    break;
	case EVT_TOUCHBEGIN:
	case EVT_TOUCHEND:
	case EVT_TOUCHMOVE:
	    retval = 1;
	    break;
    }
    return retval;
}

void webserver_open( window_manager* wm )
{
    if( g_webserver == NULL )
    {
	uint flags = DECOR_FLAG_STATIC | DECOR_FLAG_NOBORDER | DECOR_FLAG_MAXIMIZED;
	WINDOWPTR w = new_window_with_decorator(
	    webserv_get_string( STR_WEBSERV_IMPORT_EXPORT ),
	    320, 200,
	    wm->dialog_color,
	    wm->root_win,
	    webserver_handler,
	    flags,
	    wm );
	show_window( w );
	bring_to_front( w, wm );
	recalc_regions( wm );
	draw_window( wm->root_win, wm );
    }
}

void webserver_wait_for_close( window_manager* wm )
{
    while( g_webserver_window_closed == 0 )
    {
        sundog_event evt;
        EVENT_LOOP_BEGIN( &evt, wm );
        if( EVENT_LOOP_END( wm ) ) break;
    }
    g_webserver_window_closed = 0;
}

#else

bool g_webserver_available = 0;
void webserver_open( window_manager* wm ) {}
void webserver_wait_for_close( window_manager* wm ) {}

#endif
