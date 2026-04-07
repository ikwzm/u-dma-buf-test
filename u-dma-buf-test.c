#include        <stdio.h>
#include        <fcntl.h>
#include        <string.h>
#include        <time.h>
#include        <stdlib.h>
#include        <unistd.h>
#include        <getopt.h>
#include        <sys/time.h>
#include        <sys/types.h>
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

int check_buf(unsigned char* buf, unsigned int size)
{
    int m = 256;
    int n = 10;
    int i, k;
    int error_count = 0;
    while(--n > 0) {
      for(i = 0; i < size; i = i + m) {
        m = (i+256 < size) ? 256 : (size-i);
        for(k = 0; k < m; k++) {
          buf[i+k] = (k & 0xFF);
        }
        for(k = 0; k < m; k++) {
          if (buf[i+k] != (k & 0xFF)) {
            error_count++;
          }
        }
      }
    }
    return error_count;
}

int clear_buf(unsigned char* buf, unsigned int size)
{
    int n = 100;
    int error_count = 0;
    while(--n > 0) {
      memset((void*)buf, 0xFF, size);
    }
    return error_count;
}

void check_buf_test(const char* name, size_t size, unsigned int sync_mode, int o_sync)
{
    int            fd;
    char           file_name[1024];
    char           attr[1024];
    struct timeval start_time, end_time;
    int            error_count;
    unsigned char* buf;

    sprintf(file_name, "/sys/class/u-dma-buf/%s/sync_mode", name);
    if ((fd  = open(file_name, O_WRONLY)) != -1) {
      sprintf(attr, "%d", sync_mode);
      write(fd, attr, strlen(attr));
      close(fd);
    } else {
      perror(file_name);
      exit(-1);
    }

    printf("sync_mode=%d, O_SYNC=%d, ", sync_mode, (o_sync)?1:0);

    sprintf(file_name, "/dev/%s", name);
    if ((fd  = open(file_name, O_RDWR | o_sync)) != -1) {
      buf = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
      gettimeofday(&start_time, NULL);
      error_count = check_buf(buf, size);
      gettimeofday(&end_time  , NULL);
      print_diff_time(start_time, end_time);
      close(fd);
    } else {
      perror(file_name);
      exit(-1);
    }
}

void clear_buf_test(char* name, size_t size, unsigned int sync_mode, int o_sync)
{
    int            fd;
    char           file_name[1024];
    char           attr[1024];
    struct timeval start_time, end_time;
    int            error_count;
    unsigned char* buf;

    sprintf(file_name, "/sys/class/u-dma-buf/%s/sync_mode", name);
    if ((fd  = open(file_name, O_WRONLY)) != -1) {
      sprintf(attr, "%d", sync_mode);
      write(fd, attr, strlen(attr));
      close(fd);
    } else {
      perror(file_name);
      exit(-1);
    }

    printf("sync_mode=%d, O_SYNC=%d, ", sync_mode, (o_sync)?1:0);

    sprintf(file_name, "/dev/%s", name);
    if ((fd  = open(file_name, O_RDWR | o_sync)) != -1) {
      buf = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
      gettimeofday(&start_time, NULL);
      error_count = clear_buf(buf, size);
      gettimeofday(&end_time  , NULL);
      print_diff_time(start_time, end_time);
      close(fd);
    } else {
      perror(file_name);
      exit(-1);
    }
}

void read_buf_test(char* name, size_t size, int o_sync)
{
    int            fd;
    char           file_name[1024];
    struct timeval start_time, end_time;
    int            error_count;
    unsigned char* buf;

    sprintf(file_name, "/dev/%s", name);
    if ((fd  = open(file_name, O_RDWR | o_sync)) != -1) {
      if ((buf = malloc(size)) != NULL) {
        gettimeofday(&start_time, NULL);
        read(fd, (void*)buf, size);
        gettimeofday(&end_time  , NULL);
        print_diff_time(start_time, end_time);
        free(buf);
        close(fd);
      }
    } else {
      perror(file_name);
      exit(-1);
    }
}

