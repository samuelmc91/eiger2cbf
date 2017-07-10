/*
EIGER HDF5 to PARAMETERS converter
 derived by Herbert J. Bernstein
 from eiger2cbf
 Written by Takanori Nakane

To build:

Linux

gcc -std=c99 -o eiger2params -g \
 -I$HOME/prog/dials/modules/cbflib/include \
 -L$HOME/prog/dials/build/lib -Ilz4 \
 eiger2params.c \
 lz4/lz4.c lz4/h5zlz4.c \
 bitshuffle/bshuf_h5filter.c \
 bitshuffle/bshuf_h5plugin.c \
 bitshuffle/bitshuffle.c \
 /usr/lib/x86_64-linux-gnu/libhdf5_hl.a \
 /usr/lib/x86_64-linux-gnu/libhdf5.a \
 -lm -lpthread -lz -ldl

Mac OS X

gcc -std=c99 -o eiger2params -g \
 -ICBFlib-0.9.5.2/include -Ilz4 \
 eiger2params.c \
 lz4/lz4.c lz4/h5zlz4.c \
 bitshuffle/bshuf_h5filter.c \
 bitshuffle/bshuf_h5plugin.c \
 bitshuffle/bitshuffle.c \
 $HOME/local/lib/libhdf5_hl.a \
 $HOME/local/lib/libhdf5.a \
 -lm -lpthread -lz -ldl

*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "hdf5.h"
#include "hdf5_hl.h"


extern const H5Z_class2_t H5Z_LZ4;
extern const H5Z_class2_t bshuf_H5Filter;
void register_filters() {
  H5Zregister(&H5Z_LZ4);
  H5Zregister(&bshuf_H5Filter);
}

void usage( int argc, char **argv ) {
    printf("Usage:\n");
    printf("  %s [options] filename.h5           -- write parameters to STDOUT\n", argv[0]);
    printf("  The file should be \"master\" h5.\n");
    printf("  options:\n");
    printf("    -h or --help                     -- print this message\n");
    printf("    -v or --verbose                  -- provide more detail in output\n"); 
    printf("    --beam-center beamx,beamy        -- new beam center in pixels\n");
    printf("    --nimages images                 -- override the number of images\n");
    printf("    --dozor-dat                      -- format as a dozor data file\n");
    printf("    --dozor-cli                      -- format as dozor cli options\n");
    return;  
}




int main(int argc, char **argv) {
  char header[4096] = {};
  int xpixels = -1, ypixels = -1; 
  double beamx = -10000000., beamy = -10000000.;
  long ibeamx, ibeamy; 
  int nimages = -1, depth = -1, countrate_cutoff = -1;
  int ntrigger = -1;
  double nbeamx = -10000000., nbeamy = -10000000.;
  long nnimages = -1L;
  int from = -1, to = -1;
  int ret;
  int retfromto;
  int usage_printed = 0;
  int optcount = 0;      /* count of command line options */
  int verbose = 0;       /* verbose mode */
  int new_beam_cent = 0; /* new beam center provided */
  int new_nimages = 0;   /* new number of images provided */
  int dozor_dat = 0;     /* flag for dozor dat file output */
  int dozor_cli = 0;     /* flag for dozor cli options output */
  char * param_prologue="";
  char * param_epilogue="\n";
  int ii;
  char* endptr;
  char* fndptr;
  double pixelsize = -1, wavelength = -1, distance = -1, count_time = -1, 
    frame_time = -1, osc_width = -1, osc_start = -9999, thickness = -1;
  double frac_polar = -1;
  double polar[2]={-1,-1};
  double spolar[4]={-1,-1,-1,-1};
  char detector_sn[256] = {}, description[256] = {}, version[256] = {};
  char * detector;  

  hid_t hdf;


  for (ii=1; ii < argc; ii++) {
    if (!strcmp(argv[ii],"-h") || !strcmp(argv[ii],"--help")) {
      usage (argc, argv);
      optcount ++;
      usage_printed ++;
    } else if (!strcmp(argv[ii],"-v") || !strcmp(argv[ii],"--verbose")) {
      fprintf(stderr, "EIGER HDF5 to PARAMETERS converter (version 170707)\n");
      fprintf(stderr, " derived by Herbert J. Bernstein, yayahjb@gmail.com from\n");
      fprintf(stderr, "EIGER HDF5 to CBF converter (version 160929)\n");
      fprintf(stderr, " written by Takanori Nakane\n");
      fprintf(stderr, " see https://github.com/biochem-fan/eiger2cbf for details.\n\n");
      verbose = 1;
      optcount ++;
    } else if (!strcmp(argv[ii],"--beam-center")) {
      new_beam_cent = 1;
      optcount ++;
      if (ii  < argc-1) {
        ii++;
        optcount ++;
        nbeamx=strtod(argv[ii],&endptr);
        if (!endptr || endptr==argv[ii] || *endptr!=',') {
          new_beam_cent = 0;
          fprintf(stderr, "eiger2cbf error:  --beam-center provided without two comma-separated values; ignored\n");
          usage(argc, argv);
          usage_printed  ++;
        } else {
          endptr++;
          nbeamy=strtod(endptr,&fndptr);
          if (!fndptr || fndptr==endptr|| *fndptr!='\0') {
              new_beam_cent = 0;
              fprintf(stderr, "eiger2cbf error:  --beam-center provided without two comma-separated values; ignored\n");
              usage(argc, argv);
              usage_printed  ++;         }
        }     
      } else {
        fprintf(stderr, "eiger2cbf error:  --beam-center provided without a value; ignored\n");
        usage(argc, argv);
        usage_printed  ++;
        new_beam_cent = 0;
      }
    } else if (!strcmp(argv[ii],"--nimages")) {
      new_nimages = 1;
      optcount ++;
      if (ii < argc-1) {
        ii++;
        optcount ++;
        nnimages=strtol(argv[ii],&endptr,10);
        if (!endptr || endptr==argv[ii]) {
          new_nimages = 0;
          fprintf(stderr, "eiger2cbf error: --nimages invalid value; ignored\n");
          usage(argc,argv);
          usage_printed++;
        }
      } else {
        fprintf(stderr, "eiger2cbf error:  --nimages provided without a value; ignored\n");
        usage(argc, argv);
        usage_printed  ++;
        new_nimages = 0;
      }
    } else if (!strcmp(argv[ii],"--dozor-dat")) {
      dozor_dat = 1;
      dozor_cli = 0;
      param_prologue = "";
      param_epilogue = "\n";
      optcount++;
    } else if (!strcmp(argv[ii],"--dozor-cli")) {
      dozor_dat = 0;
      dozor_cli = 1;
      param_prologue = "--";
      param_epilogue = "  ";  
      optcount++;
    } else break;
  }

  if (argc-optcount <= 1 || argc-optcount  >= 3) {
    if (usage_printed == 0) usage (argc, argv); 
    return -1;
  }

  register_filters();

  if (dozor_dat == 0 && dozor_cli == 0) dozor_dat = 1;

  hdf = H5Fopen(argv[1+optcount], H5F_ACC_RDONLY, H5P_DEFAULT);
  if (hdf < 0) {
    fprintf(stderr, "eiger2cbf error: failed to open file %s\n", argv[1+optcount]);
    return -1;
  }

  H5LTread_dataset_int(hdf, "/entry/instrument/detector/detectorSpecific/nimages", &nimages);
  H5LTread_dataset_int(hdf, "/entry/instrument/detector/detectorSpecific/ntrigger", &ntrigger);
  if (nimages == 1 && ntrigger > 1) {
    fprintf(stderr, "eiger2cbf warning: nimages == 1 and ntrigger == %d \n", ntrigger);
    fprintf(stderr, "eiger2cbf warning: setting nimages to ntrigger \n");
    nimages = ntrigger;
  }
  if (new_nimages && nnimages > 0) {
    nimages = nnimages;
    fprintf(stderr, "eiger2cbf warning: setting nimages to %d \n",nimages);
  }
  
  
  H5Eset_auto(0, NULL, NULL); // Comment out this line for debugging.

  if (verbose) fprintf(stderr, "Metadata in HDF5:\n");
  detector = "unknown_detector";
  H5LTread_dataset_string(hdf, "/entry/instrument/detector/description", description);
   if (verbose) fprintf(stderr, " /entry/instrument/detector/description = %s\n", description);
   if (strcasestr(description,"Eiger")) {
     if (strcasestr(description,"16m")) {
       detector = "eiger16m";
     } else if (strcasestr(description,"9m")) {
       detector = "eiger9m";
     } else if (strcasestr(description,"4m")) {
       detector = "eiger4m";
     } else if (strcasestr(description,"1m")) {
       detector = "eiger1m";
     } else if (strcasestr(description,"500k")) {
       detector = "eiger500k";
     } else detector = description;
   } else if (strcasestr(description,"Pilatus")) {
     if (strcasestr(description,"6m")) {
       detector = "pilatus6m";
     } else if (strcasestr(description,"2m")) {
       detector = "pilatus2m";
     } else if (strcasestr(description,"1m")) {
       detector = "pilatus1m";
     } else if (strcasestr(description,"300kw")) {
       detector = "pilatus300kw";
     } else if (strcasestr(description,"300k")) {
       detector = "pliatus300k";
     } else if (strcasestr(description,"200kw")) {
       detector = "pilatus200k";
     } else if (strcasestr(description,"100k")) {
       detector = "pliatus100k";
     } else detector = description;
   } else {
     detector = description;
   }
  H5LTread_dataset_string(hdf, "/entry/instrument/detector/detector_number", detector_sn);
   if (verbose) fprintf(stderr, " /entry/instrument/detector/detector_number = %s\n", detector_sn);
  H5LTread_dataset_string(hdf, "/entry/instrument/detector/detectorSpecific/software_version", version);
   if (verbose)fprintf(stderr, " /entry/instrument/detector/detectorSpecific/software_version = %s\n", version);
  H5LTread_dataset_int(hdf, "/entry/instrument/detector/bit_depth_image", &depth);
  if (depth > 0) {
     if (verbose) fprintf(stderr, " /entry/instrument/detector/bit_depth_image = %d\n", depth);
  } else {
    fprintf(stderr, " WARNING: /entry/instrument/detector/bit_depth_image is not avaialble. We assume 16 bit.\n");
    depth = 16;
  }
  unsigned int error_val = (unsigned int)(((unsigned long long)1 << depth) - 1);

  // Saturation value
  // Firmware >= 1.5
  H5LTread_dataset_int(hdf, "/entry/instrument/detector/detectorSpecific/saturation_value", &countrate_cutoff);
  if (countrate_cutoff > 0) {
     if (verbose) fprintf(stderr, " /entry/instrument/detector/detectorSpecific/saturation_value = %d\n", countrate_cutoff);
  } else {
    // Firmware >= 1.4
     if (verbose) fprintf(stderr, "  /entry/instrument/detector/detectorSpecific/saturation_value not present. Trying another place.\n");
    H5LTread_dataset_int(hdf, "/entry/instrument/detector/detectorSpecific/countrate_correction_count_cutoff", &countrate_cutoff);
    if (countrate_cutoff > 0) {
       if (verbose) fprintf(stderr, " /entry/instrument/detector/detectorSpecific/countrate_correction_count_cutoff = %d\n", countrate_cutoff);
      countrate_cutoff++;
    } else {
      fprintf(stderr, "  /entry/instrument/detector/detectorSpecific/countrate_correction_count_cutoff not present. Trying another place.\n");
      // < 1.4
      H5LTread_dataset_int(hdf, "/entry/instrument/detector/detectorSpecific/detectorModule_000/countrate_correction_count_cutoff", &countrate_cutoff);
      if (countrate_cutoff > 0) {
        fprintf(stderr, " /entry/instrument/detector/detectorSpecific/detectorModule_000/countrate_correction_count_cutoff = %d\n", countrate_cutoff);
	fprintf(stderr, "  WARNING: The use of this field is not recommended now.\n");
	fprintf(stderr, "  You might want to change the OVERLOAD setting in your subsequent processing.\n");
        countrate_cutoff++;
      } else {
        fprintf(stderr, "  /entry/instrument/detector/detectorSpecific/detectorModule_000/countrate_correction_count_cutoff not present.\n");
        countrate_cutoff = error_val - 1;
        fprintf(stderr, "  As a last resort, we will put an arbitrary large number (%d) in the header.\n", countrate_cutoff);
        fprintf(stderr, "  You might want to change the OVERLOAD setting in your subsequent processing.\n");
      }
    }
  }

  H5LTread_dataset_double(hdf, "/entry/instrument/detector/sensor_thickness", &thickness); // in m 
  if (thickness > 0) {
     if (verbose) fprintf(stderr, " /entry/instrument/detector/sensor_thickness = %f (um)\n", thickness * 1E6);
  } else {
    thickness = 450E-6;
     if (verbose) fprintf(stderr, " /entry/instrument/detector/sensor_thickness is not avaialble. We assume it is %f um\n", thickness * 1E6);
  }
  H5LTread_dataset_int(hdf, "/entry/instrument/detector/detectorSpecific/x_pixels_in_detector", &xpixels);
  H5LTread_dataset_int(hdf, "/entry/instrument/detector/detectorSpecific/y_pixels_in_detector", &ypixels);
   if (verbose) fprintf(stderr, " /entry/instrument/detector/detectorSpecific/{x,y}_pixels_in_detector = (%d, %d) (px)\n",
	  xpixels, ypixels);
  H5LTread_dataset_double(hdf, "/entry/instrument/detector/beam_center_x", &beamx);
  H5LTread_dataset_double(hdf, "/entry/instrument/detector/beam_center_y", &beamy);
   if (verbose) fprintf(stderr, " /entry/instrument/detector/beam_center_{x,y} = (%.2f, %.2f) (px)\n", new_beam_cent?nbeamx:beamx, new_beam_cent?nbeamy:beamy);
  H5LTread_dataset_double(hdf, "/entry/instrument/detector/count_time", &count_time); // in s
   if (verbose) fprintf(stderr, " /entry/instrument/detector/count_time = %f (sec)\n", count_time);
  H5LTread_dataset_double(hdf, "/entry/instrument/detector/frame_time", &frame_time); // in s
   if (verbose) fprintf(stderr, " /entry/instrument/detector/frame_time = %f (sec)\n", frame_time);
  H5LTread_dataset_double(hdf, "/entry/instrument/detector/x_pixel_size", &pixelsize); // in m
   if (verbose) fprintf(stderr, " /entry/instrument/detector/x_pixel_size = %f (m)\n", pixelsize);

  // Detector distance

  H5LTread_dataset_double(hdf, "/entry/instrument/detector/distance", &distance); // Firmware >= 1.7
  if (distance > 0) {
     if (verbose) fprintf(stderr, " /entry/instrument/detector/distance = %f (m)\n", distance);
  } else {
     if (verbose) fprintf(stderr, "  /entry/instrument/detector/distance not present. Trying another place.\n");

    H5LTread_dataset_double(hdf, "/entry/instrument/detector/detector_distance", &distance); // Firmware< 1.7
    if (distance > 0) {
       if (verbose) fprintf(stderr, " /entry/instrument/detector/detector_distance = %f (m)\n", distance);
 
   } else {
       if (verbose) fprintf(stderr, "  /entry/instrument/detector/detector_distance not present.\n");
       if (verbose) fprintf(stderr, " WARNING: detector distance was not defined! \"Detector distance\" field in the output is set to -1.\n");
    }
  }


  // Polarization
  frac_polar = -1.;
  H5LTread_dataset_double(hdf, "/entry/sample/beam/incident_polarisation_stokes_average", spolar);
  if (spolar[0] > 0.) {
     if (verbose) fprintf(stderr, " /entry/sample/beam/incident_polarisation_stokes_average = [%g,%g,%g,%g] (W/m^2)\n", 
          spolar[0], spolar[1], spolar[2], spolar[3]); 
     frac_polar = -1.;
     if (spolar[0] > 0. && sqrt(spolar[1]*spolar[1]+spolar[2]*spolar[2]) >= spolar[0]) {
       frac_polar = spolar[0]/sqrt(spolar[1]*spolar[1]+spolar[2]*spolar[2]);
     }
  } else {
     spolar[0] = spolar[1] = spolar[2] = spolar[3] = -1.;
     frac_polar = -1.;
  }
  if (frac_polar < 0.) {
     H5LTread_dataset_double(hdf, "/entry/sample/beam/incident_polarisation_stokes", spolar);
     if (spolar[0] >= 0.) {
       if (verbose) fprintf(stderr, " /entry/sample/beam/incident_polarisation_stokes[0] = [%g,%g,%g,%g] (W/m^2)\n",
           spolar[0], spolar[1], spolar[2], spolar[3]); 
       frac_polar = -1;
       if (spolar[0] > 0. && sqrt(spolar[1]*spolar[1]+spolar[2]*spolar[2]) >= spolar[0]) {
         frac_polar = spolar[0]/sqrt(spolar[1]*spolar[1]+spolar[2]*spolar[2]);
       } 
    } else {
     polar[0] = polar[1] = -1.;
     frac_polar = -1.;
    }
  }
  if (frac_polar < 0.) {
     H5LTread_dataset_double(hdf, "/entry/sample/beam/incident_polarization", polar);
     if (polar[0] >= 0.) {
       if (verbose) fprintf(stderr, " /entry/sample/beam/incident_polarization = [%g,%g] (ratio, angle)\n",
           polar[0], polar[1]); 
           if (polar[0] >= 0. && polar[0] <= 1.) frac_polar = polar[0];
       }
  }


  // Wavelength
  H5LTread_dataset_double(hdf, "/entry/sample/beam/incident_wavelength", &wavelength); // Firmware >= 1.7
  if (wavelength > 0) {
     if (verbose) fprintf(stderr, " /entry/sample/beam/incident_wavelength = %f (A)\n", wavelength);
  } else {
     if (verbose) fprintf(stderr, "  /entry/sample/beam/incident_wavelength not present. Trying another place.\n");

    H5LTread_dataset_double(hdf, "/entry/instrument/beam/wavelength", &wavelength);
    if (wavelength > 0) {
       if (verbose) fprintf(stderr, " /entry/instrument/beam/wavelength = %f (A)\n", wavelength);
    } else {
      fprintf(stderr, "  /entry/instrument/beam/wavelength not present. Trying another place.\n");

      H5LTread_dataset_double(hdf, "/entry/instrument/monochromator/wavelength", &wavelength);
      if (wavelength > 0) {
	 if (verbose) fprintf(stderr, " /entry/instrument/monochromator/wavelength = %f (A)\n", wavelength);
      } else {
	fprintf(stderr, "  /entry/instrument/monochromator/wavelength not present. Trying another place.\n");

	H5LTread_dataset_double(hdf, "/entry/instrument/beam/incident_wavelength", &wavelength); // Firmware 1.6
	if (wavelength > 0) {
	   if (verbose) fprintf(stderr, " /entry/instrument/beam/incident_wavelength = %f (A)\n", wavelength);
	} else {
	  fprintf(stderr, "  /entry/instrument/beam/incident_wavelength not present.\n");
	}
      }
    }
  }
  if (wavelength < 0) {
    fprintf(stderr, " WARNING: wavelength was not defined! \"Wavelength\" field in the output is set to -1.\n");
  }

  H5LTread_dataset_double(hdf, "/entry/sample/goniometer/omega_range_average", &osc_width);
  if (osc_width > 0) {
     if (verbose) fprintf(stderr, " /entry/sample/goniometer/omega_range_average = %f (deg)\n", osc_width);
  } else {
    fprintf(stderr, " WARNING: oscillation width was not defined. \"Start_angle\" field in the output is set to 0!\n");
    osc_width = 0;
  }

  // TODO: Is it always in omega?
  int bufsize = (nimages < 100000) ? 100000 : nimages;
  double *angles = (double*)malloc(bufsize * sizeof(double));
  // I don't know why but nimages can be too small ...
  if (angles == NULL) {
    fprintf(stderr, "failed to allocate buffer for omega.\n");
    return -1;
  }
  angles[0] = -9999;
  H5LTread_dataset_double(hdf, "/entry/sample/goniometer/omega", angles);
  fprintf(stderr, "\n");

  hid_t entry, group;
  entry = H5Gopen2(hdf, "/entry", H5P_DEFAULT);
  if (entry < 0) {
    fprintf(stderr, "/entry does not exist!\n");
    return -1;
  }

  // Check if /entry/data present
  group = H5Gopen2(entry, "data", H5P_DEFAULT);  
  if (group < 0) {
    group = entry; // leak!
  }

  int block_start = 1;
  if (H5LTfind_dataset(group, "data_000000")) {
    fprintf(stderr, "This dataset starts from data_000000.\n");
    block_start = 0;
  } else {
    fprintf(stderr, "This dataset starts from data_000001.\n");
  }

  char data_name[20] = {};
  hid_t data, dataspace;
  int number_per_block = 0;
  
  // Open the first data block to get the number of frames in a block
  snprintf(data_name, 20, "data_%06d", block_start); 
  data = H5Dopen2(group, data_name, H5P_DEFAULT);
  dataspace = H5Dget_space(data);
  if (data < 0) {
    fprintf(stderr, "failed to open /entry/%s\n", data_name);
    return -1;
  }
  if (H5Sget_simple_extent_ndims(dataspace) != 3) {
    fprintf(stderr, "Dimension of /entry/%s is not 3!\n", data_name);
    return -1;    
  }
  hsize_t dims[3];
  H5Sget_simple_extent_dims(dataspace, dims, NULL);
  number_per_block = dims[0];
  fprintf(stderr, "The number of images per data block is %d.\n", number_per_block);

  H5Sclose(dataspace);
  H5Dclose(data);

  fprintf(stderr, "\nFile analysis completed.\n\n");
 
  {
    /* char header_format[] = 
      "\n"
      "# Detector: %s, S/N %s\n"
      "# Pixel_size %de-6 m x %de-6 m\n"
      "# Silicon sensor, thickness %.6f m\n"
      "# Exposure_time %f s\n"
      "# Exposure_period %f s\n"
      "# Count_cutoff %d counts\n"
      "# Wavelength %f A\n"
      "# Detector_distance %f m\n"
      "# Beam_xy (%.2f, %.2f) pixels\n"
      "# Start_angle %f deg.\n"
      "# Angle_increment %f deg.\n"; 

    char header_content[4096] = {};
    snprintf(header_content, 4096, header_format,
	     description, detector_sn,
	     thickness,
	     (int)(pixelsize * 1E6), (int)(pixelsize * 1E6),
	     count_time, frame_time, countrate_cutoff, wavelength, distance,
	     new_beam_cent?nbeamx:beamx, new_beam_cent?nbeamy:beamy, osc_start, osc_width);
    */


    H5Sclose(dataspace);
    H5Dclose(data);

    /////////////////////////////////////////////////////////////////
    // Reading done. Here output starts...

    FILE *fh = stdout;
    if (dozor_dat) fprintf(stdout,"!\n");
    fprintf(stdout,"%sdetector %s%s",param_prologue,detector,param_epilogue);
    if (count_time > 0.) {
      fprintf(stdout,"%sexposure %f%s",param_prologue,count_time,param_epilogue);  
    } else if (verbose) {
      fprintf(stdout,"%sexposure %s%s",param_prologue,"unknown_exposure",param_epilogue);  
    }
    if (distance >= 0.) {
      fprintf(stdout,"%sdetector_distance %f%s",param_prologue,distance*1.e3,param_epilogue);  
    } else if (verbose) {
      fprintf(stdout,"%sdetector_distance %s%s",param_prologue,"unknown_distance",param_epilogue);  
    }
    if (wavelength >= 0.) {
      fprintf(stdout,"%sX-ray_wavelength %f%s",param_prologue,wavelength,param_epilogue);  
    } else if (verbose) {
      fprintf(stdout,"%sX-ray_wavelength %s%s",param_prologue,"unknown_wavelength",param_epilogue);  
    }
    if (frac_polar >= 0.) {
      fprintf(stdout,"%sfraction_polarization %f%s",param_prologue,frac_polar,param_epilogue);  
    } else if (verbose) {
      fprintf(stdout,"%sfraction_polarization %s%s",param_prologue,"unknown_polarization",param_epilogue);  
    }
    if (verbose) {
      fprintf(stdout,"%spixel_min 1%s",param_prologue,param_epilogue);
    }
    if (countrate_cutoff >= 0) {
      fprintf(stdout,"%spixel_max %d%s",param_prologue,countrate_cutoff,param_epilogue);
    } else if (verbose) {
      fprintf(stdout,"%spixel_max %s%s",param_prologue,"unknown_pixel_max",param_epilogue);
    }

    beamx=new_beam_cent?nbeamx:beamx;
    beamy=new_beam_cent?nbeamy:beamy;
    if (beamx > -9999999. && beamy > -9999999.) {
      fprintf(stdout,"%six_min %f%s",param_prologue,beamx-50.,param_epilogue);
      fprintf(stdout,"%six_max %f%s",param_prologue,beamx+50.,param_epilogue);
      fprintf(stdout,"%siy_min %f%s",param_prologue,beamy-50.,param_epilogue);                
      fprintf(stdout,"%siy_max %f%s",param_prologue,beamy+50.,param_epilogue);
      fprintf(stdout,"%sorgx %f%s",param_prologue,beamx,param_epilogue);
      fprintf(stdout,"%sorgy %f%s",param_prologue,beamy,param_epilogue);  
    } else if (verbose) {
      fprintf(stdout,"%six_min %s%s",param_prologue,"unknown_ix_min",param_epilogue);                
      fprintf(stdout,"%six_max %s%s",param_prologue,"unknown_ix_max",param_epilogue);
      fprintf(stdout,"%siy_min %s%s",param_prologue,"unknown_iy_min",param_epilogue);
      fprintf(stdout,"%siy_max %s%s",param_prologue,"unknown_iy_max",param_epilogue);
      fprintf(stdout,"%sorgx %s%s",param_prologue,"unknown_orgx",param_epilogue);
      fprintf(stdout,"%sorgy %s%s",param_prologue,"unknown_orgy",param_epilogue); 
    }
    if (osc_width >= 0.) {
      fprintf(stdout,"%soscillation_range %f%s",param_prologue,osc_width,param_epilogue);
    } else if (verbose) {
      fprintf(stdout,"%soscillation_range %s%s",param_prologue,"unknown_oscillation_range",param_epilogue);
    }
    if (nimages > 0 && angles[0] > -9999.) {
      fprintf(stdout,"%simage_step %f%s",param_prologue,angles[1]-angles[0],param_epilogue);
    } else if (verbose) {
      fprintf(stdout,"%simage_step %s%s",param_prologue,"unknown_image_step",param_epilogue);
    }
    if (angles[0] > -9999.) {
      fprintf(stdout,"%sstarting_angle %f%s",param_prologue,angles[0],param_epilogue);
    } else {
      fprintf(stdout,"%sstarting_angle %s%s",param_prologue,"unknown_starting_angle",param_epilogue);
    }
    if (nimages > 0) {
      fprintf(stdout,"%sfirst_image_number %d%s",param_prologue,1,param_epilogue);
      fprintf(stdout,"%snumber_images %d%s",param_prologue,nimages,param_epilogue);
    } else if (verbose) {
      fprintf(stdout,"%sfirst_image_number %s%s",param_prologue,"unknown_first_image_number",param_epilogue);
      fprintf(stdout,"%snumber_images %s%s",param_prologue,"unknown_number_images",param_epilogue);
    }
   
    fprintf(stdout,"%sname_template_image %s%s",param_prologue,argv[1+optcount],param_epilogue);

  }

  H5Gclose(group);
  H5Fclose(hdf);

  free(angles);

  if (verbose) fprintf(stderr, "\nAll done!\n");

  return 0;
}
