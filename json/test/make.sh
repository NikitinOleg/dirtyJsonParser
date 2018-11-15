#!/bin/sh

DEBUD=1

PREFIX="/srv/resolute7_dev_e/"
CC="/srv/resolute7_dev_e/bin/gcc"

if [ "$CC" == "" ]; then
 CC=cc
fi

if [ "$PREFIX" == "" ]; then
 PREFIX="/usr/local"
fi

OUT=jstest
rm -f $OUT

LDFLAGS="-L$PREFIX/lib -Wl,-rpath=$PREFIX/lib -Wl,--dynamic-linker=$PREFIX/lib/ld-linux-x86-64.so.2"
#LDFLAGS="-L$PREFIX/lib"

if [ "$DEBUD" != "" ]; then
 ulimit -c unlimited
 __PARAM="-g -DDEBUG -O0 -fpic -Wall -Wextra -std=gnu99 -D_REENTRANT"
else
 __PARAM="-O2 -fpic -Wall -Wextra -std=gnu99 -D_REENTRANT"
fi

$CC ${__PARAM} \
	-lrt \
	-I${PREFIX}/include \
	$CPPFLAGS \
	$CFLAGS \
	$LDFLAGS \
	-o $OUT ./$OUT.c \
	../lib/json/json.c \
	../lib/string2/string2.c

chmod 755 ./$OUT
./$OUT

# профилирование на утечки памяти
export PPROF_PATH=/usr/local/bin/pprof
#export HEAP_CHECK_TEST_POINTER_ALIGNMENT=1
LD_PRELOAD=/usr/local/lib/libtcmalloc.so HEAPCHECK=normal ./$OUT
#LD_PRELOAD=/usr/local/lib/libtcmalloc.so HEAPCHECK=strict ./$OUT

#pprof ./src/bro "./bro.25523.net_run-end.heap" --inuse_objects --lines --heapcheck  --edgefraction=1e-10 --nodefraction=1e-10
#pprof ./jstest "./jstest.2427._main_-end.heap" --inuse_objects --lines --heapcheck  --edgefraction=1e-10 --nodefraction=1e-10
