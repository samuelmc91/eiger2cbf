/*
 EIGER HDF5 reader plugin
  written by Takanori Nakane, 2017/3/28
  based on Keitaro Yamashita's serial version.

Reference:
 https://github.com/dectris/dummy_xds_hook
 https://github.com/dectris/neggia

To build:

 Linux:

 gcc -std=gnu99 -o plugin.so -shared -fPIC -g -O3 \
     -I/app/dials/base/include -L/app/dials/base/lib \
     plugin.c \
     -Ilz4 lz4/lz4.c lz4/h5zlz4.c \
     bitshuffle/bshuf_h5filter.c \
     bitshuffle/bshuf_h5plugin.c \
     bitshuffle/bitshuffle.c \
     -lpthread -lhdf5_hl -lhdf5 -lrt

 Mac OS:
 TODO: need to test.

TODO:
 Need better error exit to ensure shared memories are freed.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __linux
 #include <sys/prctl.h>
 #include <linux/limits.h>
#endif

#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <H5Ppublic.h>
#include "H5api_adpt.h"
#include "hdf5_hl.h"
#include "hdf5.h"
#include <pthread.h>

#define INVALID -9999


extern const H5Z_class2_t H5Z_LZ4;
extern const H5Z_class2_t bshuf_H5Filter;
void register_filters() {
  H5Zregister(&H5Z_LZ4);
  H5Zregister(&bshuf_H5Filter);
}

struct GlobalData {
  char filename[4096];
  hid_t hdf, group;
  int dimx, dimy;
  int Nminus1, Nminus2;
  int datasize;
  int nframesPerDataset;
  signed int *minus1, *minus2;
  float xpixelSize;
  float ypixelSize;
  int block_start;
  unsigned int error_val;
  pthread_mutex_t lock;
};
struct GlobalData *GLOBAL_DATA = NULL;
int prev_block_number = -1;
hid_t data = -1, dataspace = -1, memspace=-1;


void plugin_get_header(int *nx, int *ny, int *nbytes, float *qx, float *qy,
                       int *number_of_frames, int info[1024],
                       int *error_flag);

void plugin_open(const char *filename, int info_array[1024], int *error_flag) {
  register_filters();

  /* patch bug in the latest BUILT */
  char fn[4096];
  strcpy(fn, filename);
  int n = strlen(fn);
  fn[n - 9] = 'm';
  fn[n - 8] = 'a';
  fn[n - 7] = 's';
  fn[n - 6] = 't';
  fn[n - 5] = 'e';
  fn[n - 4] = 'r';
  fprintf(stderr, "PLUGIN INFO: plugin_open called with filename = %s\n", fn);

  int nx, ny, nbytes, nframes, info[1024], dummy;
  float qx, qy;

  if (GLOBAL_DATA != NULL) {
    fprintf(stderr, "PLUGIN ERROR: CAN ONLY OPEN ONE FILE AT A TIME\n");
    *error_flag = -4;
    return;
  }

 
  /* Setup global variables */
  GLOBAL_DATA = (struct GlobalData*)malloc(sizeof(struct GlobalData));
  strcpy(GLOBAL_DATA->filename, fn);

  pthread_mutex_init(&(GLOBAL_DATA->lock), NULL);

  GLOBAL_DATA->hdf = H5Fopen(fn, H5F_ACC_RDONLY, H5P_DEFAULT);
  if (GLOBAL_DATA->hdf < 0) {
    fprintf(stderr, "PLUGIN ERROR: Failed to open file %s\n", filename);
    *error_flag = -4;
    return;
  }

  plugin_get_header(&nx, &ny, &nbytes, &qx, &qy, &nframes, info, &dummy);

  *error_flag = 0;
  return;
}

