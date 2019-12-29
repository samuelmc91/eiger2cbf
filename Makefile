# This Makefile was contributed by Harry Powell (MRC-LMB)
# Revised for NSLS-II LSBR cluster HJB, 17 Aug 16, 6 Jul 17

#EIGER2CBF_PREFIX ?=	/usr/local/crys-local/ccp4-7.0
EIGER2CBF_PREFIX ?=	/usr/local/crys-local
#EIGER2CBF_PREFIX ?=	$(PWD)
CBFLIB_LIB_INSTALL ?=	$(EIGER2CBF_PREFIX)/lib
CBFLIB_KIT ?=	$(EIGER2CBF_PREFIX)/cbflib
CBFINC ?=	$(EIGER2CBF_PREFIX)/include/cbflib
HDF5LIB ?=	$(EIGER2CBF_PREFIX)/lib
CC 	?=	gcc
CFLAGS	?=	-std=c99 -g -O3
#CFLAGS  ?=      -std=c99 -g -O0
#FGETLN  ?= 	-lbsd
FGETLN  ?= 	fgetln.c


all:	eiger2cbf eiger2cbf.so eiger2cbf-so-worker eiger2params \
	xsplambda2cbf


CBFLIB_URL ?=	http://github.com/yayahjb/cbflib.git
$(CBFLIB_KIT):  clone_cbflib_kit
	rm -rf $(CBFLIB_KIT):
	touch clone_cbflib_kit
	git clone $(CBFLIB_URL)
	touch $(CBFLIB_KIT)
	(export CBF_PREFIX=$(EIGER2CBF_PREFIX);cd $(CBFLIB_KIT);make install;)
	
	
eiger2cbf:  eiger2cbf.c lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c \
	$(CBFLIB_KIT)
	${CC} ${CFLAGS} -o eiger2cbf \
	-I${CBFINC} -Wl,-rpath-link=$(CBFLIB_LIB_INSTALL) \
	eiger2cbf.c \
        -Ilz4 \
	lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c \
	$(CBFLIB_LIB_INSTALL)/libcbf.so \
	$(HDF5LIB)/libhdf5_hl.so \
	$(HDF5LIB)/libhdf5.so \
	-lm -lpthread -lz -ldl

eiger2params:  eiger2params.c lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c fgetln.c \
	$(CBFLIB_KIT)
	${CC} ${CFLAGS} -o eiger2params \
	-I${CBFINC} -Wl,-rpath-link=$(CBFLIB_LIB_INSTALL) \
	eiger2params.c \
        -Ilz4 \
	lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c \
	$(HDF5LIB)/libhdf5_hl.so \
	$(HDF5LIB)/libhdf5.so \
	-lm $(FGETLN) -lpthread -lz -ldl

eiger2cbf-so-worker:	plugin-worker.c \
	lz4 lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c \
	$(CBFLIB_KIT)
	${CC} ${CFLAGS} -o eiger2cbf-so-worker \
	-I${CBFINC} -Wl,-rpath-link=$(CBFLIB_LIB_INSTALL) \
	plugin-worker.c \
	-Ilz4 lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c \
	$(CBFLIB_LIB_INSTALL)/libcbf.so \
	$(HDF5LIB)/libhdf5_hl.so \
	$(HDF5LIB)/libhdf5.so \
	-L$(HDF5LIB) -lpthread -lhdf5_hl -lhdf5 -lrt

eiger2cbf.so:	plugin.c \
	lz4 lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c \
	$(CBFLIB_KIT)
	${CC} ${CFLAGS} -o eiger2cbf.so -shared -fPIC \
	-I${CBFINC} -Wl,-rpath-link=$(CBFLIB_LIB_INSTALL) \
	plugin.c \
	-Ilz4 lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c \
	$(CBFLIB_LIB_INSTALL)/libcbf.so \
	$(HDF5LIB)/libhdf5_hl.so \
	$(HDF5LIB)/libhdf5.so \
	-L$(HDF5LIB) -lpthread -lhdf5_hl -lhdf5 -lrt

xsplambda2cbf:  xsplambda2cbf.c lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c \
	$(CBFLIB_KIT)
	${CC} ${CFLAGS} -o xsplambda2cbf \
	-I${CBFINC} -Wl,-rpath-link=$(CBFLIB_LIB_INSTALL) \
	xsplambda2cbf.c \
        -Ilz4 \
	lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c \
	$(CBFLIB_LIB_INSTALL)/libcbf.so \
	$(HDF5LIB)/libhdf5_hl.so \
	$(HDF5LIB)/libhdf5.so \
	-lm -lpthread -lz -ldl
	

$(EIGER2CBF_PREFIX)/bin:
	mkdir -p $(EIGER2CBF_PREFIX)/bin
	
$(EIGER2CBF_PREFIX)/lib:
	mkdir -p $(EIGER2CBF_PREFIX)/lib

install: all $(EIGER2CBF_PREFIX)/bin $(EIGER2CBF_PREFIX)/lib \
	eiger2cbf_par eiger2cbf_4t eiger2cbf eiger2params eiger2cbf.so eiger2cbf-so-worker \
	xsplambda2cbf $(CBFLIB_KIT)
	cp eiger2cbf $(EIGER2CBF_PREFIX)/bin/eiger2cbf
	chmod 755 $(EIGER2CBF_PREFIX)/bin/eiger2cbf
	cp eiger2params $(EIGER2CBF_PREFIX)/bin/eiger2params
	chmod 755 $(EIGER2CBF_PREFIX)/bin/eiger2params
	cp eiger2cbf_par $(EIGER2CBF_PREFIX)/bin/eiger2cbf_par
	chmod 755 $(EIGER2CBF_PREFIX)/bin/eiger2cbf_par
	cp eiger2cbf_4t $(EIGER2CBF_PREFIX)/bin/eiger2cbf_4t
	chmod 755 $(EIGER2CBF_PREFIX)/bin/eiger2cbf_4t
	cp eiger2cbf-so-worker $(EIGER2CBF_PREFIX)/bin/eiger2cbf-so-worker
	chmod 755 $(EIGER2CBF_PREFIX)/bin/eiger2cbf-so-worker
	cp eiger2cbf.so $(EIGER2CBF_PREFIX)/lib/eiger2cbf.so
	cp $(CBFLIB_KIT)/solib/lib* $(EIGER2CBF_PREFIX)/lib
	cp $(CBFLIB_KIT)/lib/lib* $(EIGER2CBF_PREFIX)/lib
	chmod 755 $(EIGER2CBF_PREFIX)/lib*
	cp eiger2cbf.so $(EIGER2CBF_PREFIX)/bin/eiger2cbf.so
	cp xsplambda2cbf $(EIGER2CBF_PREFIX)/bin/xsplambda2cbf
	chmod 755 $(EIGER2CBF_PREFIX)/bin/xsplambda2cbf

clean: 
	rm -rf *.o eiger2cbf eiger2cbf.so eiger2cbf-so-worker xsplambda2cbf $(CBFLIB_KIT)
	touch clone_cbflib_kit

