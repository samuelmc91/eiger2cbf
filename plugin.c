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

#define _GNU_SOURCE 

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <pthread.h>

#ifdef __linux
 #include <sys/prctl.h>
 #include <linux/limits.h>
#endif

#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <H5Ppublic.h>
#include "H5api_adpt.h"
#include "hdf5_hl.h"
#include "hdf5.h"

#define INVALID -9999

#define MAXCHILD 16
#define DEFAULT_NCHILD 4

void emergency_close( void );
int did_close=1;

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
  char filename[300];
  char shm_names[MAXCHILD][NAME_MAX];
  hid_t hdf, group;
  int dimx, dimy;
  int Nminus1, Nminus2;
  int datasize;
  int nframesPerDataset;
  float xpixelSize;
  float ypixelSize;
  int block_start;
  unsigned int error_val;
  int nchild;
  pthread_mutex_t locks[MAXCHILD];
  ShmTransferBuffer* mapped_transfer_bufs[MAXCHILD];
  unsigned int *mapped_bufs[MAXCHILD];
};
struct GlobalData *GLOBAL_DATA = NULL;

static void sigcont_handler (int signal, siginfo_t *siginfo, void * context) {}

void plugin_get_header(int *nx, int *ny, int *nbytes, float *qx, float *qy,
                       int *number_of_frames, int info[1024],
                       int *error_flag);
void child_loop(int myid);

