# This Makefile was contributed by Harry Powell (MRC-LMB)
# Revised for NSLS-II LSBR cluster HJB, 17 Aug 16

PREFIX ?=	/usr/local/crys-prod
CBFLIB ?=	$(PREFIX)/lib
CBFINC ?=	$(PREFIX)/include/cbflib
HDF5LIB ?=	$(PREFIX)/lib
CC=gcc -O3

all:	eiger2cbf eiger2cbf.so eiger2cbf-so-worker
	
eiger2cbf:  eiger2cbf.c lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c 
	${CC} -std=c99 -o eiger2cbf -g \
	-I${CBFINC} \
	eiger2cbf.c \
        -Ilz4 \
	lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c \
	${CBFLIB}/libcbf.a \
	${HDF5LIB}/libhdf5_hl.a \
	${HDF5LIB}/libhdf5.a \
	-lm -lpthread -lz -ldl

eiger2cbf-so-worker:	plugin-worker.c \
	lz4 lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c
	${CC} -std=c99 -o eiger2cbf-so-worker -g -O3 \
	-I${CBFINC} \
	plugin-worker.c \
	-Ilz4 lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c \
	-L${HDF5LIB} -lpthread -lhdf5_hl -lhdf5 -lrt

eiger2cbf.so:	plugin.c \
	lz4 lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c
	${CC} -std=c99 -o eiger2cbf.so -shared -fPIC -g -O3 \
	-I${CBFINC} \
	plugin.c \
	-Ilz4 lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c \
	-L${HDF5LIB} -lpthread -lhdf5_hl -lhdf5 -lrt
	

install: all
	cp eiger2cbf $(PREFIX)/bin/eiger2cbf
	chmod 755 $(PREFIX)/bin/eiger2cbf
	cp eiger2cbf-so-worker $(PREFIX)/bin/eiger2cbf-so-worker
	chmod 755 $(PREFIX)/bin/eiger2cbf-so-worker
	cp eiger2cbf.so $(PREFIX)/lib/eiger2cbf.so

clean: 
	rm -f *.o eiger2cbf eiger2cbf.so eiger2cbf-so-worker

