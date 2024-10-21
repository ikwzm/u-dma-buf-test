#include        <stdio.h>
#include        <fcntl.h>
#include        <string.h>
#include        <time.h>
#include        <stdlib.h>
#include        <unistd.h>
#include        <getopt.h>
#include        <errno.h>
#include        <sys/ioctl.h>
#include        <sys/time.h>
#include        <sys/types.h>
#include        <sys/mman.h>
#include        <sys/utsname.h>
#include        <inttypes.h>

struct u_dma_buf
{
    char*  name;
    char*  dev_name;
    char*  sys_path;
    char*  version;
    size_t size;
    int    dma_coherent;
    int    sync_for_cpu_file;
    int    sync_for_dev_file;
    char   sync_command[1024];
    int    sync_command_len;
};
const  int  U_DMA_BUF_READ_WRITE  =  0;
const  int  U_DMA_BUF_WRITE_ONLY  =  1;
const  int  U_DMA_BUF_READ_ONLY   =  2;

void u_dma_buf_destroy(struct u_dma_buf* this)
{
    if (this == NULL)
        return;
    
    if (this->sync_for_cpu_file >= 0) close(this->sync_for_cpu_file);
    if (this->sync_for_dev_file >= 0) close(this->sync_for_dev_file);
    if (this->name     != NULL) free(this->name);
    if (this->dev_name != NULL) free(this->dev_name);
    if (this->sys_path != NULL) free(this->sys_path);
    if (this->version  != NULL) free(this->version);
    free(this);
}

struct u_dma_buf* u_dma_buf_create(char* name)
{
    struct u_dma_buf*  this;
    char               file_name[1024];
    char               attr[1024];
    int                str_len;
    int                fd;

    if ((this = calloc(1, sizeof(struct u_dma_buf))) == NULL) {
        printf("Can not alloc u_dma_buf\n");
        goto failed;
    }
    this->sync_for_cpu_file = -1;
    this->sync_for_dev_file = -1;
    
    if ((this->name = strdup(name)) == NULL) {
        printf("Can not alloc this->name\n");
        goto failed;
    }
    str_len = sprintf(file_name, "/dev/%s", this->name);
    if ((this->dev_name = strdup(file_name)) == NULL) {
        printf("Can not alloc this->dev_name\n");
        goto failed;
    }
    str_len = sprintf(file_name, "/sys/class/u-dma-buf/%s", this->name);
    if ((this->sys_path = strdup(file_name)) == NULL) {
        printf("Can not alloc this->sys_path\n");
        goto failed;
    }
    str_len = sprintf(file_name, "%s/size", this->sys_path);
    if ((fd = open(file_name, O_RDONLY)) != -1) {
        read(fd, attr, 1024);
        sscanf(attr, "%ld", &this->size);
        close(fd);
    } else {
        printf("Can not open %s\n", file_name);
        goto failed;
    } 
    str_len = sprintf(file_name, "%s/driver_version", this->sys_path);
    if ((fd = open(file_name, O_RDONLY)) != -1) {
        int len;
        len = read(fd, attr, 1024);
        while(--len >= 0) {
          if (attr[len] =='\n') {
            attr[len] = '\0';
            break;
          }
        }
        this->version = strdup(attr);
        close(fd);
    } else {
        printf("Can not open %s\n", file_name);
        goto failed;
    } 
    str_len = sprintf(file_name, "%s/dma_coherent", this->sys_path);
    if ((fd = open(file_name, O_RDONLY)) != -1) {
        read(fd, attr, 1024);
        sscanf(attr, "%d", &this->dma_coherent);
        close(fd);
    } else {
        printf("Can not open %s\n", file_name);
        goto failed;
    } 
    str_len = sprintf(file_name, "%s/sync_for_cpu", this->sys_path);
    if ((fd = open(file_name, O_RDWR)) != -1) {
        this->sync_for_cpu_file = fd;
    } else {
        printf("Can not open %s\n", file_name);
        goto failed;
    } 
    str_len = sprintf(file_name, "%s/sync_for_device", this->sys_path);
    if ((fd = open(file_name, O_RDWR)) != -1) {
        this->sync_for_dev_file = fd;
    } else {
        printf("Can not open %s\n", file_name);
        goto failed;
    }
    return this;
      