void plugin_open(const char *filename, int info_array[1024], int *error_flag) {
  struct sigaction action;
  char fn[4096];
  int n;
  register_filters();

  memset (&action, '\0', sizeof(action));
  action.sa_sigaction=&sigcont_handler;
  action.sa_flags = SA_SIGINFO;
  if (sigaction(SIGCONT, &action, NULL) < 0) {   
    fprintf(stderr,"sigaction failed\n");
    *error_flag = -1;
    return;
  }



  for(n=0;n<4096 && filename[n] != '\0';n++) {fn[n]=filename[n];}
  fn[4095]='\0';
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

  /* Decide the number of children */
  char *env_nchild = getenv("PLUGIN_NCHILD"); // Do not free!
  if (env_nchild == NULL) {
    GLOBAL_DATA->nchild = DEFAULT_NCHILD;
  } else {
    GLOBAL_DATA->nchild = atoi(env_nchild);
    if (GLOBAL_DATA->nchild > MAXCHILD) {
      fprintf(stderr, "PLUGIN WARNING: The maximum number of the child processes is limited to %d.\n", MAXCHILD);
      GLOBAL_DATA->nchild = MAXCHILD;
    }
  }
  fprintf(stderr, "PLUGIN INFO: Running with %d child processes.\n", GLOBAL_DATA->nchild);

  int nx, ny, nbytes, nframes, info[1024], dummy;
  float qx, qy;
  plugin_get_header(&nx, &ny, &nbytes, &qx, &qy, &nframes, info, &dummy);

  /* Setup and start child processes */
  int ppid = getpid();
  char child_id[16];
  char maxchild_str[16];

  for (int i = 0; i < GLOBAL_DATA->nchild; i++) {
    snprintf(child_id, 16, "%d", i);
    snprintf(maxchild_str, 16, "%d", MAXCHILD);
    pthread_mutex_init(&GLOBAL_DATA->locks[i], NULL);

    /* allocate shared memory and setup memory mapping */
    snprintf(GLOBAL_DATA->shm_names[i], NAME_MAX, "/plugin%d_%d.shm", ppid, i);
    int shm_fd = shm_open(GLOBAL_DATA->shm_names[i], O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (shm_fd == -1) {
      fprintf(stderr, "PLUGIN ERROR: failed to create shared memory %s.\n", GLOBAL_DATA->shm_names[i]);
      *error_flag = -2;
      return;
    }
    if (ftruncate(shm_fd, sizeof(unsigned int) * nx * ny + sizeof(ShmTransferBuffer)) < 0) {
      fprintf(stderr, "PLUGIN ERROR: failed to set the size of shared memory %s.\n", GLOBAL_DATA->shm_names[i]);
      *error_flag = -2;
      return;
    }
    GLOBAL_DATA->mapped_transfer_bufs[i]=(ShmTransferBuffer *)mmap(0, (sizeof(unsigned int) * nx * ny + sizeof(ShmTransferBuffer)), 
                                          PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    GLOBAL_DATA->mapped_transfer_bufs[i]->ptoc_flag_snd=0;
    GLOBAL_DATA->mapped_transfer_bufs[i]->ptoc_flag_rcv=0;
    GLOBAL_DATA->mapped_transfer_bufs[i]->ctop_flag_snd=0;
    GLOBAL_DATA->mapped_transfer_bufs[i]->ctop_flag_rcv=0;
    GLOBAL_DATA->mapped_bufs[i] = (unsigned int *)(GLOBAL_DATA->mapped_transfer_bufs[i]+sizeof(ShmTransferBuffer));
    // MAP_HUGETLB | 25 << MAP_HUGE_SHIFT causes SEGV in HDF5 lib
    if (GLOBAL_DATA->mapped_bufs[i] == NULL) {
      fprintf(stderr, "PLUGIN ERROR: failed to mmap %s\n", GLOBAL_DATA->shm_names[i]);
      *error_flag = -2;
      return;
    }

    /* start child process */
    GLOBAL_DATA->mapped_transfer_bufs[i]->ppid=getpid();
    int pid = fork();
    if (pid < 0) {
      fprintf(stderr, "PLUGIN ERROR: Failed to create subprocess.\n");
      *error_flag = -2;
      return;
    }
    if (pid == 0) {
      /* This is a child */
      execlp("eiger2cbf-so-worker", "eiger2cbf-so-plugin-worker", fn, GLOBAL_DATA->shm_names[i], child_id, maxchild_str, NULL);
      fprintf(stderr, "PLUGIN CHILD: Failed to launch eiger2cbf-so-worker. Is it in the PATH?\n");
      exit(-1);
    } else {
      /* This is the parent */
      GLOBAL_DATA->mapped_transfer_bufs[i]->cpid=pid;
    }
  }

  did_close = 0;
  atexit(emergency_close);

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

void plugin_get_data(int *frame_number, int *nx, int *ny,
		     int data_array[], int info_array[1024],
		     int *error_flag) {
  if (GLOBAL_DATA == NULL){
    fprintf(stderr, "PLUGIN ERROR: plugin_get_data called before plugin_open.\n");
    *error_flag = -4;
    return;
  }

  int child_id = *frame_number % GLOBAL_DATA->nchild;
  int retrycount;
  int terminate;
  int retval;
  terminate=0;
  pthread_mutex_lock(&GLOBAL_DATA->locks[child_id]);
  fprintf(stderr, "PLUGIN PARENT: get_data for frame #%d delegated to child #%d.\n", *frame_number, child_id);
  while (1) {
    if (GLOBAL_DATA->mapped_transfer_bufs[child_id]->ptoc_flag_snd == GLOBAL_DATA->mapped_transfer_bufs[child_id]->ptoc_flag_rcv) {
      GLOBAL_DATA->mapped_transfer_bufs[child_id]->ptoc_data = *frame_number;
      GLOBAL_DATA->mapped_transfer_bufs[child_id]->ptoc_flag_snd == 1-GLOBAL_DATA->mapped_transfer_bufs[child_id]->ptoc_flag_rcv;
      kill(GLOBAL_DATA->mapped_transfer_bufs[child_id]->cpid,SIGCONT);
      break;
    } else {
      if (++retrycount < 240) {
        struct timespec sleep = {0, 1000000000L}; nanosleep(&sleep, NULL);
        /* sleep(1); */
        kill(GLOBAL_DATA->mapped_transfer_bufs[child_id]->cpid,SIGCONT);
        if (retrycount%20 == 0)
          fprintf(stderr, "PLUGIN CHILD %d: write from parent delayed %d sec.\n", 
            child_id, retrycount);
        continue;
      } else {
        fprintf(stderr, "PLUGIN CHILD %d: write from parent failed.\n", child_id);
        terminate = -1;
        retval = -4;
        break;
      }
    }
  }


  while (1) {
    if (GLOBAL_DATA->mapped_transfer_bufs[child_id]->ctop_flag_snd != GLOBAL_DATA->mapped_transfer_bufs[child_id]->ctop_flag_rcv) {
      retval=GLOBAL_DATA->mapped_transfer_bufs[child_id]->ctop_data;
      GLOBAL_DATA->mapped_transfer_bufs[child_id]->ctop_flag_rcv == 1-GLOBAL_DATA->mapped_transfer_bufs[child_id]->ctop_flag_snd;
      kill(GLOBAL_DATA->mapped_transfer_bufs[child_id]->cpid,SIGCONT);
      fprintf(stderr, "PLUGIN PARENT: received %d for frame #%d from child #%d.\n", retval, *frame_number, child_id);
      if (retval == 0) {
        memcpy(data_array, GLOBAL_DATA->mapped_bufs[child_id], sizeof(unsigned int) * GLOBAL_DATA->dimx * GLOBAL_DATA->dimy);
      } 
      break;
    } else {
      if (++retrycount < 240) {
        struct timespec sleep = {0, 1000000000L}; nanosleep(&sleep, NULL);
        /* sleep(1); */
        kill(GLOBAL_DATA->mapped_transfer_bufs[child_id]->cpid,SIGCONT);
        if (retrycount%20 == 0)
          fprintf(stderr, "PLUGIN CHILD %d: return data delayed %d sec.\n", 
            child_id, retrycount);
        continue;
      } else {
        fprintf(stderr, "PLUGIN CHILD %d: return data failed.\n", child_id);
        terminate = -1;
        retval = -4;
        break;
      }
    }
  }

  pthread_mutex_unlock(&GLOBAL_DATA->locks[child_id]);

  *error_flag = retval;
  return;
}

void plugin_close(int *error_flag){
  printf("PLUGIN PARENT: plugin_close called.\n");

  for (int i = 0; i < GLOBAL_DATA->nchild; i++) {
    pthread_mutex_lock(&GLOBAL_DATA->locks[i]);
    fprintf(stderr, "PLUGIN PARENT: undelegate to child #%d.\n", i);
    int val = INVALID;
    int retrycount;
    while (1) {
      if (GLOBAL_DATA->mapped_transfer_bufs[i]->ptoc_flag_snd == GLOBAL_DATA->mapped_transfer_bufs[i]->ptoc_flag_rcv) {
        GLOBAL_DATA->mapped_transfer_bufs[i]->ptoc_data = val;
        GLOBAL_DATA->mapped_transfer_bufs[i]->ptoc_flag_snd == 1-GLOBAL_DATA->mapped_transfer_bufs[i]->ptoc_flag_rcv;
        kill(GLOBAL_DATA->mapped_transfer_bufs[i]->cpid,SIGCONT);
        break;
      } else {
        if (++retrycount < 240) {
        struct timespec sleep = {0, 1000000000L}; nanosleep(&sleep, NULL);
        /* sleep(1); */
        kill(GLOBAL_DATA->mapped_transfer_bufs[i]->cpid,SIGCONT);
        if (retrycount%20 == 0)
            fprintf(stderr, "PLUGIN CHILD %d: write from parent delayed %d sec.\n", 
              i, retrycount);
          continue;
        } else {
          fprintf(stderr, "PLUGIN ERROR: cannot write to child #%d for exit.\n", i);
        }
      }
    }
    pthread_mutex_unlock(&GLOBAL_DATA->locks[i]);
    munmap(GLOBAL_DATA->mapped_transfer_bufs[i], sizeof(unsigned int) * GLOBAL_DATA->dimx * GLOBAL_DATA->dimy+sizeof(ShmTransferBuffer));
    shm_unlink(GLOBAL_DATA->shm_names[i]);
  }
  did_close=1;
}

void emergency_close( void ){
    int error_flag;
    if (!did_close) plugin_close(&error_flag);
}

