/*
 EIGER HDF5 reader plugin (worker)
  written by Takanori Nakane, 2017/3/28
  based on Keitaro Yamashita's serial version.

Reference:
 https://github.com/dectris/dummy_xds_hook
 https://github.com/dectris/neggia

To build:

 Linux:

 gcc -std=gnu99 -o plugin-worker -g -O3 \
     plugin-worker.c \
     -Ilz4 lz4/lz4.c lz4/h5zlz4.c \
     bitshuffle/bshuf_h5filter.c \
     bitshuffle/bshuf_h5plugin.c \
     bitshuffle/bitshuffle.c \
     -lpthread -lhdf5_hl -lhdf5 -lrt

 Mac OS:
 TODO: need to test.
*/

#define _GNU_SOURCE
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <H5Ppublic.h>
#include "H5api_adpt.h"
#include "hdf5_hl.h"
#include "hdf5.h"

#define INVALID -9999

extern const H5Z_class2_t H5Z_LZ4;
extern const H5Z_class2_t bshuf_H5Filter;
void register_filters() {
  H5Zregister(&H5Z_LZ4);
  H5Zregister(&bshuf_H5Filter);
}

typedef struct _ShmTransferBuffer {
    volatile int ptoc_flag_snd;  /* only modify from parent, and only after putting data in ptoc_data */
    volatile int ptoc_flag_rcv;  /* only modify from child,  and only after processing incoming ptoc_data */
    volatile int ptoc_data;      /* only modify from parent, and only when ptoc_flag_snd==ptoc_flag_rcv */
    volatile pid_t ppid;         /* parent process pid, returned by getpid in the parent */
    volatile int ctop_flag_snd;  /* only modify ftom child,  and only after putting data in ctop_data */
    volatile int ctop_flag_rcv;  /* only modify from parent, and only after processing incoming ctop_data */
    volatile int ctop_data;      /* only modify from child,  and only when ctop_flag_snd==ctop_flag_rcv */
    volatile pid_t cpid;         /* child process pid, returned by fork in the parent and getpid in the child */
} ShmTransferBuffer;

struct GlobalData {
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
  ShmTransferBuffer *mapped_transfer_buf;
  unsigned int *mapped_buf;
};
struct GlobalData *GLOBAL_DATA = NULL;

static void sigcont_handler (int signal, siginfo_t *siginfo, void * context) {}

void plugin_get_header(int *nx, int *ny, int *nbytes, float *qx, float *qy,
                       int *number_of_frames, int info[1024],
                       int *error_flag);
void child_loop(int myid);

