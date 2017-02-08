# This Makefile was contributed by Harry Powell (MRC-LMB)
# Revised for NSLS-II LSBR cluster HJB, 17 Aug 16

PREFIX=/usr/local/crys-prod
CBFLIB=$(PREFIX)/lib
CBFINC=$(PREFIX)/include/cbflib
HDF5LIB=$(PREFIX)/lib
CC=gcc -O3
all:	
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

install: all
	cp eiger2cbf $(PREFIX)/bin/eiger2cbf
	chmod 755 $(PREFIX)/bin/eiger2cbf

clean: 
	rm -f *.o eiger2cbf
