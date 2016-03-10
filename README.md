eiger2cbf: EIGER HDF5 to miniCBF converter
==========================================

eiger2cbf is a simple program that converts diffraction images from
EIGER in the HDF5 format to the miniCBF format. This program is intended
to be used with MOSFLM. You do not need this program to process images
with DIALS (nightly build as of 2016 March, but not the one comes with 
CCP4 7.0).

Installation
------------

You can get static-linked binaries from Harry Powell's website.

-   [Linux](http://www.mrc-lmb.cam.ac.uk/harry/imosflm/ver721/downloads/miniCBF-Linux)
-   [Mac OS X](http://www.mrc-lmb.cam.ac.uk/harry/imosflm/ver721/downloads/miniCBF-OSX)

To build yourself, you should edit Makefile and run `make`.

Running eiger2cbf without command line options shows a help.

```
$ eiger2cbf
EIGER HDF5 to CBF converter (build 160309)
 Written by Takanori Nakane

Usage:
  ./eiger2cbf filename.h5           -- get number of frames
  ./eiger2cbf filename.h5 N out.cbf -- write N-th frame to out.cbf
  ./eiger2cbf filename.h5 N         -- write N-th frame to STDOUT
  ./eiger2cbf filename.h5 N:M   out -- write N to M-th frames to outNNNNNN.cbf
  N is 1-indexed. The file should be "master" h5.
```

H5ToXds compatibility
---------------------

The command line option is almost the same as H5ToXds from
Dectris. Actually, you can use eiger2cbf instead of H5ToXds. This
might be useful on Mac OS X because H5ToXds is provided only for Linux
(as of 2016 Feb). In this case, you may want to create a following
wrapper script named H5ToXds.

```
#!/bin/sh

# redirect STDERR to /dev/null so that it does not disturb console outputs from other programs.
/path/to/eiger2cbf %@ 2>/dev/null 
```

Alternative choices
-------------------

H5ToXds from Dectris and hdf2mini-cbf from Global Phasing have similar
capacities. The advantage of eiger2cbf is that it is open-source, free
software. The others are closed-source.

Support
-------

If the program does not work fine, feel free to contact me
from the "Issues" page. If you can give me test datasets, it 
would be very helpful!

Acknowledgements
----------------

I thank Harry Powell for writing Makefile and fixing compiler warnings.
I thank those who gave me test datasets.

LICENSE
-------

BSD license. 

eiger2cbf includes [bitshuffle](https://github.com/kiyo-masui/bitshuffle) and 
[lz4 HDF5 plugin](https://github.com/dectris/HDF5Plugin), both of which are also
licensed under the BSD license. See LICENSE files in bitshuffle and lz4
directories.

eiger2cbf also depends on [HDF5 library](https://www.hdfgroup.org/HDF5/) and 
[CBFlib](http://www.bernstein-plus-sons.com/software/CBF/). 
See LICENSE.HDF5 and LICENSE.CBFlib. Both are not included in this source distribution.