int main(int argc, char* argv[])
{
    int            fd;
    char           name[256];
    char           attr[1024];
    char           file_name[1024];
    size_t         buf_size;
    unsigned long  phys_addr;
    unsigned long  debug_vma       = 0;
    unsigned long  sync_mode       = 2;
    int            dma_coherent    = -1;
    int            quirk_mmap_mode = -1;
    char*          driver_version  = NULL;
    int            verbose         = 0;
    int            opt;
    int            optindex;
    struct option  longopts[] = {
      { "name"      , required_argument, NULL, 'n'},
      { "verbose"   , no_argument      , NULL, 'v'},
      { NULL        , 0                , NULL,  0 },
    };

    strncpy(name, "udmabuf0", sizeof(name));
    while ((opt = getopt_long(argc, argv, "n:", longopts, &optindex)) != -1) {
      switch (opt) {
        case 'n':
          strncpy(name, optarg, sizeof(name));
          break;
        case 'v':
          verbose = 1;
          break;
        default:
          printf("error options\n");
          break;
      }
    }
    printf("device=%s\n", name);

    sprintf(file_name, "/sys/class/u-dma-buf/%s/driver_version", name);
    if ((fd  = open(file_name, O_RDONLY)) != -1) {
      int len;
      len = read(fd, attr, 1024);
      while(--len >= 0) {
        if (attr[len] =='\n') {
          attr[len] = '\0';
          break;
        }
      }
      driver_version = strdup(attr);
      close(fd);
    } else {
      perror(file_name);
      exit(-1);
    }

    sprintf(file_name, "/sys/class/u-dma-buf/%s/phys_addr", name);
    if ((fd  = open(file_name, O_RDONLY)) != -1) {
      read(fd, attr, 1024);
      sscanf(attr, "%lx", &phys_addr);
      close(fd);
    } else {
      perror(file_name);
      exit(-1);
    }

    sprintf(file_name, "/sys/class/u-dma-buf/%s/size", name);
    if ((fd  = open(file_name, O_RDONLY)) != -1) {
      read(fd, attr, 1024);
      sscanf(attr, "%ld", &buf_size);
      close(fd);
    } else {
      perror(file_name);
      exit(-1);
    }

    sprintf(file_name, "/sys/class/u-dma-buf/%s/sync_mode", name);
    if ((fd  = open(file_name, O_WRONLY)) != -1) {
      sprintf(attr, "%ld", sync_mode);
      write(fd, attr, strlen(attr));
      close(fd);
    } else {
      perror(file_name);
      exit(-1);
    }

    sprintf(file_name, "/sys/class/u-dma-buf/%s/debug_vma", name);
    if ((fd  = open(file_name, O_WRONLY)) != -1) {
      sprintf(attr, "%ld", debug_vma);
      write(fd, attr, strlen(attr));
      close(fd);
    }

    sprintf(file_name, "/sys/class/u-dma-buf/%s/dma_coherent", name);
    if ((fd  = open(file_name, O_RDONLY)) != -1) {
      read(fd, attr, 1024);
      sscanf(attr, "%d", &dma_coherent);
      close(fd);
    }
    
    sprintf(file_name, "/sys/class/u-dma-buf/%s/quirk_mmap_mode", name);
    if ((fd  = open(file_name, O_RDONLY)) != -1) {
      read(fd, attr, 1024);
      sscanf(attr, "%d", &quirk_mmap_mode);
      close(fd);
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
    printf("phys_addr=0x%lx\n", phys_addr);
    printf("size=%ld\n", buf_size);

    sprintf(file_name, "/dev/%s", name);
    if ((fd  = open(file_name, O_RDWR)) != -1) {
      long last_pos = lseek(fd, 0, 2);
      if (last_pos == -1) {
        printf("lseek error\n");
        exit(-1);
      }
      close(fd);
    } else {
      perror(file_name);
      exit(-1);
    }

    printf("check_buf()\n");
    check_buf_test(name, buf_size, 0, 0);
    check_buf_test(name, buf_size, 0, O_SYNC);
    check_buf_test(name, buf_size, 1, 0);
    check_buf_test(name, buf_size, 1, O_SYNC);
    check_buf_test(name, buf_size, 2, 0);
    check_buf_test(name, buf_size, 2, O_SYNC);
    check_buf_test(name, buf_size, 3, 0);
    check_buf_test(name, buf_size, 3, O_SYNC);
    check_buf_test(name, buf_size, 4, 0);
    check_buf_test(name, buf_size, 4, O_SYNC);
    check_buf_test(name, buf_size, 5, 0);
    check_buf_test(name, buf_size, 5, O_SYNC);
    check_buf_test(name, buf_size, 6, 0);
    check_buf_test(name, buf_size, 6, O_SYNC);
    check_buf_test(name, buf_size, 7, 0);
    check_buf_test(name, buf_size, 7, O_SYNC);

    printf("clear_buf()\n");
    clear_buf_test(name, buf_size, 0, 0);
    clear_buf_test(name, buf_size, 0, O_SYNC);
    clear_buf_test(name, buf_size, 1, 0);
    clear_buf_test(name, buf_size, 1, O_SYNC);
    clear_buf_test(name, buf_size, 2, 0);
    clear_buf_test(name, buf_size, 2, O_SYNC);
    clear_buf_test(name, buf_size, 3, 0);
    clear_buf_test(name, buf_size, 3, O_SYNC);
    clear_buf_test(name, buf_size, 4, 0);
    clear_buf_test(name, buf_size, 4, O_SYNC);
    clear_buf_test(name, buf_size, 5, 0);
    clear_buf_test(name, buf_size, 5, O_SYNC);
    clear_buf_test(name, buf_size, 6, 0);
    clear_buf_test(name, buf_size, 6, O_SYNC);
    clear_buf_test(name, buf_size, 7, 0);
    clear_buf_test(name, buf_size, 7, O_SYNC);
}
