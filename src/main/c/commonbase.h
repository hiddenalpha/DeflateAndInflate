
/* common config. Stuff like feature-test-macros for example */

#define _POSIX_C_SOURCE 200809L

#define STR_QUOT_IAHGEWIH(s) #s
#define STR_QUOT(s) STR_QUOT_IAHGEWIH(s)

#define STR_CAT(a, b) a ## b

#ifndef likely
#   define likely(a) (a)
#endif
#ifndef unlikely
#   define unlikely(a) (a)
#endif

#ifdef _WIN32
#   define WINDOOF 1
#endif


#if WINDOOF
void fixBrokenStdio( void );
#else
#   define fixBrokenStdio()
#endif

