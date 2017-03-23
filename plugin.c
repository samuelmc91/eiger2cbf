/*
 EIGER HDF5 reader plugin
  written by Takanori Nakane, 2017/3/23

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
     -lpthread -lhdf5_hl -lhdf5 -O3

 Mac OS:
 TODO: need to test.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#ifdef __linux
 #include <sys/prctl.h>
#endif

#include <unistd.h>
#include <sys/mman.h>
#include <signal.h>
#include "hdf5_hl.h"
#include "hdf5.h"

#define NCHILD 4 // EDIT!

extern const H5Z_class2_t H5Z_LZ4;
extern const H5Z_class2_t bshuf_H5Filter;
void register_filters() {
  H5Zregister(&H5Z_LZ4);
  H5Zregister(&bshuf_H5Filter);
}

struct GlobalData {
  char filename[300];
  hid_t hdf, group;
  int dimx;
  int dimy;
  int datasize;
  int nframesPerDataset;
  signed int *pixel_mask;
  float xpixelSize;
  float ypixelSize;
  int block_start;
  unsigned int error_val;
  pthread_mutex_t locks[NCHILD];
  unsigned int *mapped_bufs[NCHILD];
  int ctop_pipes[NCHILD][2]; 
  int ptoc_pipes[NCHILD][2];
};
struct GlobalData *GLOBAL_DATA = NULL;

void plugin_get_header(int *nx, int *ny, int *nbytes, float *qx, float *qy,
                       int *number_of_frames, int info[1024],
                       int *error_flag);
void child_loop(int myid);

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
  if (GLOBAL_DATA != NULL) {
    fprintf(stderr, "PLUGIN ERROR: CAN ONLY OPEN ONE FILE AT A TIME\n");
    *error_flag = -4;
    return;
  }

  /* Setup global variables */
  GLOBAL_DATA = (struct GlobalData*)malloc(sizeof(struct GlobalData));
  strcpy(GLOBAL_DATA->filename, fn);

  GLOBAL_DATA->hdf = H5Fopen(fn, H5F_ACC_RDONLY, H5P_DEFAULT);
  if (GLOBAL_DATA->hdf < 0) {
    fprintf(stderr, "PLUGIN ERROR: Failed to open file %s\n", filename);
    *error_flag = -4;
    return;
  }

  int nx, ny, nbytes, nframes, info[1024], dummy;
  float qx, qy;
  plugin_get_header(&nx, &ny, &nbytes, &qx, &qy, &nframes, info, &dummy);

  /* Setup and start child processes */
  for (int i = 0; i < NCHILD; i++) {
    pthread_mutex_init(&GLOBAL_DATA->locks[i], NULL);
    int pipe1 = pipe(&GLOBAL_DATA->ctop_pipes[i][0]);
    int pipe2 = pipe(&GLOBAL_DATA->ptoc_pipes[i][0]);
    GLOBAL_DATA->mapped_bufs[i] = mmap(0, sizeof(unsigned int) * nx * ny, 
                                         PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    // MAP_HUGETLB | 25 << MAP_HUGE_SHIFT causes SEGV in HDF5 lib

    int pid = fork();
    if (pipe1 < 0 || pipe2 < 0 || pid < 0) {
      fprintf(stderr, "PLUGIN ERROR: Failed to create subprocess.\n");
      *error_flag = -2;
      return;
    }
    if (pid == 0) {
       child_loop(i);
    }
  }

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

  /* Pixel mask */
  GLOBAL_DATA->pixel_mask = (signed int*)malloc(sizeof(signed int) * xpixels * ypixels);
  GLOBAL_DATA->pixel_mask[0] = -9999;
  H5LTread_dataset_int(hdf, "/entry/instrument/detector/detectorSpecific/pixel_mask", GLOBAL_DATA->pixel_mask);
  if (GLOBAL_DATA->pixel_mask[0] == -9999) {
    fprintf(stderr, "PLUGIN ERROR: failed to read the pixel mask from /entry/instrument/detector/detectorSpecific/pixel_mask.\n");
    *error_flag = -4;
    return;
  }
  
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

  H5Sclose(dataspace);
  H5Dclose(data);

  *nx = GLOBAL_DATA->dimx;
  *ny = GLOBAL_DATA->dimy;
  *nbytes = GLOBAL_DATA->dimx * GLOBAL_DATA->dimy * sizeof(int);
  // TODO: This actually depends on the depth, but an INTEGER array is supplied to plugin_get_data.
  //       So this is OK?
  *qx = GLOBAL_DATA->xpixelSize;
  *qy = GLOBAL_DATA->ypixelSize;
  *number_of_frames = (int)(nimages * ntrigger);

  *error_flag = 0;
  return;
}