  failed:
    u_dma_buf_destroy(this);
    return NULL;
}

int  u_dma_buf_open(struct u_dma_buf* this, int flags)
{
    return open(this->dev_name, flags);
}

void u_dma_buf_set_sync_area(struct u_dma_buf* this, unsigned int offset, unsigned int size, int direction)
{
    this->sync_command_len = 
        sprintf(this->sync_command, "0x%08X%08X\n",
                offset,
               ((size & 0xFFFFFFF0) | (direction << 2) | 1));
}

size_t u_dma_buf_sync_for_cpu(struct u_dma_buf* this)
{
    if (this->sync_command_len > 0)
        return write(this->sync_for_cpu_file,
                     this->sync_command,
                     this->sync_command_len);
    else
        return 0;
}

size_t u_dma_buf_sync_for_dev(struct u_dma_buf* this)
{
    if (this->sync_command_len > 0)
        return write(this->sync_for_dev_file,
                     this->sync_command,
                     this->sync_command_len);
    else
        return 0;
}

struct test_time
{
    struct timeval main;
    struct timeval sync_for_cpu;
    struct timeval sync_for_dev;
    struct timeval total;
};

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

int u_dma_buf_mmap_write_test(struct u_dma_buf* this, void* buf, unsigned int size, int sync, struct test_time* time)
{
    int            fd;
    void*          iomem;
    struct timeval test_start_time, test_end_time;
    struct timeval main_start_time, main_end_time;

    if (sync == 0)
        u_dma_buf_set_sync_area(this, 0, size, U_DMA_BUF_WRITE_ONLY);
      
    if ((fd  = u_dma_buf_open(this, O_RDWR | ((sync)?O_SYNC:0))) != -1) {
        iomem = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        gettimeofday(&test_start_time, NULL);
        if (sync == 0)
            u_dma_buf_sync_for_cpu(this);
        gettimeofday(&main_start_time, NULL);
        memcpy(iomem, buf, size);
        gettimeofday(&main_end_time, NULL);
        if (sync == 0)
            u_dma_buf_sync_for_dev(this);
        gettimeofday(&test_end_time  , NULL);
        if (time != NULL) {
            diff_time(&time->total       , &test_start_time, &test_end_time  );
            diff_time(&time->sync_for_cpu, &test_start_time, &main_start_time);
            diff_time(&time->main        , &main_start_time, &main_end_time  );
            diff_time(&time->sync_for_dev, &main_end_time  , &test_end_time  );
        }
        (void)close(fd);
        return 0;
    } else {
        return -1;
    }
}

int u_dma_buf_mmap_read_test(struct u_dma_buf* this, void* buf, unsigned int size, int sync, struct test_time* time)
{
    int            fd;
    void*          iomem;
    struct timeval test_start_time, test_end_time;
    struct timeval main_start_time, main_end_time;

    if (sync == 0)
        u_dma_buf_set_sync_area(this, 0, size, U_DMA_BUF_READ_ONLY);
      
    if ((fd  = u_dma_buf_open(this, O_RDWR | ((sync)?O_SYNC:0))) != -1) {
        iomem = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        gettimeofday(&test_start_time, NULL);
        if (sync == 0)
            u_dma_buf_sync_for_cpu(this);
        gettimeofday(&main_start_time, NULL);
        memcpy(buf, iomem, size);
        gettimeofday(&main_end_time  , NULL);
        if (sync == 0)
            u_dma_buf_sync_for_dev(this);
        gettimeofday(&test_end_time  , NULL);
        if (time != NULL) {
            diff_time(&time->total       , &test_start_time, &test_end_time  );
            diff_time(&time->sync_for_cpu, &test_start_time, &main_start_time);
            diff_time(&time->main        , &main_start_time, &main_end_time  );
            diff_time(&time->sync_for_dev, &main_end_time  , &test_end_time  );
        }
        close(fd);
        return 0;
    } else {
        return -1;
    }
}

