eiger2cbf: EIGER HDF5 to miniCBF converter
==========================================

eiger2cbf is a simple program that converts diffraction images from
EIGER in the HDF5 format to miniCBF formats. This program is intended
to be used with MOSFLM. You do not need this program to process images
by DIALS (version 1.1 and later).

Installation
------------

You can get static-linked binaries from Harry Powell's website.
To build yourself, you should edit Makefile and run "make".

Running eiger2cbf without command line options shows a help.

sh'''
$ eiger2cbf

'''

H5ToXds compatibility
---------------------

The command line option is almost the same as H5ToXds from
Dectris. Actually, you can use eiger2cbf instead of H5ToXds. This
might be useful on Mac OS X because H5ToXds is provided only for Linux
(as of 2016 Feb). In this case, you may want to put the following
script.

sh'''
#!/bin/sh

# redirect STDERR to /dev/null so that it does not disturb console outputs from other programs.
/path/to/eiger2cbf %@ 2>/dev/null 
'''

Alternative choices
-------------------

H5ToXds from Dectris and hdf2mini-cbf from Global Phasing have similar
capacities. The advantage of eiger2cbf is that it is open-source, free
software. The others are closed-source.

LICENSE
-------

BSD license. 

eiger2cbf includes bitshuffle and lz4 plugin, both of which are also licensed under BSD license. See LICENSE files in bitshuffle and lz4 directories.

eiger2cbf also depends on HDF5 library and CBFlib.
