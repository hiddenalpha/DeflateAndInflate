
Showcase how to build and install
=================================

WARN: Do NOT perform any of these steps on your host system! This script
      MUST only be run on a system which is a
      just-throw-it-away-if-broken system.

Sometimes happy developers (like me) have no choice but using terribly
restricted systems where setting up tools to run even something as
trivial as configure/make/install becomes a nightmare if not impossible.
I found it to be very handy to have some independent qemu VM at hand
which lets me install whatever I need, neither with any special software
nor any annoying privileges on a host machine. Qemu runs portable and in
user mode even doesn't need any annoying permissions at all.



## Setup a minimal system in your qemu VM

This setup mainly targets debian. Nevertheless it tries to stay POSIX
compatible as far as possible. So setup a minimal install of your system
of choice and then as soon you've SSH access to a (posix) shell, you're
ready for the next step.

Still not sure which system to use? Link below provides some candidates.
HINT: Windows IMHO is a terrible choice. So stop complaining if you go
this route.

https://en.wikipedia.org/wiki/POSIX#POSIX-oriented_operating_systems



## Start VM with SSH access

Easiest way to work with your machine is via SSH. Therefore if you've
chosen to use a qemu VM, make sure you've setup and configured sshd
properly inside the VM. Then just pass args like those to qemu:

  --device e1000,netdev=n0 --netdev user,id=n0,hostfwd=tcp:127.0.0.1:2222-:22

Started this way, the SSHDaemon inside the VM is accessible from your
host via "localhost" at port "2222":

  ssh localhost -p2222

I strongly recommend using SSH paired with a well-designed
terminal-emulator in place of some fancy VGA/GUI emulators or similar.
Those commandline tools give immediate benefits like copy-pasting of
script snippets which becomes very handy with the scripts which follow,
or also with file transfers to get the final build result out the VM. My
condolence to users which still think windows is the only way to use a
computer.



## Configure build environment

Setup environ vars according to your chosen build system. Here are few examlpes:

### Config for debian
true \
  && PKGS_TO_ADD="ca-certificates curl gcc git make libc-dev tar" \
  && SUDO=sudo \
  && PKGINIT="$SUDO apt update" \
  && PKGADD="$SUDO apt install -y --no-install-recommends" \
  && PKGCLEAN="$SUDO apt clean" \
  && HOST= \
  && true

### Config for alpine with w64 cross compiler
true \
  && PKGS_TO_ADD="curl mingw-w64-gcc git make tar" \
  && printf '#!/bin/sh\nsu root -c "$(echo "$@")"\n' >/home/${USER:?}/mysudo \
  && chmod +x /home/${USER:?}/mysudo \
  && SUDO="/home/${USER:?}/mysudo" \
  && PKGINIT=true \
  && PKGADD="$SUDO apk add" \
  && PKGCLEAN=true \
  && HOST="x86_64-w64-mingw32" \
  && true


## make, install

As soon environ is configured, we can trigger the build:

true \
  && GIT_TAG="master" \
  && ZLIB_VERSION="1.2.11" \
  && WORKDIR="/home/${USER}/work" \
  && ZLIB_URL="https://downloads.sourceforge.net/project/libpng/zlib/${ZLIB_VERSION:?}/zlib-${ZLIB_VERSION:?}.tar.gz" \
  && ZLIB_LOCAL=zlib-${ZLIB_VERSION:?}.tgz \
  && INSTALL_ROOT="/usr/${HOST:-local}" \
  && true \
  && mkdir -p "${WORKDIR:?}" && cd "${WORKDIR:?}" \
  && ${PKGINIT:?} && ${PKGADD:?} $PKGS_TO_ADD \
  && if test -f "/var/tmp/${ZLIB_LOCAL:?}"; then true \
     && echo "[DEBUG] Already have \"/var/tmp/${ZLIB_LOCAL:?}\"" \
     ;else true \
     && echo "[DEBUG] Downloading \"/var/tmp/${ZLIB_LOCAL:?}\"" \
     && echo "[DEBUG]   from \"${ZLIB_URL:?}\"" \
     && curl -sSL "${ZLIB_URL:?}" -o "/var/tmp/${ZLIB_LOCAL:?}" \
     ;fi \
  && printf '\nMake zlib\n\n' \
  && (cd /tmp \
    && tar xzf "/var/tmp/${ZLIB_LOCAL:?}" \
    && export SRCDIR="/tmp/zlib-${ZLIB_VERSION:?}" \
    && mkdir ${SRCDIR:?}/build && cd ${SRCDIR:?} \
    && if test -n "$HOST"; then true \
       && export DESTDIR=./build BINARY_PATH=/bin INCLUDE_PATH=/include LIBRARY_PATH=/lib \
       && sed -i "s;^PREFIX =.\*\$;;" win32/Makefile.gcc \
       && make -e -j$(nproc) -fwin32/Makefile.gcc PREFIX=${HOST:?}- \
       && make -e -fwin32/Makefile.gcc install PREFIX=${HOST:?}- \
       && unset DESTDIR BINARY_PATH INCLUDE_PATH LIBRARY_PATH \
       ;else true \
       && ./configure --prefix=${SRCDIR:?}/build/ \
       && make -j$(nproc) && make install \
       ;fi \
    && cp README build/. \
    && (cd build \
      && rm -rf lib/pkgconfig \
      && find -type f -not -name MD5SUM -exec md5sum -b {} + > MD5SUM \
      && tar --owner=0 --group=0 -cz * > /var/tmp/zlib-${ZLIB_VERSION:?}-bin.tgz \
      ;) \
    && cd / && rm -rf /tmp/zlib-${ZLIB_VERSION:?} \
    && printf '\n\n  zlib Done :)\n\n' \
    ;) \
  && $SUDO tar -C "${INSTALL_ROOT:?}" -f /var/tmp/zlib-${ZLIB_VERSION:?}-bin.tgz -x include lib \
  && printf '\n  Build project itself\n\n' \
  && git clone --depth 42 --branch "${GIT_TAG:?}" https://github.com/hiddenalpha/DeflateAndInflate.git . \
  && git config advice.detachedHead false \
  && git checkout "${GIT_TAG:?}" \
  && ./configure \
  && if test -n "$HOST"; then true \
     && sed -Ei 's;^CC=gcc$;CC='"${HOST:?}"'-gcc;' Makefile \
     && sed -Ei 's;^(PREFIX=/usr/)local$;\1'"${HOST:?}"';' Makefile \
     ;fi \
  && make clean && make -j$(nproc) \
  && if test -z "$HOST"; then $SUDO make install; fi \
  && dirOfDistBundle="$(realpath dist)" \
  && printf '\n  SUCCESS  :)  Distribution bundle is ready in:\n\n  %s\n\n  Tip: Before pulling out your hair about how to get that archive out of\n       your qemu VM. STOP kluding around with silly tools and learn how\n       basic tools do the job perfectly fine:\n\n  ssh %s@localhost -p2222 -- sh -c '\''true && cd "%s" && tar c *'\'' | tar x\n\n  BTW: In addition 'deflate' and 'inflate' got installed and are ready-to-use.\n\n' "${dirOfDistBundle:?}" "$USER" "${dirOfDistBundle:?}" \
  && true





