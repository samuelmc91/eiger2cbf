# This Makefile was contributed by Harry Powell (MRC-LMB)
# Revised for NSLS-II LSBR cluster HJB, 17 Aug 16, 6 Jul 17

PREFIX ?=	/usr/local/crys-local/ccp4-7.0
CBFLIB ?=	$(PREFIX)/lib
CBFINC ?=	$(PREFIX)/include/cbflib
HDF5LIB ?=	$(PREFIX)/lib
CC 	?=	gcc
CFLAGS	?=	-std=c99 -g -O3
#CFLAGS  ?=      -std=c99 -g -O0
#FGETLN  ?= 	-lbsd
FGETLN  ?= 	fgetln.c

all:	eiger2cbf eiger2cbf.so eiger2cbf-so-worker eiger2params \
	xsplambda2cbf
	
eiger2cbf:  eiger2cbf.c lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c 
	${CC} ${CFLAGS} -o eiger2cbf \
	-I${CBFINC} \
	eiger2cbf.c \
        -Ilz4 \
	lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c \
	${CBFLIB}/libcbf.so \
	${HDF5LIB}/libhdf5_hl.so \
	${HDF5LIB}/libhdf5.so \
	-lm -lpthread -lz -ldl

eiger2params:  eiger2params.c lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c fgetln.c 
	${CC} ${CFLAGS} -o eiger2params \
	-I${CBFINC} \
	eiger2params.c \
        -Ilz4 \
	lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c \
	${HDF5LIB}/libhdf5_hl.so \
	${HDF5LIB}/libhdf5.so \
	-lm $(FGETLN) -lpthread -lz -ldl

eiger2cbf-so-worker:	plugin-worker.c \
	lz4 lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c
	${CC} ${CFLAGS} -o eiger2cbf-so-worker \
	-I${CBFINC} \
	plugin-worker.c \
	-Ilz4 lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c \
	${CBFLIB}/libcbf.so \
	${HDF5LIB}/libhdf5_hl.so \
	${HDF5LIB}/libhdf5.so \
	-L${HDF5LIB} -lpthread -lhdf5_hl -lhdf5 -lrt

eiger2cbf.so:	plugin.c \
	lz4 lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c
	${CC} ${CFLAGS} -o eiger2cbf.so -shared -fPIC \
	-I${CBFINC} \
	plugin.c \
	-Ilz4 lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c \
	${CBFLIB}/libcbf.so \
	${HDF5LIB}/libhdf5_hl.so \
	${HDF5LIB}/libhdf5.so \
	-L${HDF5LIB} -lpthread -lhdf5_hl -lhdf5 -lrt

xsplambda2cbf:  xsplambda2cbf.c lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c 
	${CC} ${CFLAGS} -o xsplambda2cbf \
	-I${CBFINC} \
	xsplambda2cbf.c \
        -Ilz4 \
	lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c \
	${CBFLIB}/libcbf.so \
	${HDF5LIB}/libhdf5_hl.so \
	${HDF5LIB}/libhdf5.so \
	-lm -lpthread -lz -ldl
	

$(PREFIX)/bin:
	mkdir -p $(PREFIX)/bin
	
$(PREFIX)/lib:
	mkdir -p $(PREFIX)/lib

install: all $(PREFIX)/bin $(PREFIX)/lib \
	eiger2cbf_par eiger2cbf_4t eiger2cbf eiger2params eiger2cbf.so eiger2cbf-so-worker \
	xsplambda2cbf
	cp eiger2cbf $(PREFIX)/bin/eiger2cbf
	chmod 755 $(PREFIX)/bin/eiger2cbf
	cp eiger2params $(PREFIX)/bin/eiger2params
	chmod 755 $(PREFIX)/bin/eiger2params
	cp eiger2cbf_par $(PREFIX)/bin/eiger2cbf_par
	chmod 755 $(PREFIX)/bin/eiger2cbf_par
	cp eiger2cbf_4t $(PREFIX)/bin/eiger2cbf_4t
	chmod 755 $(PREFIX)/bin/eiger2cbf_4t
	cp eiger2cbf-so-worker $(PREFIX)/bin/eiger2cbf-so-worker
	chmod 755 $(PREFIX)/bin/eiger2cbf-so-worker
	cp eiger2cbf.so $(PREFIX)/lib/eiger2cbf.so
	cp eiger2cbf.so $(PREFIX)/bin/eiger2cbf.so
	cp xsplambda2cbf $(PREFIX)/bin/xsplambda2cbf
	chmod 755 $(PREFIX)/bin/xsplambda2cbf

clean: 
	rm -f *.o eiger2cbf eiger2cbf.so eiger2cbf-so-worker xsplambda2cbf

