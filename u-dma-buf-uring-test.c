#define         _GNU_SOURCE
#include        <stdio.h>
#include        <fcntl.h>
#include        <string.h>
#include        <time.h>
#include        <stdlib.h>
#include        <errno.h>
#include        <unistd.h>
#include        <getopt.h>
#include        <liburing.h>
#include        <sys/time.h>
#include        <sys/types.h>
#include        <sys/stat.h>
#include        <sys/mman.h>
#include        <sys/utsname.h>

static void diff_time(struct timeval* run_time, struct timeval* start_time, struct timeval* end_time)
{
    if (end_time->tv_usec < start_time->tv_usec) {
        run_time->tv_sec  = end_time->tv_sec  - start_time->tv_sec  - 1;
        run_time->tv_usec = end_time->tv_usec - start_time->tv_usec + 1000*1000;
    } else {
        run_time->tv_sec  = end_time->tv_sec  - start_time->tv_sec ;
        run_time->tv_usec = end_time->tv_usec - start_time->tv_usec;
    }
}

void write_buf_test(void* udmabuf_map, unsigned int udmabuf_size, int output_fd, struct timeval* run_time)
{
    struct timeval start_time, end_time;
    char*          dump_buf  = (char*)udmabuf_map;
    ssize_t        dump_size = udmabuf_size;

    gettimeofday(&start_time, NULL);

    struct io_uring ring;
    io_uring_queue_init(8, &ring, 0);

    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    io_uring_prep_write(sqe, output_fd, dump_buf, dump_size, 0);

    io_uring_submit(&ring);

    struct io_uring_cqe* cqe;
    io_uring_wait_cqe(&ring, &cqe);
    if (cqe->res < 0) {
      printf("write error(%d=%s)\n", -cqe->res, strerror(-cqe->res));
      exit(-1);
    }
    io_uring_cqe_seen(&ring, cqe);

    gettimeofday(&end_time  , NULL);
    diff_time(run_time, &start_time, &end_time);
    printf("time = %ld.%06ld sec\n", run_time->tv_sec, run_time->tv_usec);
}

uint32_t xorshift32(uint32_t* state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x <<  5;
    return *state = x;
}