void plugin_get_header(int *nx, int *ny, int *nbytes, float *qx, float *qy, int *number_of_frames,
		       int info[1024], int *error_flag) {
  info[0] =  1; // Vendor   [1:Dectris] // other values were not accepted by the host program!
  info[1] =  0; // Version  [Major]
  info[2] =  1; // Version  [Minor]
  info[3] =  0; // Version  [Patch]
  info[4] = -1; // Version  [timestamp]

  if (GLOBAL_DATA == NULL) {
    fprintf(stderr, "PLUGIN ERROR: plugin_get_header called before plugin_open\n");
    *error_flag = -2;
    return;
  }

  hid_t hdf = GLOBAL_DATA->hdf;

  /* Image depth*/
  int depth = -1;
  H5LTread_dataset_int(hdf, "/entry/instrument/detector/bit_depth_image", &depth);
  if (depth > 0) {
    fprintf(stderr, "PLUGIN INFO: /entry/instrument/detector/bit_depth_image = %d\n", depth);
  } else {
    fprintf(stderr, "PLUGIN WARNING: /entry/instrument/detector/bit_depth_image is not avaialble. We assume 16 bit.\n");
    depth = 16; 
  }
  GLOBAL_DATA->error_val = (unsigned int)((1ULL << depth) - 1); 

  /* Pixel size */
  H5LTread_dataset_float(hdf, "/entry/instrument/detector/x_pixel_size", &GLOBAL_DATA->xpixelSize);
  H5LTread_dataset_float(hdf, "/entry/instrument/detector/y_pixel_size", &GLOBAL_DATA->ypixelSize);

  /* Image size */
  int xpixels = -1, ypixels = -1;
  H5LTread_dataset_int(hdf, "/entry/instrument/detector/detectorSpecific/x_pixels_in_detector", &xpixels);
  H5LTread_dataset_int(hdf, "/entry/instrument/detector/detectorSpecific/y_pixels_in_detector", &ypixels);
  GLOBAL_DATA->dimx = xpixels;
  GLOBAL_DATA->dimy = ypixels;

  /* Number of images */
  int nimages = -1;
  H5LTread_dataset_int(hdf, "/entry/instrument/detector/detectorSpecific/nimages", &nimages);
  if (nimages < 0) {
    fprintf(stderr, "PLUGIN ERROR: failed to read the nimages.\n");
    *error_flag = -4;
    return;
  }

  /* Number of triggers */
  int ntrigger = -1;
  H5LTread_dataset_int(hdf, "/entry/instrument/detector/detectorSpecific/ntrigger", &ntrigger);
  if (ntrigger < 0) {
    fprintf(stderr, "PLUGIN ERROR: failed to read the ntrigger.\n");
    *error_flag = -4;
    return;
  }
  
  /* Make sure /entry/data is present */
  hid_t entry, group;
  entry = H5Gopen2(hdf, "/entry", H5P_DEFAULT);
  if (entry < 0) {
    fprintf(stderr, "PLUGIN ERROR: /entry does not exist!\n");
    *error_flag = -4;
    return;
  }
  GLOBAL_DATA->group = entry;
  group = H5Gopen2(entry, "data", H5P_DEFAULT);
  if (group < 0) {
    fprintf(stderr, "PLUGIN WARNING: /entry/data does not exist!\n");
  } else {
    GLOBAL_DATA->group = group;
  }

  /* Is it 0-indexed? */
  GLOBAL_DATA->block_start = 1;
  if (H5LTfind_dataset(group, "data_000000")) {
    fprintf(stderr, "PLUGIN INFO: This dataset starts from data_000000.\n");
    GLOBAL_DATA->block_start = 0;
  } else {
    fprintf(stderr, "PLUGIN INFO: This dataset starts from data_000001.\n");
  }

  // Open the first data block to get the number of frames in a block
  char data_name[20] = {};
  hid_t data, dataspace;
  GLOBAL_DATA->nframesPerDataset = 0;
  snprintf(data_name, 20, "data_%06d", GLOBAL_DATA->block_start); 
  data = H5Dopen2(group, data_name, H5P_DEFAULT);
  dataspace = H5Dget_space(data);
  if (data < 0) {
    fprintf(stderr, "PLUGIN ERROR: failed to open /entry/%s\n", data_name);
    *error_flag = -4;
    return;
  }
  if (H5Sget_simple_extent_ndims(dataspace) != 3) {
    fprintf(stderr, "PLUGIN ERROR: Dimension of /entry/%s is not 3!\n", data_name);
    *error_flag = -4;
    return;
  }

  hsize_t dims[3];
  H5Sget_simple_extent_dims(dataspace, dims, NULL);
  GLOBAL_DATA->nframesPerDataset = dims[0];
  fprintf(stderr, "PLUGIN INFO: The number of images per data block is %d.\n", GLOBAL_DATA->nframesPerDataset);

  H5Sclose(dataspace); dataspace = -1;
  H5Dclose(data); data = -1;

  *nx = GLOBAL_DATA->dimx;
  *ny = GLOBAL_DATA->dimy;
  *nbytes = GLOBAL_DATA->dimx * GLOBAL_DATA->dimy * sizeof(int);
  // TODO: This actually depends on the depth, but an INTEGER array is supplied to plugin_get_data.
  //       So this is OK?
  *qx = GLOBAL_DATA->xpixelSize;
  *qy = GLOBAL_DATA->ypixelSize;
  *number_of_frames = (int)(nimages * ntrigger);

  *error_flag = 0;
  /* fprintf(stderr,"PLUGIN INFO: complete for %s\n", data_name); */
  return;
}

