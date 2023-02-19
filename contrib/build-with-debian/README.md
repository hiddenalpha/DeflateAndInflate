
Showcase how to build and install on debian
===========================================

Guide how to build on a debian based system.


## Get Source

Change into an EMPTY directory and do:

```sh
git clone --depth=256 https://github.com/hiddenalpha/DeflateAndInflate.git ./.
```


## Choose a debian 9 system.

Take the debian system you want to use to build. A nice way to achieve
this is via docker (TIPP: take an older image for better ABI
portability):

In the command below, make sure WORK points to the dir where you cloned
the project to.

```sh
true \
    && WORK="$PWD" \
    && IMG=debian:oldstable-20230208-slim \
    && CNTR=DeflateAndInflate-DebianBuild \
    && docker pull "${IMG:?}" \
    && docker create --name "${CNTR:?}" -v "${WORK:?}:/work" -w /work "${IMG:?}" sh -c "while true;do sleep 1||break;done" \
    && docker start "${CNTR:?}" \
    && true
```


## Setup build environment

```sh
docker exec -ti -u0:0 "${CNTR:?}" sh -c 'true\
    && apt-get update \
    && apt-get install -y --no-install-recommends \
           curl gcc git make libc-dev ca-certificates tar zlib1g-dev \
    && true'
```


## configure & make

```sh
docker exec -ti -u$(id -u):$(id -g) "${CNTR:?}" sh -c 'true\
    && ./configure \
    && make -e clean \
    && make -e \
    && true'
```

A ready-to-distribute tarball is now ready in `./dist/`. If you'd like
to install it somewhere else, all you need is this tarball. If you want
to install on same system where you did build you can continue with the
*install* step below.


## install

BTW: This does nothing else than unpacking the built tar into configured
     directories.

```sh
make -e install
```