int u_dma_buf_file_write_test(struct u_dma_buf* this, void* buf, unsigned int size, int sync, struct test_time* time)
{
    int            fd;
    int            len;
    void*          ptr;
    struct timeval test_start_time, test_end_time;
    struct timeval main_start_time, main_end_time;

    if (sync == 0)
        u_dma_buf_set_sync_area(this, 0, size, U_DMA_BUF_WRITE_ONLY);
      
    if ((fd  = u_dma_buf_open(this, O_RDWR | ((sync)?O_SYNC:0))) != -1) {
        gettimeofday(&test_start_time, NULL);
        if (sync == 0)
            u_dma_buf_sync_for_cpu(this);
        gettimeofday(&main_start_time, NULL);
        len = size;
        ptr = buf;
        while(len > 0) {
            int count = write(fd, ptr, len);
            if (count < 0) {
                break;
            }
            ptr += count;
            len -= count;
        }
        gettimeofday(&main_end_time, NULL);
        if (sync == 0)
            u_dma_buf_sync_for_dev(this);
        gettimeofday(&test_end_time, NULL);
        if (time != NULL) {
            diff_time(&time->total       , &test_start_time, &test_end_time  );
            diff_time(&time->sync_for_cpu, &test_start_time, &main_start_time);
            diff_time(&time->main        , &main_start_time, &main_end_time  );
            diff_time(&time->sync_for_dev, &main_end_time  , &test_end_time  );
        }
        (void)close(fd);
        return 0;
    } else {
        return -1;
    }
}

int u_dma_buf_file_read_test(struct u_dma_buf* this, void* buf, unsigned int size, int sync, struct test_time* time)
{
    int            fd;
    int            len;
    void*          ptr;
    struct timeval test_start_time, test_end_time;
    struct timeval main_start_time, main_end_time;

    if (sync == 0)
        u_dma_buf_set_sync_area(this, 0, size, U_DMA_BUF_READ_ONLY);
      
    if ((fd  = u_dma_buf_open(this, O_RDWR | ((sync)?O_SYNC:0))) != -1) {
        gettimeofday(&test_start_time, NULL);
        if (sync == 0)
            u_dma_buf_sync_for_cpu(this);
        gettimeofday(&main_start_time, NULL);
        len = size;
        ptr = buf;
        while(len > 0) {
            int count = read(fd, ptr, len);
            if (count < 0) {
                break;
            }
            ptr += count;
            len -= count;
        }
        gettimeofday(&main_end_time  , NULL);
        if (sync == 0)
            u_dma_buf_sync_for_dev(this);
        gettimeofday(&test_end_time  , NULL);
        if (time != NULL) {
            diff_time(&time->total       , &test_start_time, &test_end_time  );
            diff_time(&time->sync_for_cpu, &test_start_time, &main_start_time);
            diff_time(&time->main        , &main_start_time, &main_end_time  );
            diff_time(&time->sync_for_dev, &main_end_time  , &test_end_time  );
        }
        close(fd);
        return 0;
    } else {
        return -1;
    }
}