int open_file(const char *filename) {
  register_filters();

  fprintf(stderr, "PLUGIN INFO: plugin_open called with filename = %s\n", filename);
  if (GLOBAL_DATA != NULL) {
    fprintf(stderr, "PLUGIN ERROR: CAN ONLY OPEN ONE FILE AT A TIME\n");
    fflush(stderr);
    return -4;
  }

  /* Setup global variables */
  GLOBAL_DATA = (struct GlobalData*)malloc(sizeof(struct GlobalData));

  GLOBAL_DATA->hdf = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);
  if (GLOBAL_DATA->hdf < 0) {
    fprintf(stderr, "PLUGIN ERROR: Failed to open file %s\n", filename);
    fflush(stderr);
    return -4;
  }

  int nx, ny, nbytes, nframes, info[1024], dummy;
  float qx, qy;
  plugin_get_header(&nx, &ny, &nbytes, &qx, &qy, &nframes, info, &dummy);

  return 0;
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
    fflush(stderr);
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
  GLOBAL_DATA->minus1 = (signed int*)malloc(sizeof(signed int) * xpixels * ypixels);
  GLOBAL_DATA->minus2 = (signed int*)malloc(sizeof(signed int) * xpixels * ypixels);
  signed int* pixel_mask = (signed int*)malloc(sizeof(signed int) * xpixels * ypixels);
  GLOBAL_DATA->Nminus1 = 0;
  GLOBAL_DATA->Nminus2 = 0;
  pixel_mask[0] = INVALID;
  H5LTread_dataset_int(hdf, "/entry/instrument/detector/detectorSpecific/pixel_mask", pixel_mask);
  if (pixel_mask[0] == INVALID) {
    fprintf(stderr, "PLUGIN WARNING: failed to read the pixel mask from /entry/instrument/detector/detectorSpecific/pixel_mask.\n");
    GLOBAL_DATA->Nminus1 = -1;
  } else {
    for (int i =0, ilim = xpixels * ypixels; i < ilim; i++) {
      if (pixel_mask[i] == 1) GLOBAL_DATA->minus1[GLOBAL_DATA->Nminus1++] = i;
      else if (pixel_mask[i] > 1) GLOBAL_DATA->minus2[GLOBAL_DATA->Nminus2++] = i;
    }
  }
  fprintf(stderr, "PLUGIN: #pixels masked to -1 = %d, to -2 = %d.\n", GLOBAL_DATA->Nminus1, GLOBAL_DATA->Nminus2++);
  free(pixel_mask);

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
    fflush(stderr);
    *error_flag = -4;
    return;
  }

  
  /* Make sure /entry/data is present */
  hid_t entry, group;
  entry = H5Gopen2(hdf, "/entry", H5P_DEFAULT);
  if (entry < 0) {
    fprintf(stderr, "PLUGIN ERROR: /entry does not exist!\n");
    fflush(stderr);
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
    fflush(stderr);
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
hid_t data = 0, dataspace = 0, memspace=0;

int get_data(int myid, int frame_number, int *mapped_buf) {
  int ret;
  int xpixels = GLOBAL_DATA->dimx, ypixels = GLOBAL_DATA->dimy;

  int block_number = GLOBAL_DATA->block_start + (frame_number - 1) / GLOBAL_DATA->nframesPerDataset;
  int frame_in_block = (frame_number - 1) % GLOBAL_DATA->nframesPerDataset;

  char data_name[20] = {};
 
  if (prev_block_number != block_number) {
    prev_block_number = block_number;

    if (data != 0) H5Dclose(data); 
    if (dataspace != 0) H5Sclose(dataspace);

    snprintf(data_name, 20, "data_%06d", block_number); 
    data = H5Dopen2(GLOBAL_DATA->group, data_name, H5P_DEFAULT);
    dataspace = H5Dget_space(data);
    if (data < 0) {
      fprintf(stderr, "failed to open /entry/%s\n", data_name);
      fflush(stderr);
      return -4;
    }
    if (H5Sget_simple_extent_ndims(dataspace) != 3) {
      fprintf(stderr, "Dimension of /entry/%s is not 3!\n", data_name);
      fflush(stderr);
      return -4;
    }
  }

  hsize_t offset_in[3] = {frame_in_block, 0, 0};
  hsize_t offset_out[3] = {0, 0, 0};
  hsize_t count[3] = {1, ypixels, xpixels};

  /* Create memory space */
  if (memspace == 0) {
    hsize_t dims[3];
    H5Sget_simple_extent_dims(dataspace, dims, NULL);

    memspace = H5Screate_simple(3, dims, NULL);
    if (memspace < 0) {
      fprintf(stderr, "PLUGIN CHILD %d for frame #%d: failed to create memspace.\n", myid, frame_number);
      fflush(stderr);
      return -4;
    }
    ret = H5Sselect_hyperslab(memspace, H5S_SELECT_SET, offset_out, NULL, 
                              count, NULL);
    if (ret < 0) {
      fprintf(stderr, "PLUGIN CHILD %d for frame #%d: select_hyperslab for memory failed.\n", myid, frame_number);
      fflush(stderr);
      return -2;
    }
  }
  
  /* Get the frame */
  ret = H5Sselect_hyperslab(dataspace, H5S_SELECT_SET, offset_in, NULL, count, NULL);
  if (ret < 0) {
    fprintf(stderr, "PLUGIN CHILD %d for frame #%d: select_hyperslab for file failed.\n", myid, frame_number);
    fflush(stderr);
    return -2;
  }

  ret = H5Dread(data, H5T_NATIVE_UINT, memspace, dataspace, H5P_DEFAULT, mapped_buf);
  if (ret < 0) {
    fprintf(stderr, "PLUGIN CHILD %d for frame #%d: H5Dread for image failed.\n", myid, frame_number);
    fflush(stderr);
    return -2;
  }

  int error_val = GLOBAL_DATA->error_val;
  if (GLOBAL_DATA->Nminus1 < 0) {// pixel mask is not available
    for (int i = 0, ilim = xpixels * ypixels; i < ilim; i++) {
      if (mapped_buf[i] == error_val) mapped_buf[i] = -1;
    }
  } else { // pixel mask is available
    for (int i = 0, ilim = GLOBAL_DATA->Nminus1; i < ilim; i++) {
      mapped_buf[GLOBAL_DATA->minus1[i]] = -1;
    }
    for (int i = 0, ilim = GLOBAL_DATA->Nminus2; i < ilim; i++) {
      mapped_buf[GLOBAL_DATA->minus2[i]] = -1;
    }
  }
  
  return 0; 
}

void usr1_handler(int dummy) {
  // don't care open handlers and memories; we are going to die!
  exit(0);
}

int main(int argc, char **argv) {
  int failed = 0;
  fprintf(stderr,"Entering plugin-worker with args %s %s %s %s %s %s \n",argv[0],argv[1],argv[2],argv[3],argv[4]);
  if (argc < 5) {
    fprintf(stderr, "PLUGIN: This program should not be called from the command line.\n");
    return -1;
  }
  int myid = atoi(argv[3]);
  int mymaxchild = atoi(argv[4]);
  if (myid  < 0 || myid > mymaxchild ) {
      fprintf(stderr, "PLUGIN worker called with myid %d mymaxchild %d \n",myid,mymaxchild);
      return -1;
  }

  fprintf(stderr, "PLUGIN CHILD %d started for %s with shared memory %s.\n", myid, argv[1], argv[2]);

  if (open_file(argv[1]) < 0) {
    failed = 1;
  }

  struct sigaction action;
  memset (&action, '\0', sizeof(action));
  action.sa_sigaction=&sigcont_handler;
  action.sa_flags = SA_SIGINFO;
  if (sigaction(SIGCONT, &action, NULL) < 0) {
    fprintf(stderr,"sigaction failed\n");
    failed=1;
  }

  int child_shm_fd = shm_open(argv[2], O_RDWR, 0);
  if (child_shm_fd < 0) {
    fprintf(stderr, "PLUGIN CHILD %d: Failed to open shared memory %s.\n", myid, argv[2]);
    failed = 1;
  } else {
    GLOBAL_DATA->mapped_buf = mmap(0, sizeof(unsigned int) * GLOBAL_DATA->dimx * GLOBAL_DATA->dimy
                                   +sizeof(ShmTransferBuffer)*mymaxchild, 
                                   PROT_READ | PROT_WRITE, MAP_SHARED, child_shm_fd, 0);

    if (GLOBAL_DATA->mapped_buf == NULL) {
      fprintf(stderr, "PLUGIN CHILD %d: Failed to setup memory mapping.\n", myid);
      failed = 1;
    }    
    GLOBAL_DATA->mapped_transfer_buf = (ShmTransferBuffer *)GLOBAL_DATA->mapped_buf+sizeof(ShmTransferBuffer)*myid;
    GLOBAL_DATA->mapped_transfer_buf->cpid = getpid();
    GLOBAL_DATA->mapped_buf+=sizeof(ShmTransferBuffer)*mymaxchild;

  }
  if (failed != 0) {
    fprintf(stderr, "PLUGIN CHILD %d: Failed to start.\n", myid);
    exit(-1);
    // TODO: This kills the host program by 'broken pipe'. Should look for more 'gentle' way.
  }

  // Some programs do not call plugin_close :-<
  #ifdef HAVE_SYS_PRCTL_H
  signal(SIGUSR1, usr1_handler);
  prctl(PR_SET_PDEATHSIG, SIGUSR1); // TODO: this is not supported on Mac OS.
  #endif

  int frame_num;
  int terminate;
  int retrycount;
  terminate=0;
  retrycount=0;


  while (1) {
    while (1) {
      if (GLOBAL_DATA->mapped_transfer_buf->ptoc_flag_snd != GLOBAL_DATA->mapped_transfer_buf->ptoc_flag_rcv) {
        frame_num=GLOBAL_DATA->mapped_transfer_buf->ptoc_data;
        GLOBAL_DATA->mapped_transfer_buf->ptoc_flag_rcv = 1-GLOBAL_DATA->mapped_transfer_buf->ptoc_flag_snd;
        kill(GLOBAL_DATA->mapped_transfer_buf->ppid,SIGCONT);
        break;
      } else {
        if (++retrycount < 240) {
          struct timespec sleep = {0, 1000000000L}; nanosleep(&sleep, NULL);
          /* sleep(1); */
          kill(GLOBAL_DATA->mapped_transfer_buf->ppid,SIGCONT);
          if (retrycount%20 == 0)
            fprintf(stderr, "PLUGIN CHILD %d: parent to child data delayed %d sec.\n", 
              myid, retrycount); 
          continue;
        } else {
          fprintf(stderr, "PLUGIN CHILD %d: parent to child data failed\n", myid);
          terminate = -1;
          break;
        }
      }
    }

    if (frame_num == INVALID) break;
    if (terminate < 0) break;
    /* do the work */
    fprintf(stderr, "PLUGIN CHILD %d: got request for frame #%d.\n", myid, frame_num);
    int retval = get_data(myid, frame_num, GLOBAL_DATA->mapped_buf);

    retrycount=0;
    while(1) {
      if (GLOBAL_DATA->mapped_transfer_buf->ctop_flag_snd == GLOBAL_DATA->mapped_transfer_buf->ctop_flag_rcv) {
        GLOBAL_DATA->mapped_transfer_buf->ctop_data = retval;
        GLOBAL_DATA->mapped_transfer_buf->ctop_flag_snd == 1-GLOBAL_DATA->mapped_transfer_buf->ctop_flag_rcv;
        kill(GLOBAL_DATA->mapped_transfer_buf->ppid,SIGCONT);
        break;
      } else {
        if (++retrycount < 240) {
          struct timespec sleep = {0, 1000000000L}; nanosleep(&sleep, NULL);
          /* sleep(1); */
          kill(GLOBAL_DATA->mapped_transfer_buf->ppid,SIGCONT);
          if (retrycount%20 == 0)
            fprintf(stderr, "PLUGIN CHILD %d: write to parent delayed %d sec.\n", 
              myid, retrycount);
          continue;
        } else {
          fprintf(stderr, "PLUGIN CHILD %d: write to parent failed.\n", myid);
          terminate = -1;
          break;
        }
      }
    }

    if (terminate == -1) break;
    fprintf(stderr, "PLUGIN CHILD %d: processed frame #%d with retval %d.\n", myid, frame_num, retval);
   
  }

  munmap(GLOBAL_DATA->mapped_buf-sizeof(ShmTransferBuffer)*mymaxchild, 
    sizeof(unsigned int) * GLOBAL_DATA->dimx * GLOBAL_DATA->dimy+sizeof(ShmTransferBuffer)*mymaxchild);
  shm_unlink(argv[2]);
  free(GLOBAL_DATA -> minus1);
  free(GLOBAL_DATA -> minus2);
  fprintf(stderr, "PLUGIN CHILD %d: finished.\n", myid);
  exit(-1);
}
