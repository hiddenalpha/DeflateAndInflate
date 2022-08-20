/*

  QuickNDirty bash equivalent:

  bash-deflate(){ gzip -c | tail -c +10; }

*/

#include "commonbase.h"

/* system */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* libs */
#include <zlib.h>


typedef  unsigned char  uchar; /* TODO rm */
typedef  struct Deflate  Deflate;


struct Deflate {
    int level;
    int useZlibHdr;
};


static void printHelp( void ){
    printf("\n  %s%s", strrchr(__FILE__,'/')+1, " @ " STR_QUOT(PROJECT_VERSION) "\n"
        "\n"
        "deflates stdin to stdout\n"
        "\n"
        "Options:\n"
        "\n"
        "    --level\n"
        "        Compression level 0-9 (0 fastest, 9 smallest).\n"
        "\n"
        "    --zlib\n"
        "        Prepend output with a zlib header.\n"
        "\n");
}


static int parseArgs( int argc, char**argv, Deflate*deflateCls ){
    deflateCls->level = Z_DEFAULT_COMPRESSION;
    deflateCls->useZlibHdr = 0;
    for( int i = 1 ; i < argc ; ++i ){
        char *arg = argv[i];
        if( !strcmp(arg,"--help") ){
            printHelp(); return -1;
        }else if( !strcmp(arg,"--level") ){
            arg = argv[++i];
            if( arg == NULL ){ fprintf(stderr, "Arg level needs a value\n"); return -1; }
            char *end;
            deflateCls->level = strtol(arg, &end, 10);
            if( arg == end ){ fprintf(stderr, "Failed to parse '%s'\n", arg); return -1; }
            if( deflateCls->level < 0 || deflateCls->level > 9 ){
                fprintf(stderr, "OutOfRange: %s %s\n", argv[i-1], arg); return -1;
            }
        }else if( !strcmp(arg,"--zlib") ){
            deflateCls->useZlibHdr = !0;
        }else{
            fprintf(stderr, "Unknown arg: %s\n", arg);
            return -1;
        }
    }
    return 0;
}


static int doDeflate( Deflate*deflateCls ){
    int err;
    z_stream strm;
    uchar innBuf[65536];
    const int innBuf_cap = sizeof innBuf;
    int innBuf_len = 0, innBuf_off = 0;
    uchar outBuf[65536];
    const int outBuf_cap = sizeof outBuf;
    int outBuf_len = 0;
    int zlibHdrWritten = deflateCls->useZlibHdr ? 2 : 0;
    int inputIsEOF = 0;
    int outputIsEOF = 0;

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.next_in = innBuf;
    strm.avail_in = innBuf_len;

    err = deflateInit(&strm, deflateCls->level);
    if( err != Z_OK ){
        fprintf(stderr, "Error: deflateInit(): (ret=%d): %s\n", err, strerror(errno));
        return -1;
    }

    for(; !outputIsEOF ;){

        /* input */ {
            int space = innBuf_cap - innBuf_len;
            /* look if we can shift */
            if( space <= 0 && innBuf_off > 0 ){
                int wr = 0, rd = innBuf_off;
                for(;;){
                    err = rd - wr; /* chunk length to copy */
                    if( rd + err > innBuf_cap) err = innBuf_cap - rd;
                    if( err <= 0 ) break;
                    memcpy(innBuf + wr, innBuf + rd, err);
                    rd += err;
                    wr += err;
                }
                innBuf_len -= innBuf_off;
                innBuf_off = 0;
                space = innBuf_cap - innBuf_len;
            }
            if( space > 0 && !inputIsEOF ){
                err = fread(innBuf + innBuf_len, 1, space, stdin);
                if(unlikely( err <= 0 )){
                    if( feof(stdin) ){
                        inputIsEOF = !0;
                    }else{
                        fprintf(stderr, "Error: fread(): %s\n", strerror(errno));
                        return -1;
                    }
                }
                assert(err <= space);
                innBuf_len += err;
            }
            assert(innBuf_len - innBuf_off >= 0);
        }

        /* process */ {
            strm.next_in = innBuf + innBuf_off;
            strm.avail_in = innBuf_len - innBuf_off;
            strm.next_out = outBuf;
            strm.avail_out = outBuf_cap;
            assert(strm.avail_out > 0);
            assert(strm.avail_in > 0 || inputIsEOF);
            err = deflate(&strm, inputIsEOF ? Z_FINISH : Z_NO_FLUSH);
            if(unlikely( err != Z_OK )){
                if( err == Z_STREAM_END ){
                    outputIsEOF = !0;
                }else{
                    fprintf(stderr, "Error: deflate(): %s\n", strm.msg);
                    return -1;
                }
            }
            innBuf_off = strm.next_in - innBuf;
            outBuf_len = strm.next_out - outBuf;
        }

        /* output */ {
            int outBuf_off = 0;
            if( zlibHdrWritten < 2 ){
                /* try to skip up to 2 bytes */
                err = 2 - zlibHdrWritten;
                if( outBuf_len < err ){ err = outBuf_len; }
                zlibHdrWritten += err;
                outBuf_off += err;
            }
            err = fwrite(outBuf + outBuf_off, 1, outBuf_len - outBuf_off, stdout);
            if(unlikely( err != outBuf_len - outBuf_off )){
                fprintf(stderr, "Error: fwrite(): %s\n", strerror(errno));
                return -1;
            }
            outBuf_len = 0;
        }
    }

    err = deflateEnd(&strm);
    if( err != Z_OK ){
        fprintf(stderr, "Warn: deflateEnd() ret=%d: %s\n", err, strm.msg);
        return -2;
    }

    return 0;
}


int deflate_main( int argc, char**argv ){
    int err;
    Deflate deflateCls;
    #define deflateCls (&deflateCls)

    err = parseArgs(argc, argv, deflateCls);
    if( err < 0 ){ return err; }

    fixBrokenStdio();

    err = doDeflate(deflateCls);
    if( err < 0 ){ return err; }

    return 0;
    #undef deflate
}

