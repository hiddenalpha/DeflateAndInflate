
Guide how to cross build for windoof
====================================

## Get Source

Change into an EMPTY directory and do:

```sh
git clone --depth=256 https://github.com/hiddenalpha/DeflateAndInflate.git ./.
```


## Choose an alpine system

Take the alpine system you want to use as build env. A nice way to
achieve this is via docker.

In the command below, make sure WORK points to the dir where you cloned
the project to.

```sh
true\
    && WORK="${PWD:?}" \
    && IMG=alpine:3.17.2 \
    && CNTR=DeflateAndInflate-AlpineMingw64 \
    && docker pull "${IMG:?}" \
    && docker create --name "${CNTR:?}" -v "${WORK:?}:/work" -w /work "${IMG:?}" sh -c "while true;do sleep 1||break;done" \
    && docker start "${CNTR:?}" \
    && true
```

All snippets here assume this docker setup. But can easily be used
without docker and should work in any POSIX compliant shell. For this
only copy the shell script inside the ticks (') of each command. In
other words just use the part starting with *true* and ending with
another *true*.


## Setup dependencies

```sh
docker exec -ti -u0:0 "${CNTR:?}" sh -c 'true \
    && apk add curl git make mingw-w64-gcc tar \
    && ZLIB_VERSION="1.2.11" \
    && THEOLDPWD="$PWD" \
    && cd /tmp \
    && curl -LsS -o "/tmp/zlib-${ZLIB_VERSION}.tgz" "https://github.com/madler/zlib/archive/refs/tags/v${ZLIB_VERSION:?}.tar.gz" \
    && tar xzf "/tmp/zlib-${ZLIB_VERSION:?}.tgz" \
    && export SRCDIR="/tmp/zlib-${ZLIB_VERSION:?}" \
    && mkdir $SRCDIR/build \
    && cd "${SRCDIR:?}" \
    && export DESTDIR=./build BINARY_PATH=/bin INCLUDE_PATH=/include LIBRARY_PATH=/lib \
    && sed -i "s;^PREFIX =.\*\$;;" win32/Makefile.gcc \
    && make -e -fwin32/Makefile.gcc PREFIX=x86_64-w64-mingw32- \
    && make -e -fwin32/Makefile.gcc install PREFIX=x86_64-w64-mingw32- \
    && unset DESTDIR BINARY_PATH INCLUDE_PATH LIBRARY_PATH \
    && cp README build/. \
    && (cd build && rm -rf lib/pkgconfig) \
    && (cd build && find -type f -not -name MD5SUM -exec md5sum -b {} + > MD5SUM) \
    && (cd build && tar --owner=0 --group=0 -cz *) > /tmp/zlib-1.2.11-windoof.tgz \
    && cd / \
    && rm -rf /tmp/zlib-1.2.11 \
    && mkdir -p /usr/local/x86_64-w64-mingw32 \
    && tar -C /usr/x86_64-w64-mingw32 -f /tmp/zlib-1.2.11-windoof.tgz -x include lib \
    && cd "${THEOLDPWD:?}" \
    && unset THEOLDPWD ZLIB_VERSION \
    && true'
```

## configure & make

Build the project itself.

```sh
docker exec -ti -u$(id -u):$(id -g) "${CNTR:?}" sh -c 'true\
    && export CC=x86_64-w64-mingw32-gcc \
    && sh ./configure \
    && make -e clean \
    && make -e \
    && true'
```

A ready-to-distribute tarball is now available in `./dist/`. If you'd
like to install it somewhere else, all you need is this tarball. If you
want to install on same system where you did build you can continue with
the *install* step below.

 
## install

There is no trivial *make-install* equivalent for windoof. Therefore
unpack the tarball into an appropriate directory of your choise and make
sure *bin* subdir is in your PATH.  

