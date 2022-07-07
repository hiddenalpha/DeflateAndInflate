/*

  bash-deflate(){ gzip -c | tail -c +10; }
  bash-inflate(){ (printf "\x1f\x8b\x08\x00\x00\x00\x00\x00\x00"&&cat -)|gzip -dc; }

*/

#include "commonbase.h"

/* system */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

/* libs */
#include "zlib.h"

/* project */
#include "log.h"
#include "string-util.h"


typedef  struct MyInflate  MyInflate;


struct MyInflate {
    int tryParseHeaders;
};


static void printHelp( void ){
    printf("\n  %s%s", strrchr(__FILE__,'/')+1, " @ " STR_QUOT(PROJECT_VERSION) "\n"
        "\n"
        "inflates stdin to stdout\n"
        "\n"
        "Options:\n"
        "\n"
        "    --raw\n"
        "        By default, we'll try to decode leading headers. Using this\n"
        "        option, no header parsing is done and the input is expected to\n"
        "        be a pure deflate stream.\n"
        "\n"
        "\n");
}


static int parseArgs( int argc, char**argv, MyInflate*myInflate ){
    myInflate->tryParseHeaders = !0; /* enabled by default */
    for( uint i=1 ; i<argc ; ++i ){
        char *arg = argv[i];
        if( !strcmp(arg,"--help") ){
            printHelp(); return -1;
        }else if( !strcmp(arg,"--raw") ){
            myInflate->tryParseHeaders = 0;
        }else{
            fprintf(stderr, "Unknown arg: %s\n", arg);
            return -1;
        }
    }
    return 0;
}


static int doInflate( MyInflate*myInflate ){
    #define MAX(a, b) ((a) > (b) ? (a) : (b))
    int err;
    z_stream strm;
    uchar innBuf[65536];
    const int innBuf_cap = sizeof innBuf;
    int innBuf_len = 0, innBuf_off = 0;
    uchar outBuf[65536];
    const int outBuf_cap = sizeof outBuf;
    int outBuf_len = 0;
    int inputIsEOF = 0;
    int outputIsEOF = 0;
    int noProgressSince = 0;
    /* only counts while reading through possible file headers. Later, the
     * counter will not be updated anymore, to prevent overflows */
    int headerIsPassed = 0;

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.next_in = innBuf;
    strm.avail_in = innBuf_len;

    // Pass special 2nd arg. See "https://codeandlife.com/2014/01/01/unzip-library-for-c/"
    err = inflateInit2(&strm, -MAX_WBITS);
    if( err != Z_OK ){
        LOG_ERROR("inflateInit() ret=%d: %s", err, strerror(errno));
        return -1;
    }

    for(; !inputIsEOF || !outputIsEOF ;){

        /* input */ {
            int space = innBuf_cap - innBuf_len;
            /* look if we can shift */
            if( space <= 0 && innBuf_off > 0 ){
                //LOG_DEBUG("try-shift space=%d, innBuf_off=%d\n", space, innBuf_off);
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
                //LOG_DEBUG("after shift space=%d, innBuf_off=%d, innBuf_len=%d\n", space, innBuf_off, innBuf_len);
            }
            if( space > 0 && !inputIsEOF ){
                //LOG_TRACE("go read  space=%d, inputIsEOF=%d\n", space, inputIsEOF);
                err = fread(innBuf + innBuf_len, 1, space, stdin);
                //LOG_TRACE("fread() -> %d\n", err);
                if(unlikely( err <= 0 )){
                    if( feof(stdin) ){
                        inputIsEOF = !0;
                    }else{
                        LOG_ERROR("fread(): %s", strerror(errno));
                        return -1;
                    }
                }
                assert(err <= space);
                innBuf_len += err;
            }
        }

        if(unlikely( !headerIsPassed )){
            #define avail (innBuf_len - innBuf_off)
            /* Ensure we have at least two bytes for zlib header detection */
            if( avail < 2 && !inputIsEOF ){ LOG_DEBUG("Not enough data for header detection. have"
                    " %d\n", avail); continue; }
            headerIsPassed = !0;
            if( myInflate->tryParseHeaders ){
                int b0 = innBuf[0], b1 = innBuf[1];
                if( b0 == 0x78 && (b1 == 0x01 || b1 == 0x5E || b1 == 0x9C || b1 == 0xDA) ){
                    //LOG_DEBUG("looks like a zlib header. Skip it\n");
                    innBuf_off += 2;
                }
            }
            #undef avail
        }

        /* process */ {
            strm.next_in = innBuf + innBuf_off;
            strm.avail_in = MAX(0, innBuf_len - innBuf_off);
            strm.next_out = outBuf;
            strm.avail_out = outBuf_cap;
            // TODO remove assert(strm.avail_in >= 0 || inputIsEOF);
            assert(strm.avail_out > 0);
            int flush = inputIsEOF ? Z_FINISH : Z_NO_FLUSH;
            //LOG_DEBUG("inflate(in=0x%p, inLen=%d, %s)\n", strm.next_in, strm.avail_in,
            //        (flush==Z_FINISH) ? "Z_FINISH" : (flush==Z_FULL_FLUSH) ? "Z_FULL_FLUSH" : "Z_NO_FLUSH");
            errno = 0;
            err = inflate(&strm, flush);
            if(unlikely( err != Z_OK )){
                if( err == Z_STREAM_END ){
                    //LOG_DEBUG("inflate() -> Z_STREAM_END\n");
                    outputIsEOF = !0;
                }else if( err == Z_BUF_ERROR ){
                    /* Could not make any progress. Have to call again with updated buffers */
                    noProgressSince += 1;
                    if( noProgressSince > 42 ){
                        LOG_WARN("inflate() -> Z_BUF_ERROR: %s (maybe EOF came too early?)\n",
                                (strm.msg == NULL) ? "Could not make any progress" : strm.msg);
                        return -1;
                    }
                }else if( strm.msg != NULL || errno ){
                    LOG_ERROR("inflate(): %s\n", (strm.msg != NULL) ? strm.msg : strerror(errno));
                    return -1;
                }else{
                    LOG_ERROR("inflate(): -> %d\n", err);
                    return -1;
                }
            }else{
                noProgressSince = 0; /* reset as we just made progress */
            }
            innBuf_off += strm.next_in - innBuf;
            outBuf_len = strm.next_out - outBuf;
            if( innBuf_off > innBuf_len ){ innBuf_off = innBuf_len; }
            //LOG_DEBUG("strm.remain=%d, outputIsEOF=%d\n", innBuf_len - innBuf_off, outputIsEOF);
        }

        /* output */ {
            err = fwrite(outBuf, 1, outBuf_len, stdout);
            //LOG_DEBUG("fwrite(0x%p, 1, %d, stdout) -> %d\n", outBuf, outBuf_len, err);
            if(unlikely( err != outBuf_len )){
                LOG_ERROR("fwrite(): %s\n", strerror(errno));
                return -1;
            }
            outBuf_len = 0;
        }
    }

    err = inflateEnd(&strm);
    if( err != Z_OK ){
        LOG_WARN("inflateEnd() ret=%d: %s\n", err, strm.msg);
    }

    return 0;
    #undef MAX
}


int inflate_main( int argc, char**argv ){
    int err;
    MyInflate myInflate;
    #define myInflate (&myInflate)

    err = parseArgs(argc, argv, myInflate);
    if( err < 0 ){ return err; }

    stdinModeBinary();
    stdoutModeBinary();

    err = doInflate(myInflate);
    if( err < 0 ){ return err; }

    return 0;
    #undef myInflate
}