int prev_block_number = -1;
hid_t data = NULL, dataspace = NULL, memspace=NULL;

int get_data(int myid, int frame_number, int *mapped_buf) {
  int ret;
  int xpixels = GLOBAL_DATA->dimx, ypixels = GLOBAL_DATA->dimy;

  int block_number = GLOBAL_DATA->block_start + (frame_number - 1) / GLOBAL_DATA->nframesPerDataset;
  int frame_in_block = (frame_number - 1) % GLOBAL_DATA->nframesPerDataset;

  char data_name[20] = {};
 
  if (prev_block_number != block_number) {
    prev_block_number = block_number;
//    if (data != NULL) H5Gclose(data); 
//    if (dataspace != NULL) H5Dclose(dataspace);

    snprintf(data_name, 20, "data_%06d", block_number); 
    data = H5Dopen2(GLOBAL_DATA->group, data_name, H5P_DEFAULT);
    dataspace = H5Dget_space(data);
    if (data < 0) {
      fprintf(stderr, "failed to open /entry/%s\n", data_name);
      return -4;
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
  if (memspace == NULL) {
    hsize_t dims[3];
    H5Sget_simple_extent_dims(dataspace, dims, NULL);

    memspace = H5Screate_simple(3, dims, NULL);
    if (memspace < 0) {
      fprintf(stderr, "PLUGIN CHILD %d for frame #%d: failed to create memspace\n", myid, frame_number);
      return -4;
    }
    ret = H5Sselect_hyperslab(memspace, H5S_SELECT_SET, offset_out, NULL, 
                              count, NULL);
    if (ret < 0) {
      fprintf(stderr, "PLUGIN CHILD %d for frame #%d: select_hyperslab for memory failed\n", myid, frame_number);
      return -2;
    }
  }
  
  /* Get the frame */
  ret = H5Sselect_hyperslab(dataspace, H5S_SELECT_SET, offset_in, NULL, count, NULL);
  if (ret < 0) {
    fprintf(stderr, "PLUGIN CHILD %d for frame #%d: select_hyperslab for file failed\n", myid, frame_number);
    return -2;
  }

  ret = H5Dread(data, H5T_NATIVE_UINT, memspace, dataspace, H5P_DEFAULT, mapped_buf);
  if (ret < 0) {
    fprintf(stderr, "PLUGIN CHILD %d for frame #%d: H5Dread for image failed. Wrong frame number?\n", myid, frame_number);
    return -2;
  }

  signed int *pixel_mask = GLOBAL_DATA->pixel_mask;
  int error_val = GLOBAL_DATA->error_val;
  if (pixel_mask[0] == -9999) {// pixel mask is not available
    for (int i = 0, ilim = xpixels * ypixels; i < ilim; i++) {
      if (mapped_buf[i] == error_val) mapped_buf[i] = -1;
    }
  } else { // pixel mask is available
    for (int i = 0, ilim = xpixels * ypixels; i < ilim; i++) {
      if (pixel_mask[i] == 1) mapped_buf[i] = -1;
      else if (pixel_mask[i] > 1) mapped_buf[i] = -2;
    }
  }
  
  return 0; 
}

void usr1_handler(int dummy) {
  // don't care open handlers and memories; we are going to die!
  exit(0);
}

void child_loop(int myid) {
  printf("PLUGIN CHILD %d: started.\n", myid);

  // Some programs do not call plugin_close :-<
  #ifdef HAVE_SYS_PRCTL_H
  signal(SIGUSR1, usr1_handler);
  prctl(PR_SET_PDEATHSIG, SIGUSR1); // TODO: this is not supported on Mac OS.
  #endif

  int frame_num;
  while (1) {
    /* receive command from the parent */
    if (read(GLOBAL_DATA->ptoc_pipes[myid][0], &frame_num, sizeof(int)) < 0) {
      fprintf(stderr, "PLUGIN CHILD %d ERROR: cannot read from parent.\n", myid);
      // we cannot do anything but crash!
    }
    if (frame_num == -999) break;

    /* do the work */
    //fprintf(stderr, "PLUGIN CHILD %d: got request for frame #%d\n", myid, frame_num);
    int retval = get_data(myid, frame_num, GLOBAL_DATA->mapped_bufs[myid]);

    /* send back the result */
    if (write(GLOBAL_DATA->ctop_pipes[myid][1], &retval, sizeof(int)) < 0) {
      fprintf(stderr, "PLUGIN CHILD %d ERROR: cannot write to parent.\n", myid);
    }
    //fprintf(stderr, "PLUGIN CHILD %d: processed frame #%d with retval %d\n", myid, frame_num, retval);
  }

  fprintf(stderr, "PLUGIN CHILD %d: finished.\n", myid);
  exit(-1);
}

void plugin_get_data(int *frame_number, int *nx, int *ny,
		     int data_array[], int info_array[1024],
		     int *error_flag) {
  if (GLOBAL_DATA == NULL){
    fprintf(stderr, "PLUGIN ERROR: plugin_get_data called before plugin_open\n");
    *error_flag = -4;
    return;
  }

  int child_id = *frame_number % NCHILD;
  pthread_mutex_lock(&GLOBAL_DATA->locks[child_id]);
  fprintf(stderr, "PLUGIN PARENT: get_data for frame #%d delegated to child #%d\n", *frame_number, child_id);
  if (write(GLOBAL_DATA->ptoc_pipes[child_id][1], frame_number, sizeof(int)) < 0) {
    fprintf(stderr, "PLUGIN ERROR: cannot write to child #%d for frame #%d\n", child_id, *frame_number);
    *error_flag = -1;
    return;
  }

  int retval;
  if (read(GLOBAL_DATA->ctop_pipes[child_id][0], &retval, sizeof(int)) < 0) {
    fprintf(stderr, "PLUGIN ERROR: cannot read from child #%d for frame #%d\n", child_id, *frame_number);
    *error_flag = -1;
    return;
  }
  //fprintf(stderr, "PLUGIN PARENT: received %d for frame #%d from child #%d\n", retval, *frame_number, child_id);
  memcpy(data_array, GLOBAL_DATA->mapped_bufs[child_id], sizeof(unsigned int) * GLOBAL_DATA->dimx * GLOBAL_DATA->dimy);
  pthread_mutex_unlock(&GLOBAL_DATA->locks[child_id]);

  *error_flag = retval;
  return;
}

void plugin_close(int *error_flag){
  printf("PLUGIN PARENT: plugin_close called.\n");

  for (int i = 0; i < NCHILD; i++) {
    int val = -999;
    if (write(GLOBAL_DATA->ptoc_pipes[i][1], &val, sizeof(int)) < 0) {
      fprintf(stderr, "PLUGIN ERROR: cannot write to child #%d for exit.\n", i);
    }
  }
  free(GLOBAL_DATA->pixel_mask);
}
