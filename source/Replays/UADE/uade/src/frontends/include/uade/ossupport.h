#ifndef _UADE_OSSUPPORT_H_
#define _UADE_OSSUPPORT_H_

#include <uade/unixsupport.h>

typedef ptrdiff_t ssize_t;

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif // PATH_MAX

#define strcasecmp _stricmp
#define strncasecmp _strnicmp

inline char* strsep(char** stringp, const char* delim)
{
    char* start = *stringp;
    char* p;

    p = (start != NULL) ? strpbrk(start, delim) : NULL;

    if (p == NULL)
    {
        *stringp = NULL;
    }
    else
    {
        *p = '\0';
        *stringp = p + 1;
    }

    return start;
}

/* Values for the second argument to access.
   These may be OR'd together.  */
#define R_OK    4       /* Test for read permission.  */
#define W_OK    2       /* Test for write permission.  */
//#define   X_OK    1       /* execute permission - unsupported in windows*/
#define F_OK    0       /* Test for existence.  */

/* Read user permission */
#if !defined(S_IRUSR)
#   define S_IRUSR S_IREAD
#endif

/* Write user permission */
#if !defined(S_IWUSR)
#   define S_IWUSR S_IWRITE
#endif

/* Execute user permission */
#if !defined(S_IXUSR)
#   define S_IXUSR 0
#endif

#endif
