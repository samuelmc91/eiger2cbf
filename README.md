eiger2cbf: EIGER HDF5 to miniCBF converter
==========================================

eiger2cbf is a simple program that converts diffraction images from
EIGER in the HDF5 format to the miniCBF format. This program is intended
to be used with MOSFLM. You do *not* need this program to process images
with [DIALS](http://dials.diamond.ac.uk/); DIALS can process HDF5 images 
directly (nightly build as of 2016 March, but not the version that comes with CCP4 7.0.0).

Installation
------------

You can get static-linked binaries built by Harry Powell from his website.
See the bottom of the
[iMosflm download page](http://www.mrc-lmb.cam.ac.uk/harry/imosflm/ver721/downloads.html#miniCBF).

To build yourself, you should edit Makefile and run `make`.

Running eiger2cbf without command line options shows a help.

```
$ eiger2cbf
EIGER HDF5 to CBF converter (build 160310)
 Written by Takanori Nakane

Usage:
  ./eiger2cbf filename.h5           -- get number of frames
  ./eiger2cbf filename.h5 N out.cbf -- write N-th frame to out.cbf
  ./eiger2cbf filename.h5 N         -- write N-th frame to STDOUT
  ./eiger2cbf filename.h5 N:M   out -- write N to M-th frames to outNNNNNN.cbf
  N starts from 1. The file should be "master" h5.
```

H5ToXds compatibility
---------------------

The command line option is almost the same as H5ToXds from
Dectris. Actually, you can use eiger2cbf instead of H5ToXds. This
might be useful on Mac OS X because H5ToXds is provided only for Linux
(as of 2016 Feb). In this case, you may want to create the following
wrapper script named H5ToXds.

```bash
#!/bin/sh

# redirect STDERR to /dev/null so that it does not disturb console outputs from other programs.
/path/to/eiger2cbf $@ 2>/dev/null
```

Alternative choices
-------------------

H5ToXds from Dectris and hdf2mini-cbf from Global Phasing have similar
capacities. The advantage of eiger2cbf is that it is open-source, free
software. The others are closed-source. MOSFLM cannot process outputs
from H5ToXds because they lack essential headers.

Conversion details
------------------

The program assumes that the pixel mask has already been applied to the images.
The values of the invalid pixels are converted to -1 (as in Pilatus).

The following metadata are converted from the master h5.

-   Detector name and serial number
-   Pixel size
-   Sensor thickness
-   Countrate correction cutoff
-   Wavelength
-   Detector distance
-   Beam center
-   Exposure time
-   Exposure period
-   Start angle
-   Angle increment

Unfortunately, some of them are missing in some datasets. In these cases, 
the converter outputs zero or standard (common) values. See the console output.
In the future, we will add command-line options to supply metadata.

Warning: currently, we assume the rotation is around the 'omega' axis and
two-theta is 0. Send me test data if you need support for more complex geometry.

Support
-------

The program has been tested on the following datasets.

-   EIGER 16M at SLS X06SA (2016 Feb)
-   EIGER 4M at ESRF ID30A-3
-   EIGER 4M and 16M sample datasets on Dectris website (downloaded in 2015 Nov)

As there are many beamline and firmware-version specific
"dialects", eiger2cbf might not work on your dataset.
If the program does not work well, feel free to contact me
from the "Issues" page. If you can give me test datasets, it 
would be very helpful!

Acknowledgements
----------------

I thank [Harry Powell](http://www.mrc-lmb.cam.ac.uk/harry/) for writing Makefile,
fixing compiler warnings and building & hosting binaries. 
I thank those who gave me feedback and test datasets.

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
