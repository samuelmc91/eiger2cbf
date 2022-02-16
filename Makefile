# This Makefile was contributed by Harry Powell (MRC-LMB)
# Revised for NSLS-II LSBR cluster HJB, 17 Aug 16, 6 Jul 17

EIGER2CBF_PREFIX ?=	/usr/local/crys-local
EIGER2CBF_BUILD =	$(PWD)
CBFLIB_KIT ?=	$(EIGER2CBF_BUILD)/cbflib
CBFINC ?=	$(EIGER2CBF_BUILD)/include/cbflib
HDF5LIB ?=	$(EIGER2CBF_BUILD)/lib
CC 	?=	gcc
CFLAGS	?=	-std=c99 -g -O3
#CFLAGS  ?=      -std=c99 -g -O0
#FGETLN  ?= 	-lbsd
FGETLN  ?= 	fgetln.c


all:	$(EIGER2CBF_BUILD)/bin/eiger2cbf \
	$(EIGER2CBF_BUILD)/lib/eiger2cbf.so \
	$(EIGER2CBF_BUILD)/bin/eiger2cbf-so-worker \
	$(EIGER2CBF_BUILD)/bin/eiger2params \
	$(EIGER2CBF_BUILD)/bin/xsplambda2cbf \
	$(EIGER2CBF_BUILD)/bin/eiger2cbf_par \
	$(EIGER2CBF_BUILD)/bin/eiger2cbf_4t


CBFLIB_URL ?=	http://github.com/samuelmc91/cbflib.git
$(CBFLIB_KIT):  clone_cbflib_kit
	echo EIGER2CBF_BUILD: $(EIGER2CBF_BUILD)
	echo CBFLIB_KIT: $(CBFLIB_KIT) 
	rm -rf $(CBFLIB_KIT)
	touch clone_cbflib_kit
	git clone $(CBFLIB_URL)
	touch $(CBFLIB_KIT)
	(export CBF_PREFIX=$(EIGER2CBF_BUILD);cd $(CBFLIB_KIT);make install;)
	
CBFLIB_KIT_INSTALL:	$(CBFLIB_KIT)
	(export CBF_PREFIX=$(EIGER2CBF_PREFIX);cd $(CBFLIB_KIT);make install;)
	
$(EIGER2CBF_BUILD)/bin/eiger2cbf:  eiger2cbf.c lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c \
	$(CBFLIB_KIT) $(EIGER2CBF_BUILD)/bin
	${CC} ${CFLAGS} -o $(EIGER2CBF_BUILD)/bin/eiger2cbf \
	-I${CBFINC} \
	eiger2cbf.c \
        -Ilz4 \
	lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c \
	$(CBFLIB_KIT)/lib/libcbf.so \
	$(HDF5LIB)/libhdf5_hl.so \
	$(HDF5LIB)/libhdf5.so \
	-lm -lpthread -lz -ldl

$(EIGER2CBF_BUILD)/bin/eiger2params:  eiger2params.c lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c fgetln.c \
	$(CBFLIB_KIT) $(EIGER2CBF_BUILD)/bin
	${CC} ${CFLAGS} -o $(EIGER2CBF_BUILD)/bin/eiger2params \
	-I${CBFINC} \
	eiger2params.c \
        -Ilz4 \
	lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c \
	$(CBFLIB_KIT)/lib/libcbf.so \
	$(HDF5LIB)/libhdf5_hl.so \
	$(HDF5LIB)/libhdf5.so \
	-lm $(FGETLN) -lpthread -lz -ldl

$(EIGER2CBF_BUILD)/bin/eiger2cbf-so-worker:	plugin-worker.c \
	lz4 lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c \
	$(CBFLIB_KIT) $(EIGER2CBF_BUILD)/bin
	${CC} ${CFLAGS} -o $(EIGER2CBF_BUILD)/bin/eiger2cbf-so-worker \
	-I${CBFINC} \
	plugin-worker.c \
	-Ilz4 lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c \
	$(CBFLIB_KIT)/lib/libcbf.so \
	$(HDF5LIB)/libhdf5_hl.so \
	$(HDF5LIB)/libhdf5.so \
	-L$(HDF5LIB) -lpthread -lhdf5_hl -lhdf5 -lrt

$(EIGER2CBF_BUILD)/lib/eiger2cbf.so:	plugin.c \
	lz4 lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c \
	$(CBFLIB_KIT) $(EIGER2CBF_BUILD)/lib
	${CC} ${CFLAGS} -o $(EIGER2CBF_BUILD)/lib/eiger2cbf.so -shared -fPIC \
	-I${CBFINC} \
	plugin.c \
	-Ilz4 lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c \
	$(CBFLIB_KIT)/lib/libcbf.so \
	$(HDF5LIB)/libhdf5_hl.so \
	$(HDF5LIB)/libhdf5.so \
	-L$(HDF5LIB) -lpthread -lhdf5_hl -lhdf5 -lrt

$(EIGER2CBF_BUILD)/bin/xsplambda2cbf:  xsplambda2cbf.c lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c \
	$(CBFLIB_KIT) $(EIGER2CBF_BUILD)/bin
	${CC} ${CFLAGS} -o $(EIGER2CBF_BUILD)/bin/xsplambda2cbf \
	-I${CBFINC} \
	xsplambda2cbf.c \
        -Ilz4 \
	lz4/lz4.c lz4/h5zlz4.c \
	bitshuffle/bshuf_h5filter.c \
	bitshuffle/bshuf_h5plugin.c \
	bitshuffle/bitshuffle.c \
	$(CBFLIB_KIT)/lib/libcbf.so \
	$(HDF5LIB)/libhdf5_hl.so \
	$(HDF5LIB)/libhdf5.so \
	-lm -lpthread -lz -ldl
	
