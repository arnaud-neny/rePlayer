#pragma once

#define NO_LOG

inline char* basename(char const* name)
{
	char const* base = name;
	char const* p;

	for (p = base; *p; p++)
	{
		if (*p == '/' || *p == '\\')
		{
			/* Treat multiple adjacent slashes like a single slash.  */
			do p++;
			while (*p == '/' || *p == '\\');

			/* If the file name ends in slash, use the trailing slash as
			   the basename if no non-slashes have been found.  */
			if (!*p)
			{
				if (*base == '/' || *base == '\\')
					base = p - 1;
				break;
			}

			/* *P is a non-slash preceded by a slash.  */
			base = p;
		}
	}

	return (char*)base;
}

inline void myNop() {}

#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define zz_nop myNop()