int main(int argc, char* argv[])
{
    struct u_dma_buf* u_dma_buf;
    char              device_name[256];
    unsigned int      err_count = 0;
    size_t            buf_size;
    void*             null_buf = NULL;
    void*             src0_buf = NULL;
    void*             src1_buf = NULL;
    void*             temp_buf = NULL;
    int               verbose  = 0;
    int               opt;
    int               optindex;
    struct option     longopts[] = {
      { "name"      , required_argument, NULL, 'n'},
      { "verbose"   , no_argument      , NULL, 'v'},
      { NULL        , 0                , NULL,  0 },
    };

    strncpy(device_name, "udmabuf0", sizeof(device_name));
    while ((opt = getopt_long(argc, argv, "n:", longopts, &optindex)) != -1) {
      switch (opt) {
        case 'n':
          strncpy(device_name, optarg, sizeof(device_name));
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
    //
    // u_dma_buf
    //
    if ((u_dma_buf = u_dma_buf_create(device_name)) == NULL) {
        goto done;
    }
    printf("driver_version=%s\n", u_dma_buf->version     );
    printf("size=%ld\n"         , u_dma_buf->size        );
    printf("dma_coherent=%d\n"  , u_dma_buf->dma_coherent);
    //
    // initilize buffers 
    //
    buf_size = u_dma_buf->size;
    if ((null_buf = malloc(buf_size)) == NULL) {
        printf("Can not malloc null_buf\n");
        goto done;
    } else {
        memset(null_buf, 0, buf_size);
    }
    if ((src0_buf = malloc(buf_size)) == NULL) {
        printf("Can not malloc src0_buf\n");
        goto done;
    } else {
        int*   word  = (int *)src0_buf;
        size_t words = buf_size/sizeof(int);
        for(int i = 0; i < words; i++) {
            word[i] = i;
        }
    }
    if ((src1_buf = malloc(buf_size)) == NULL) {
        printf("Can not malloc src1_buf\n");
        goto done;
    } else {
        int*   word  = (int *)src1_buf;
        size_t words = buf_size/sizeof(int);
        for(int i = 0; i < words; i++) {
            word[i] = ~i;
        }
    }
    if ((temp_buf = malloc(buf_size)) == NULL) {
        printf("Can not malloc temp_buf\n");
        goto done;
    } else {
        memset(temp_buf, 0, buf_size);
    }
    //
    // define TEST1()
    //
#define TEST1(w_type,w_sync,r_type,r_sync,src,dst,size)    \
    {                                                      \
        struct test_time w_time;                           \
        struct test_time r_time;                           \
        memset(dst, 0, buf_size);                          \
        printf(#w_type " write test : sync=%d ", w_sync);  \
        u_dma_buf_##w_type##_write_test(u_dma_buf, src, size, w_sync, &w_time); \
        printf("time=%ld.%06ld sec (%ld.%06ld sec)\n", w_time.total.tv_sec, w_time.total.tv_usec, w_time.main.tv_sec, w_time.main.tv_usec); \
        printf(#r_type " read  test : sync=%d ", r_sync);  \
        u_dma_buf_##r_type##_read_test (u_dma_buf, dst, size, r_sync, &r_time); \
        printf("time=%ld.%06ld sec (%ld.%06ld sec)\n", r_time.total.tv_sec, r_time.total.tv_usec, r_time.main.tv_sec, r_time.main.tv_usec); \
        if (memcmp(dst, src, size) != 0) {   \
            printf("compare = mismatch\n");  \
            err_count++;                     \
        } else {                             \
            printf("compare = ok\n");        \
        }                                    \
    }
    TEST1(mmap, 1, mmap, 1, src0_buf, temp_buf, buf_size);
    TEST1(mmap, 0, mmap, 1, src1_buf, temp_buf, buf_size);
    TEST1(mmap, 1, mmap, 0, src0_buf, temp_buf, buf_size);
    TEST1(mmap, 0, mmap, 0, src1_buf, temp_buf, buf_size);
    TEST1(file, 1, mmap, 0, src0_buf, temp_buf, buf_size);
    TEST1(file, 0, mmap, 0, src1_buf, temp_buf, buf_size);
    TEST1(mmap, 0, file, 1, src0_buf, temp_buf, buf_size);
    TEST1(mmap, 0, file, 0, src1_buf, temp_buf, buf_size);

 done:
    if (temp_buf  != NULL)
        free(temp_buf);
    if (src1_buf  != NULL)
        free(src1_buf);
    if (src0_buf  != NULL)
        free(src0_buf);
    if (null_buf  != NULL)
        free(null_buf);
    if (u_dma_buf != NULL)
      u_dma_buf_destroy(u_dma_buf);
}
