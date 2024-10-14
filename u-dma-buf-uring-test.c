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

void print_diff_time(struct timeval start_time, struct timeval end_time)
{
    struct timeval diff_time;
    if (end_time.tv_usec < start_time.tv_usec) {
        diff_time.tv_sec  = end_time.tv_sec  - start_time.tv_sec  - 1;
        diff_time.tv_usec = end_time.tv_usec - start_time.tv_usec + 1000*1000;
    } else {
        diff_time.tv_sec  = end_time.tv_sec  - start_time.tv_sec ;
        diff_time.tv_usec = end_time.tv_usec - start_time.tv_usec;
    }
    printf("time = %ld.%06ld sec\n", diff_time.tv_sec, diff_time.tv_usec);
}

void write_buf_test(void* udmabuf_map, unsigned int udmabuf_size)
{
    struct timeval start_time, end_time;
    int*           word_buf  = (int*)udmabuf_map;
    int            word_pos  = 0;
    int            word_size = udmabuf_size/sizeof(int);
    for(word_pos = 0; word_pos < word_size; word_pos++) {
        word_buf[word_pos] = 0;
    }

    int            dump_fd;
    char*          dump_buf  = (char*)udmabuf_map;
    ssize_t        dump_size = udmabuf_size;
    if ((dump_fd = open("dump_file.dat", O_CREAT | O_WRONLY)) == -1) {
        printf("can not open %s\n", "dump_file.dat");
        exit(-1);
    }

    gettimeofday(&start_time, NULL);

    struct io_uring ring;
    io_uring_queue_init(8, &ring, 0);

    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    io_uring_prep_write(sqe, dump_fd, dump_buf, dump_size, 0);

    io_uring_submit(&ring);

    struct io_uring_cqe* cqe;
    io_uring_wait_cqe(&ring, &cqe);
    io_uring_cqe_seen(&ring, cqe);

    gettimeofday(&end_time  , NULL);
    print_diff_time(start_time, end_time);
    close(dump_fd);
}

void main(int argc, char* argv[])
{
    char           device_name[256];
    char           file_name[1024];
    unsigned char  attr[1024];
    int            udmabuf_fd;
    void*          udmabuf_map;
    unsigned int   udmabuf_size;
    int            dma_coherent    = -1;
    int            quirk_mmap_mode = -1;
    char*          driver_version  = NULL;
    int            try_count       = 10;
    int            verbose         = 0;
    int            opt;
    int            optindex;
    struct option  longopts[] = {
      { "name"      , required_argument, NULL, 'n'},
      { "t"         , required_argument, NULL, 't'},
      { "verbose"   , no_argument      , NULL, 'v'},
      { NULL        , 0                , NULL,  0 },
    };

    strncpy(device_name, "udmabuf0", sizeof(device_name));
    while ((opt = getopt_long(argc, argv, "n:", longopts, &optindex)) != -1) {
      switch (opt) {
        case 'n':
          strncpy(device_name, optarg, sizeof(device_name));
          break;
        case 't':
          scanf(optarg, "%d", &try_count);
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

    sprintf(file_name, "/dev/%s", device_name);
    if ((udmabuf_fd  = open(file_name, O_RDWR)) == -1) {
      perror(file_name);
      exit(-1);
    }

    udmabuf_map = mmap(NULL, udmabuf_size, PROT_READ|PROT_WRITE, MAP_SHARED, udmabuf_fd, 0);
    if (udmabuf_map == MAP_FAILED) {
      printf("can not mmap\n");
      exit(-1);
    }

    printf("write_buf_test(size=%d)\n", udmabuf_size);
    for (int i = 0; i < try_count; i++) {
        write_buf_test(udmabuf_map, udmabuf_size);
    }

    munmap(udmabuf_map, udmabuf_size);
    close(udmabuf_fd);
}
