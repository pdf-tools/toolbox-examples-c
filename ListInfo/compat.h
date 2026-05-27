#ifndef COMPAT_H
#define COMPAT_H

#if defined(WIN32) || defined(_WIN32)
#define PATH_SEP_CHAR '\\'
#define PATH_SEP_STR  "\\"
#else
#define PATH_SEP_CHAR '/'
#define PATH_SEP_STR  "/"
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#if !defined(WIN32) && !defined(_WIN32)
#include <strings.h>          // POSIX: strcasecmp
#define TCHAR char
#define _tcslen strlen
#define _tcscat strcat
#define _tcscpy strcpy
#define _tcsrchr strrchr
#define _tcstok strtok
#define _tcscmp strcmp
#define _tcsicmp strcasecmp
#define _tcsftime strftime
#define _tcsncpy strncpy
#define _tmain main
#define _tfopen fopen
#define _ftprintf fprintf
#define _stprintf sprintf
#define _sntprintf snprintf
#define _tstof atof
#define _tremove remove
#define _tprintf printf
#define _stricmp strcasecmp
#define _T(str) str
#endif

#endif /* COMPAT_H */