void main(int argc, char* argv[])
{
    char           device_name[256];
    char           file_name[1024];
    char           attr[1024];
    int            udmabuf_fd;
    void*          udmabuf_map;
    unsigned int   udmabuf_size;
    int            dma_coherent    = -1;
    int            quirk_mmap_mode = -1;
    char*          driver_version  = NULL;
    int            try_count       = 10;
    uint32_t       random_seed     = 2463534242;
    int            verbose         = 0;
    struct timeval run_time;
    uint64_t       total_usec;
    uint64_t       total_size;
    char           output_name[256];
    int            output_fd;
    int            opt;
    int            optindex;
    struct option  longopts[] = {
      { "name"      , required_argument, NULL, 'n'},
      { "output"    , required_argument, NULL, 'o'},
      { "try"       , required_argument, NULL, 't'},
      { "verbose"   , no_argument      , NULL, 'v'},
      { NULL        , 0                , NULL,  0 },
    };

    strncpy(device_name, "udmabuf0"     , sizeof(device_name));
    strncpy(output_name, "dump_file.dat", sizeof(output_name));
    while ((opt = getopt_long(argc, argv, "n:o:t:v", longopts, &optindex)) != -1) {
      switch (opt) {
        case 'n':
          strncpy(device_name, optarg, sizeof(device_name));
          break;
        case 'o':
          strncpy(output_name, optarg, sizeof(output_name));
          break;
        case 't':
          if (sscanf(optarg, "%d", &try_count) != 1) {
              printf("error options -t %s\n", optarg);
          }
          break;
        case 'v':
          verbose = 1;
          break;
        default:
          printf("error options\n");
          break;
      }
    }
    printf("device=%s\n", device_name);

    sprintf(file_name, "/sys/class/u-dma-buf/%s/driver_version", device_name);
    if ((udmabuf_fd  = open(file_name, O_RDONLY)) != -1) {
      int len;
      len = read(udmabuf_fd, attr, 1024);
      while(--len >= 0) {
        if (attr[len] =='\n') {
          attr[len] = '\0';
          break;
        }
      }
      driver_version = strdup(attr);
      close(udmabuf_fd);
    } else {
      perror(file_name);
      exit(-1);
    }

    sprintf(file_name, "/sys/class/u-dma-buf/%s/size", device_name);
    if ((udmabuf_fd  = open(file_name, O_RDONLY)) != -1) {
      read(udmabuf_fd, attr, 1024);
      sscanf(attr, "%d", &udmabuf_size);
      close(udmabuf_fd);
    } else {
      perror(file_name);
      exit(-1);
    }

    sprintf(file_name, "/sys/class/u-dma-buf/%s/dma_coherent", device_name);
    if ((udmabuf_fd  = open(file_name, O_RDONLY)) != -1) {
      read(udmabuf_fd, attr, 1024);
      sscanf(attr, "%d", &dma_coherent);
      close(udmabuf_fd);
    }

    sprintf(file_name, "/sys/class/u-dma-buf/%s/quirk_mmap_mode", device_name);
    if ((udmabuf_fd  = open(file_name, O_RDONLY)) != -1) {
      read(udmabuf_fd, attr, 1024);
      sscanf(attr, "%d", &quirk_mmap_mode);
      close(udmabuf_fd);
    }

    if (driver_version)
      printf("driver_version=%s\n", driver_version);
    if (dma_coherent >= 0)
      printf("dma_coherent=%d\n", dma_coherent);
    if (quirk_mmap_mode == 0)
      printf("quirk_mmap=undefined\n");
    if (quirk_mmap_mode == 1)
      printf("quirk_mmap=off\n");
    if (quirk_mmap_mode == 2)
      printf("quirk_mmap=on\n");
    if (quirk_mmap_mode == 3)
      printf("quirk_mmap=auto\n");
    if (quirk_mmap_mode == 4)
      printf("quirk_mmap=page\n");
    printf("size=%d\n", udmabuf_size);
    printf("output=%s\n", output_name);

    sprintf(file_name, "/dev/%s", device_name);
    if ((udmabuf_fd  = open(file_name, O_RDWR)) == -1) {
      perror(file_name);
      exit(-1);
    }

    udmabuf_map = mmap(NULL, udmabuf_size, PROT_READ|PROT_WRITE, MAP_SHARED, udmabuf_fd, 0);
    if (udmabuf_map == MAP_FAILED) {
      printf("can not mmap\n");
      exit(-1);
    } else {
      uint32_t  state     = random_seed;
      uint32_t* word_buf  = (uint32_t*)udmabuf_map;
      int       pos       = 0;
      int       words     = udmabuf_size/sizeof(uint32_t);
      for(pos = 0; pos < words; pos++) {
        word_buf[pos] = xorshift32(&state);
      }
    }

    printf("write_buf_test(size=%d, O_DIRECT=0)\n", udmabuf_size);
    output_fd = open(output_name, O_CREAT | O_WRONLY | O_SYNC);
    if (output_fd == -1) {
        printf("can not open %s\n", output_name);
        exit(-1);
    }
    fallocate(output_fd, 0, 0, udmabuf_size);
    total_size = 0;
    total_usec = 0;
    for (int i = 0; i < try_count; i++) {
        write_buf_test(udmabuf_map, udmabuf_size, output_fd, &run_time);
        total_size += udmabuf_size;
        total_usec += ((int64_t)run_time.tv_sec*(1000*1000) +
                       (int64_t)run_time.tv_usec);
    }
    printf("throughput=%5.1f MBytes/sec\n", (double)total_size/((double)total_usec));
    close(output_fd);

    printf("write_buf_test(size=%d, O_DIRECT=1)\n", udmabuf_size);
    output_fd = open(output_name, O_CREAT | O_WRONLY | O_SYNC | O_DIRECT);
    if (output_fd == -1) {
        printf("can not open %s\n", output_name);
        exit(-1);
    }
    fallocate(output_fd, 0, 0, udmabuf_size);
    total_size = 0;
    total_usec = 0;
    for (int i = 0; i < try_count; i++) {
        write_buf_test(udmabuf_map, udmabuf_size, output_fd, &run_time);
        total_size += udmabuf_size;
        total_usec += ((int64_t)run_time.tv_sec*(1000*1000) +
                       (int64_t)run_time.tv_usec);
    }
    printf("throughput=%5.1f MBytes/sec\n", (double)total_size/((double)total_usec));
    close(output_fd);

    munmap(udmabuf_map, udmabuf_size);
    close(udmabuf_fd);
}