$(EIGER2CBF_BUILD)/bin/eiger2cbf_par: $(EIGER2CBF_BUILD) eiger2cbf_par
	cp eiger2cbf_par $(EIGER2CBF_BUILD)/bin/eiger2cbf_par
	chmod 755 $(EIGER2CBF_BUILD)/bin/eiger2cbf_par

$(EIGER2CBF_BUILD)/bin/eiger2cbf_4t: $(EIGER2CBF_BUILD) eiger2cbf_4t
	cp eiger2cbf_4t $(EIGER2CBF_BUILD)/bin/eiger2cbf_4t
	chmod 755 $(EIGER2CBF_BUILD)/bin/eiger2cbf_4t

$(EIGER2CBF_PREFIX)/bin:
	mkdir -p $(EIGER2CBF_PREFIX)/bin
	
$(EIGER2CBF_PREFIX)/lib:
	mkdir -p $(EIGER2CBF_PREFIX)/lib

$(EIGER2CBF_BUILD)/bin:
	mkdir -p $(EIGER2CBF_BUILD)/bin
	
$(EIGER2CBF_BUILD)/lib:
	mkdir -p $(EIGER2CBF_BUILD)/lib

install: all $(EIGER2CBF_PREFIX)/bin $(EIGER2CBF_PREFIX)/lib \
	$(EIGER2CBF_BUILD)/bin $(EIGER2CBF_BUILD)/lib \
	$(EIGER2CBF_BUILD)/bin/eiger2cbf_par \
	$(EIGER2CBF_BUILD)/bin/eiger2cbf_4t \
	$(EIGER2CBF_BUILD)/bin/eiger2cbf \
	$(EIGER2CBF_BUILD)/bin/eiger2params \
	$(EIGER2CBF_BUILD)/lib/eiger2cbf.so \
	$(EIGER2CBF_BUILD)/bin/eiger2cbf-so-worker \
	$(EIGER2CBF_BUILD)/bin/xsplambda2cbf $(CBFLIB_KIT) $(CBFLIB_KIT_INSTALL)
	cp $(EIGER2CBF_BUILD)/bin/eiger2cbf $(EIGER2CBF_PREFIX)/bin/eiger2cbf
	chmod 755 $(EIGER2CBF_PREFIX)/bin/eiger2cbf
	cp $(EIGER2CBF_BUILD)/bin/eiger2params $(EIGER2CBF_PREFIX)/bin/eiger2params
	chmod 755 $(EIGER2CBF_PREFIX)/bin/eiger2params
	cp $(EIGER2CBF_BUILD)/bin/eiger2cbf_par $(EIGER2CBF_PREFIX)/bin/eiger2cbf_par
	chmod 755 $(EIGER2CBF_PREFIX)/bin/eiger2cbf_par
	cp $(EIGER2CBF_BUILD)/bin/eiger2cbf_4t $(EIGER2CBF_PREFIX)/bin/eiger2cbf_4t
	chmod 755 $(EIGER2CBF_PREFIX)/bin/eiger2cbf_4t
	cp $(EIGER2CBF_BUILD)/bin/eiger2cbf-so-worker $(EIGER2CBF_PREFIX)/bin/eiger2cbf-so-worker
	chmod 755 $(EIGER2CBF_PREFIX)/bin/eiger2cbf-so-worker
	cp $(EIGER2CBF_BUILD)/lib/eiger2cbf.so $(EIGER2CBF_PREFIX)/lib/eiger2cbf.so
	cp $(CBFLIB_KIT)/solib/lib* $(EIGER2CBF_PREFIX)/lib
	cp $(CBFLIB_KIT)/lib/lib* $(EIGER2CBF_PREFIX)/lib
	chmod 755 $(EIGER2CBF_PREFIX)/lib*
	cp $(EIGER2CBF_BUILD)/lib/eiger2cbf.so $(EIGER2CBF_PREFIX)/bin/eiger2cbf.so
	cp $(EIGER2CBF_BUILD)/bin/xsplambda2cbf $(EIGER2CBF_PREFIX)/bin/xsplambda2cbf
	chmod 755 $(EIGER2CBF_PREFIX)/bin/xsplambda2cbf

clean: 
	rm -rf *.o 
	rm -rf $(EIGER2CBF_BUILD)/bin/eiger2cbf
	rm -rf $(EIGER2CBF_BUILD)/lib/eiger2cbf.so
	rm -rf $(EIGER2CBF_BUILD)/bin/eiger2prams
	rm -rf $(EIGER2CBF_BUILD)/bin/eiger2cbf-so-worker
	rm -rf $(EIGER2CBF_BUILD)/bin/xsplambda2cbf 
	rm -rf $(EIGER2CBF_BUILD)/bin/eiger2cbf_par
	rm -rf $(EIGER2CBF_BUILD)/bin/eiger2cbf_4t

distclean:	clean
	rm -rf $(CBFLIB_KIT)
	touch clone_cbflib_kit
	rm -rf $(EIGER2CBF_BUILD)/bin
	rm -rf $(EIGER2CBF_BUILD)/lib

