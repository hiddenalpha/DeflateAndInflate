#include "commonbase.h"

#if WINDOOF

#include <fcntl.h>
#include <stdio.h>
#include <windows.h>


void fixBrokenStdio( void ){
    _setmode(_fileno(stdin), O_BINARY);
    _setmode(_fileno(stdout), O_BINARY);
}

#endif /* end WINDOOF */