int get_local_data(int frame_number, int *local_buf) {
  int ret;
  int xpixels = GLOBAL_DATA->dimx, ypixels = GLOBAL_DATA->dimy;

  int block_number = GLOBAL_DATA->block_start + (frame_number - 1) / GLOBAL_DATA->nframesPerDataset;
  int frame_in_block = (frame_number - 1) % GLOBAL_DATA->nframesPerDataset;

  char data_name[20] = {};

  if (prev_block_number != block_number) {
    prev_block_number = block_number;

    /* fprintf(stderr, "processing block %d\n",block_number); */ 

    if (data > 0) H5Dclose(data);
    data = -1;
    if (dataspace > 0) H5Sclose(dataspace);
    dataspace = -1;

    snprintf(data_name, 20, "data_%06d", block_number);
    if (GLOBAL_DATA->group <= 0 ) {
      fprintf(stderr, "failed to find /entry/%s\n", "data...");
      return -4;
    }
    data = H5Dopen2(GLOBAL_DATA->group, data_name, H5P_DEFAULT);
    if (data <=  0) {
      fprintf(stderr, "failed to open /entry/%s\n", data_name);
      return -4;
    } else  {
      /* fprintf(stderr, "opened dataset /entry/%s\n", data_name); */
    }
    dataspace = H5Dget_space(data);
    if (dataspace < 0) {
      fprintf(stderr, "failed to open dataspace of /entry/%s\n", data_name);
      return -4;
    } else {
      /* fprintf(stderr, "opened dataspace of /entry/%s\n", data_name); */
    }

    if (H5Sget_simple_extent_ndims(dataspace) != 3) {
      fprintf(stderr, "Dimension of /entry/%s is not 3!\n", data_name);
      return -4;
    }
  }



  hsize_t offset_in[3] = {frame_in_block, 0, 0};
  hsize_t offset_out[3] = {0, 0, 0};
  hsize_t count[3] = {1, ypixels, xpixels};

  /* Create memory space */
  if (memspace <= 0) {
    hsize_t dims[3];
    H5Sget_simple_extent_dims(dataspace, dims, NULL);

    memspace = H5Screate_simple(3, dims, NULL);
    if (memspace < 0) {
      fprintf(stderr, "PLUGIN for frame #%d: failed to create memspace.\n", frame_number);
      return -4;
    }
    ret = H5Sselect_hyperslab(memspace, H5S_SELECT_SET, offset_out, NULL,
                              count, NULL);
    if (ret < 0) {
      fprintf(stderr, "PLUGIN for frame #%d: select_hyperslab for memory failed.\n", frame_number);
      return -2;
    }
  }

  /* Get the frame */
  ret = H5Sselect_hyperslab(dataspace, H5S_SELECT_SET, offset_in, NULL, count, NULL);
  if (ret < 0) {
    fprintf(stderr, "PLUGIN for frame #%d: select_hyperslab for file failed.\n", frame_number);
    return -2;
  }

  ret = H5Dread(data, H5T_NATIVE_UINT, memspace, dataspace, H5P_DEFAULT, local_buf);
  if (ret < 0) {
    fprintf(stderr, "PLUGIN for frame #%d: H5Dread for image failed.\n", frame_number);
    return -2;
  }

  /* fprintf(stderr,"processing frame %d\n", frame_in_block); */
  int error_val = GLOBAL_DATA->error_val;
  if (GLOBAL_DATA->Nminus1 < 0) {// pixel mask is not available
    for (int i = 0, ilim = xpixels * ypixels; i < ilim; i++) {
      if (local_buf[i] == error_val) local_buf[i] = -1;
    }
  } else { // pixel mask is available
    for (int i = 0, ilim = GLOBAL_DATA->Nminus1; i < ilim; i++) {
      local_buf[GLOBAL_DATA->minus1[i]] = -1;
    }
    for (int i = 0, ilim = GLOBAL_DATA->Nminus2; i < ilim; i++) {
      local_buf[GLOBAL_DATA->minus2[i]] = -1;
    }
  }

  return 0;
}


void plugin_get_data(int *frame_number, int *nx, int *ny,
		     int data_array[], int info_array[1024],
		     int *error_flag) {
  if (GLOBAL_DATA == NULL){
    fprintf(stderr, "PLUGIN ERROR: plugin_get_data called before plugin_open.\n");
    *error_flag = -4;
    return;
  }

  /* fprintf(stderr,"entering get_local_data\n");*/ 
  pthread_mutex_lock(&(GLOBAL_DATA->lock));
  *error_flag = get_local_data(*frame_number, data_array);
  pthread_mutex_unlock(&(GLOBAL_DATA->lock));
  /* fprintf(stderr,"exiting with error_flag %d\n",*error_flag);*/

  return;
}

void plugin_close(int *error_flag){
  printf("PLUGIN PARENT: plugin_close called.\n");


}
