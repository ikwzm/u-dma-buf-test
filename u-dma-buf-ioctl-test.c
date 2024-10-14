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
#include        <linux/dma-buf.h>
#include        "u-dma-buf-ioctl.h"

struct proc_time_info {
	struct timespec start;
	struct timespec done;
	long long       elapsed;
};
static  long long proc_time_calc_elapsed(struct proc_time_info* t)
{
	long time_sec  = t->done.tv_sec  - t->start.tv_sec; 
	long time_nsec = t->done.tv_nsec - t->start.tv_nsec;
	t->elapsed = 1000*1000*1000*time_sec + time_nsec;
}

int xioctl(int fd, int ioctl_code, void* parameter)
{
    int result;
    do {
        result = ioctl(fd, ioctl_code, parameter);
    } while ((result == -1) && (errno == EINTR));
    return result;
}

int  check_driver_version(const char* device_name, const char* driver_version)
{
    int   fd;
    char  attr[1024];
    char  file_name[1024];

    sprintf(file_name, "/sys/class/u-dma-buf/%s/driver_version", device_name);
    
    if ((fd  = open(file_name, O_RDONLY)) < 0) {
        perror(file_name);
        exit(-1);
    } else {
        int len;
        len = read(fd, attr, sizeof(attr));
        while(--len >= 0) {
            if (attr[len] =='\n') {
                attr[len] = '\0';
                break;
            }
        }
        close(fd);
    }
    if (strcmp(attr, driver_version) != 0) {
        printf("read %s => %s\n", file_name, attr);
        printf("mismatch driver_version(=%s)\n", driver_version);
        exit(-1);
    }
    return 0;
}
int  check_device_info(const char* device_name, u_dma_buf_ioctl_dev_info* dev_info)
{
    int      fd;
    char     attr[1024];
    char     file_name[1024];
    size_t   size;
    uint64_t phys_addr;
    int      dma_mask;
    
    sprintf(file_name, "/sys/class/u-dma-buf/%s/size", device_name);
    if ((fd  = open(file_name, O_RDONLY)) < 0) {
        perror(file_name);
        exit(-1);
    }
    read(fd, attr, sizeof(attr));
    sscanf(attr, "%" SCNu64, &size);
    close(fd);

    if (size != dev_info->size) {
        printf("read %s => %s\n", file_name, attr);
        printf("mismatch size(=%" PRIu64 ")\n", dev_info->size);
        exit(-1);
    }
    
    sprintf(file_name, "/sys/class/u-dma-buf/%s/phys_addr", device_name);
    if ((fd  = open(file_name, O_RDONLY)) < 0) {
        perror(file_name);
        exit(-1);
    }
    read(fd, attr, sizeof(attr));
    sscanf(attr, "%" SCNx64, &phys_addr);
    close(fd);

    if (phys_addr != dev_info->addr) {
        printf("read %s => %s\n", file_name, attr);
        printf("mismatch phys_addr (=0x%" PRIx64 ")\n", dev_info->addr);
        exit(-1);
    }
}

int read_sync_variables(char* device_name, uint64_t* sync_offset, uint64_t* sync_size, int* sync_direction, int* sync_owner, int* sync_for_cpu, int* sync_for_device, int* sync_mode)
{
    int      fd;
    char     attr[1024];
    char     file_name[1024];

    sprintf(file_name, "/sys/class/u-dma-buf/%s/sync_offset", device_name);
    if ((fd  = open(file_name, O_RDONLY)) < 0) {
        perror(file_name);
        exit(-1);
    }
    read(fd, attr, sizeof(attr));
    sscanf(attr, "%" SCNx64, sync_offset);
    close(fd);

    sprintf(file_name, "/sys/class/u-dma-buf/%s/sync_size", device_name);
    if ((fd  = open(file_name, O_RDONLY)) < 0) {
        perror(file_name);
        exit(-1);
    }
    read(fd, attr, sizeof(attr));
    sscanf(attr, "%" SCNu64, sync_size);
    close(fd);

    sprintf(file_name, "/sys/class/u-dma-buf/%s/sync_direction", device_name);
    if ((fd  = open(file_name, O_RDONLY)) < 0) {
        perror(file_name);
        exit(-1);
    }
    read(fd, attr, sizeof(attr));
    sscanf(attr, "%d", sync_direction);
    close(fd);

    sprintf(file_name, "/sys/class/u-dma-buf/%s/sync_owner", device_name);
    if ((fd  = open(file_name, O_RDONLY)) < 0) {
        perror(file_name);
        exit(-1);
    }
    read(fd, attr, sizeof(attr));
    sscanf(attr, "%d", sync_owner);
    close(fd);

    sprintf(file_name, "/sys/class/u-dma-buf/%s/sync_for_cpu", device_name);
    if ((fd  = open(file_name, O_RDONLY)) < 0) {
        perror(file_name);
        exit(-1);
    }
    read(fd, attr, sizeof(attr));
    sscanf(attr, "%d", sync_for_cpu);
    close(fd);

    sprintf(file_name, "/sys/class/u-dma-buf/%s/sync_for_device", device_name);
    if ((fd  = open(file_name, O_RDONLY)) < 0) {
        perror(file_name);
        exit(-1);
    }
    read(fd, attr, sizeof(attr));
    sscanf(attr, "%d", sync_for_device);
    close(fd);

    sprintf(file_name, "/sys/class/u-dma-buf/%s/sync_mode", device_name);
    if ((fd  = open(file_name, O_RDONLY)) < 0) {
        perror(file_name);
        exit(-1);
    }
    read(fd, attr, sizeof(attr));
    sscanf(attr, "%d", sync_mode);
    close(fd);

    return 0;
}

int check_ioctl_set_sync_for_cpu(char* driver_name, int u_dma_buf_fd, int sync_offset, int sync_size, int sync_direction)
{
    int      status = 0;
    uint64_t sync_for_cpu = ((uint64_t)(sync_offset) <<32) | ((uint64_t)(sync_size) & 0xFFFFFFF0) | ((uint64_t)(sync_direction) << 2) | 1;
    
    status = xioctl(u_dma_buf_fd, U_DMA_BUF_IOCTL_SET_SYNC_FOR_CPU, &sync_for_cpu);
    if (status != 0) {
        perror("U_DMA_BUF_IOCTL_SET_SYNC_FOR_CPU");
        exit(-1);
    }
    uint64_t var_sync_offset;
    uint64_t var_sync_size;
    int      var_sync_direction;
    int      var_sync_owner;
    int      var_sync_for_cpu;
    int      var_sync_for_device;
    int      var_sync_mode;
    status = read_sync_variables(driver_name, &var_sync_offset, &var_sync_size, &var_sync_direction, &var_sync_owner, &var_sync_for_cpu, &var_sync_for_device, &var_sync_mode);
    if (var_sync_for_cpu != 0) {
	printf("%s: sync_for_cpu is not 0\n", __func__);
	status = -1;
    }
    if (var_sync_for_device != 0) {
	printf("%s: sync_for_device is not 0\n", __func__);
	status = -1;
    }
    if (var_sync_owner != 0) {
	printf("%s: sync_owner is not 0\n", __func__);
	status = -1;
    }
    return status;
}

int check_ioctl_set_sync_for_device(char* driver_name, int u_dma_buf_fd, int sync_offset, int sync_size, int sync_direction)
{
    int      status = 0;
    uint64_t sync_for_cpu = ((uint64_t)(sync_offset) <<32) | ((uint64_t)(sync_size) & 0xFFFFFFF0) | ((uint64_t)(sync_direction) << 2) | 1;
    
    status = xioctl(u_dma_buf_fd, U_DMA_BUF_IOCTL_SET_SYNC_FOR_DEVICE, &sync_for_cpu);
    if (status != 0) {
        perror("U_DMA_BUF_IOCTL_SET_SYNC_FOR_DEVICE");
        exit(-1);
    }
    uint64_t var_sync_offset;
    uint64_t var_sync_size;
    int      var_sync_direction;
    int      var_sync_owner;
    int      var_sync_for_cpu;
    int      var_sync_for_device;
    int      var_sync_mode;
    status = read_sync_variables(driver_name, &var_sync_offset, &var_sync_size, &var_sync_direction, &var_sync_owner, &var_sync_for_cpu, &var_sync_for_device, &var_sync_mode);
    if (var_sync_for_cpu != 0) {
	printf("%s: sync_for_cpu is not 0\n", __func__);
	status = -1;
    }
    if (var_sync_for_device != 0) {
	printf("%s: sync_for_device is not 0\n", __func__);
	status = -1;
    }
    if (var_sync_owner != 1) {
	printf("%s: sync_owner is not 1\n", __func__);
	status = -1;
    }
    return status;
}

int check_ioctl_set_sync(char* driver_name, int u_dma_buf_fd, u_dma_buf_ioctl_sync_args* sync_args)
{
    int      status         = 0;
    uint64_t sync_offset    = sync_args->offset;
    uint64_t sync_size      = sync_args->size;;
    int      sync_direction = GET_U_DMA_BUF_IOCTL_FLAGS_SYNC_DIR(sync_args);
    int      sync_command   = GET_U_DMA_BUF_IOCTL_FLAGS_SYNC_CMD(sync_args);
	
    if (1) {
        printf("U_DMA_BUF_IOCTL_SET_SYNC: SET\n");
	printf("    sync_offset    : %" PRIu64 "\n", sync_offset   );
	printf("    sync_size      : %" PRIu64 "\n", sync_size     );
	printf("    sync_direction : %d\n"         , sync_direction);
	printf("    sync_command   : %d\n"         , sync_command  );
    }
    status = xioctl(u_dma_buf_fd, U_DMA_BUF_IOCTL_SET_SYNC, sync_args);
    if (status != 0) {
        perror("U_DMA_BUF_IOCTL_SET_SYNC");
        exit(-1);
    }
    uint64_t var_sync_offset;
    uint64_t var_sync_size;
    int      var_sync_direction;
    int      var_sync_owner;
    int      var_sync_for_cpu;
    int      var_sync_for_device;
    int      var_sync_mode;
    status = read_sync_variables(driver_name, &var_sync_offset, &var_sync_size, &var_sync_direction, &var_sync_owner, &var_sync_for_cpu, &var_sync_for_device, &var_sync_mode);
    if (1) {
        printf("U_DMA_BUF_IOCTL_SET_SYNC: READ VARIABLES\n");
	printf("    sync_offset    : %" PRIu64 "\n", var_sync_offset   );
	printf("    sync_size      : %" PRIu64 "\n", var_sync_size     );
	printf("    sync_direction : %d\n"         , var_sync_direction);
	printf("    sync_owner     : %d\n"         , var_sync_owner    );
	printf("    sync_mode      : %d\n"         , var_sync_mode     );
    }
    if (var_sync_for_cpu != 0) {
	printf("%s: sync_for_cpu is not 0\n", __func__);
	status = -1;
    }
    if (var_sync_for_device != 0) {
	printf("%s: sync_for_device is not 0\n", __func__);
	status = -1;
    }
    if (sync_offset != 0) {
	if (var_sync_offset != sync_offset) {
            printf("%s: sync_offset mismatch (set=%" PRIu64 ", var=%" PRIu64 ")\n", __func__, sync_offset, var_sync_offset);
	    status = -1;
        }
    }
    if (sync_size != 0) {
	if (var_sync_size != sync_size) {
            printf("%s: sync_size mismatch (set=%" PRIu64 ", var=%" PRIu64 ")\n", __func__, sync_size, var_sync_size);
	    status = -1;
        }
    }
    if (var_sync_direction != sync_direction) {
        printf("%s: sync_direction mismatch (set=%d, var=%d)\n", __func__, sync_direction, var_sync_direction);
	status = -1;
    }
    if (sync_command == U_DMA_BUF_IOCTL_FLAGS_SYNC_CMD_FOR_DEVICE) {
        if (var_sync_owner != 1) {
	    printf("%s: sync_owner is not 1\n", __func__);
	    status = -1;
        }
    }
    if (sync_command == U_DMA_BUF_IOCTL_FLAGS_SYNC_CMD_FOR_CPU) {
        if (var_sync_owner != 0) {
	    printf("%s: sync_owner is not 0\n", __func__);
	    status = -1;
        }
    }
    return status;
}

int check_ioctl_get_sync(char* driver_name, int u_dma_buf_fd)
{
    int      status         = 0;
    u_dma_buf_ioctl_sync_args sync_args;
	
    status = xioctl(u_dma_buf_fd, U_DMA_BUF_IOCTL_GET_SYNC, &sync_args);
    if (status != 0) {
        perror("U_DMA_BUF_IOCTL_GET_SYNC");
        exit(-1);
    }
    uint64_t sync_offset    = sync_args.offset;
    uint64_t sync_size      = sync_args.size;;
    int      sync_direction = GET_U_DMA_BUF_IOCTL_FLAGS_SYNC_DIR  (&sync_args);
    int      sync_command   = GET_U_DMA_BUF_IOCTL_FLAGS_SYNC_CMD  (&sync_args);
    int      sync_mode      = GET_U_DMA_BUF_IOCTL_FLAGS_SYNC_MODE (&sync_args);
    int      sync_owner     = GET_U_DMA_BUF_IOCTL_FLAGS_SYNC_OWNER(&sync_args);
    if (0) {
	printf("%s: get sync_args\n", __func__);
	printf("    sync_offset    = %ld\n", sync_offset   );
	printf("    sync_size      = %ld\n", sync_size     );
	printf("    sync_direction = %d\n" , sync_direction);
	printf("    sync_owner     = %d\n" , sync_owner    );
	printf("    sync_mode      = %d\n" , sync_mode     );
    }
    uint64_t var_sync_offset;
    uint64_t var_sync_size;
    int      var_sync_direction;
    int      var_sync_owner;
    int      var_sync_for_cpu;
    int      var_sync_for_device;
    int      var_sync_mode;
    status = read_sync_variables(driver_name, &var_sync_offset, &var_sync_size, &var_sync_direction, &var_sync_owner, &var_sync_for_cpu, &var_sync_for_device, &var_sync_mode);
    if (0) {
	printf("%s: read variable\n", __func__);
	printf("    sync_offset    = %ld\n", var_sync_offset   );
	printf("    sync_size      = %ld\n", var_sync_size     );
	printf("    sync_direction = %d\n" , var_sync_direction);
	printf("    sync_owner     = %d\n" , var_sync_owner    );
	printf("    sync_mode      = %d\n" , var_sync_mode     );
    }
    if (var_sync_for_cpu != 0) {
	printf("%s: sync_for_cpu is not 0\n", __func__);
	status = -1;
    }
    if (var_sync_for_device != 0) {
	printf("%s: sync_for_device is not 0\n", __func__);
	status = -1;
    }
    if (var_sync_offset != sync_offset) {
        printf("%s: sync_offset mismatch (get=%ld, var=%ld)\n", __func__, sync_offset, var_sync_offset);
	status = -1;
    }
    if (var_sync_size != sync_size) {
        printf("%s: sync_size mismatch (get=%ld, var=%ld)\n", __func__, sync_size, var_sync_size);
	status = -1;
    }
    if (var_sync_direction != sync_direction) {
        printf("%s: sync_direction mismatch (get=%d, var=%d)\n", __func__, sync_direction, var_sync_direction);
	status = -1;
    }
    if (var_sync_owner != sync_owner) {
	printf("%s: sync_owner mismatch (get=%d, var=%d)\n", __func__, sync_owner, var_sync_owner);
	status = -1;
    }
    return status;
}

int check_ioctl_export(char* driver_name, int u_dma_buf_fd, size_t u_dma_buf_size, int ioctl_version)
{
    int                         status          = 0;
    int                         fd_flags        = 0;
    u_dma_buf_ioctl_export_args export_args     = {0};
    void*                       u_dma_buf_start = NULL;
    struct dma_buf_sync         dma_buf_sync    = {0};
    struct proc_time_info       run_time_info;
    double                      run_time        = 0.0;

    //
    // check ioctl version
    //
    if (ioctl_version < 2) {
        export_args.size   = u_dma_buf_size;
        export_args.offset = 0;
        SET_U_DMA_BUF_IOCTL_FLAGS_EXPORT_FD_FLAGS(&export_args, (O_CLOEXEC | O_RDWR));
        status = xioctl(u_dma_buf_fd, U_DMA_BUF_IOCTL_EXPORT, &export_args);
        if (status == -1) {
            perror("U_DMA_BUF_IOCTL_EXPORT failed because ioctl_version<2");
            return 0;
        } else {
            printf("U_DMA_BUF_IOCTL_EXPORT successed even though ioctl_version < 2\n");
            exit(-1);
        }
    }
    //
    // check size over
    //
    export_args.size   = u_dma_buf_size*2;
    export_args.offset = 0;
    fd_flags           = O_CLOEXEC | O_RDWR;
    SET_U_DMA_BUF_IOCTL_FLAGS_EXPORT_FD_FLAGS(&export_args, fd_flags);
    status = xioctl(u_dma_buf_fd, U_DMA_BUF_IOCTL_EXPORT, &export_args);
    if (status == -1) {
     // perror("U_DMA_BUF_IOCTL_EXPORT failed because size over");
    } else {
        printf("U_DMA_BUF_IOCTL_EXPORT successed even though size over\n");
        exit(-1);
    }
    //
    // check offset over
    //
    export_args.offset = u_dma_buf_size;
    export_args.size   = u_dma_buf_size/2;
    fd_flags           = O_CLOEXEC | O_RDWR;
    SET_U_DMA_BUF_IOCTL_FLAGS_EXPORT_FD_FLAGS(&export_args, fd_flags);
    status = xioctl(u_dma_buf_fd, U_DMA_BUF_IOCTL_EXPORT, &export_args);
    if (status == -1) {
     // perror("U_DMA_BUF_IOCTL_EXPORT failed because offset over");
    } else {
        printf("U_DMA_BUF_IOCTL_EXPORT successed even though offset over\n");
        exit(-1);
    }
    //
    // check size limit over
    //
    export_args.offset = u_dma_buf_size/2;
    export_args.size   = u_dma_buf_size;
    fd_flags           = O_CLOEXEC | O_RDWR;
    SET_U_DMA_BUF_IOCTL_FLAGS_EXPORT_FD_FLAGS(&export_args, fd_flags);
    status = xioctl(u_dma_buf_fd, U_DMA_BUF_IOCTL_EXPORT, &export_args);
    if (status == -1) {
     // perror("U_DMA_BUF_IOCTL_EXPORT failed because offset+size over");
    } else {
        printf("U_DMA_BUF_IOCTL_EXPORT successed even though offset+size over\n");
        exit(-1);
    }
    //
    // check invalid fd_flags
    //
    export_args.offset = 0;
    export_args.size   = u_dma_buf_size;
    fd_flags           = O_CLOEXEC | O_CREAT;
    SET_U_DMA_BUF_IOCTL_FLAGS_EXPORT_FD_FLAGS(&export_args, fd_flags);
    status = xioctl(u_dma_buf_fd, U_DMA_BUF_IOCTL_EXPORT, &export_args);
    if (status == -1) {
     // perror("U_DMA_BUF_IOCTL_EXPORT failed because invalid fd_flags");
    } else {
        printf("U_DMA_BUF_IOCTL_EXPORT successed even though invalid fd_flags\n");
        exit(-1);
    }
    //
    // check valid fd_flags(=0)
    //
    export_args.offset = 0;
    export_args.size   = u_dma_buf_size;
    fd_flags           = 0;
    SET_U_DMA_BUF_IOCTL_FLAGS_EXPORT_FD_FLAGS(&export_args, fd_flags);
    status = xioctl(u_dma_buf_fd, U_DMA_BUF_IOCTL_EXPORT, &export_args);
    if (status == -1) {
        perror("U_DMA_BUF_IOCTL_EXPORT fd_flags=0");
        exit(-1);
    } else {
        close(export_args.fd);
    }
    //
    // check valid fd_flags(=O_RDWR)
    //
    export_args.offset = 0;
    export_args.size   = u_dma_buf_size;
    fd_flags           = O_RDWR;
    SET_U_DMA_BUF_IOCTL_FLAGS_EXPORT_FD_FLAGS(&export_args, fd_flags);
    status = xioctl(u_dma_buf_fd, U_DMA_BUF_IOCTL_EXPORT, &export_args);
    if (status == -1) {
        perror("U_DMA_BUF_IOCTL_EXPORT fd_flags=O_RDWR");
        exit(-1);
    } else {
        close(export_args.fd);
    }
    //
    // check valid fd_flags(=O_RDONLY)
    //
    export_args.offset = 0;
    export_args.size   = u_dma_buf_size;
    fd_flags           = O_RDONLY;
    SET_U_DMA_BUF_IOCTL_FLAGS_EXPORT_FD_FLAGS(&export_args, fd_flags);
    status = xioctl(u_dma_buf_fd, U_DMA_BUF_IOCTL_EXPORT, &export_args);
    if (status == -1) {
        perror("U_DMA_BUF_IOCTL_EXPORT fd_flags=O_RDONLY");
        exit(-1);
    } else {
        close(export_args.fd);
    }
    //
    // check valid fd_flags(=O_WRONLY)
    //
    export_args.offset = 0;
    export_args.size   = u_dma_buf_size;
    fd_flags           = O_WRONLY;
    SET_U_DMA_BUF_IOCTL_FLAGS_EXPORT_FD_FLAGS(&export_args, fd_flags);
    status = xioctl(u_dma_buf_fd, U_DMA_BUF_IOCTL_EXPORT, &export_args);
    if (status == -1) {
        perror("U_DMA_BUF_IOCTL_EXPORT fd_flags=O_WRONLY");
        exit(-1);
    } else {
        close(export_args.fd);
    }
    //
    // check valid fd_flags(=O_CLOEXEC|O_RDONLY)
    //
    export_args.offset = 0;
    export_args.size   = u_dma_buf_size;
    fd_flags           = O_CLOEXEC | O_RDONLY;
    SET_U_DMA_BUF_IOCTL_FLAGS_EXPORT_FD_FLAGS(&export_args, fd_flags);
    status = xioctl(u_dma_buf_fd, U_DMA_BUF_IOCTL_EXPORT, &export_args);
    if (status == -1) {
        perror("U_DMA_BUF_IOCTL_EXPORT fd_flags=O_CLOEXEC|O_RDONLY");
        exit(-1);
    } else {
        close(export_args.fd);
    }
    //
    // check valid fd_flags(=O_CLOEXEC|O_WRONLY)
    //
    export_args.offset = 0;
    export_args.size   = u_dma_buf_size;
    fd_flags           = O_CLOEXEC|O_WRONLY;
    SET_U_DMA_BUF_IOCTL_FLAGS_EXPORT_FD_FLAGS(&export_args, fd_flags);
    status = xioctl(u_dma_buf_fd, U_DMA_BUF_IOCTL_EXPORT, &export_args);
    if (status == -1) {
        perror("U_DMA_BUF_IOCTL_EXPORT fd_flags=O_CLOEXEC|O_WRONLY");
        exit(-1);
    } else {
        close(export_args.fd);
    }
    //
    // check mmap & sync
    //
    printf("U_DMA_BUF mmap(): \n");
    u_dma_buf_start = mmap(NULL, u_dma_buf_size,
                           PROT_READ | PROT_WRITE,
                           MAP_SHARED,
                           u_dma_buf_fd,
                           0);
    if (u_dma_buf_start == MAP_FAILED) {
        perror("mmap");
        exit(-1);
    }
    clock_gettime(CLOCK_MONOTONIC, &(run_time_info.start));
    {
        uint64_t* word_ptr = u_dma_buf_start;
        size_t    words    = u_dma_buf_size/sizeof(uint64_t);
        uint64_t  sum      = 0;
        for(int i = 0; i < words; i++){
            word_ptr[i] = 0;
        }
        for(int i = 0; i < words; i++){
            sum += word_ptr[i];
        }
        if (sum != 0) {
            perror("checksum");
            exit(-1);
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &(run_time_info.done));
    munmap(u_dma_buf_start, u_dma_buf_size);
    proc_time_calc_elapsed(&run_time_info);
    run_time  = (double)(run_time_info.elapsed)/(1000.0*1000.0*1000.0);
    printf("    Run Time       : %.9f #[Second]\n"  , run_time);
    //
    // check export mmap & sync(O_SYNC=0)
    //
    printf("U_DMA_BUF_IOCTL_EXPORT: \n");
    printf("    fd_flags       : O_CLOEXEC | O_RDWR \n");
    export_args.offset = 0;
    export_args.size   = u_dma_buf_size;
    fd_flags           = O_CLOEXEC | O_RDWR;
    SET_U_DMA_BUF_IOCTL_FLAGS_EXPORT_FD_FLAGS(&export_args, fd_flags);
    status = xioctl(u_dma_buf_fd, U_DMA_BUF_IOCTL_EXPORT, &export_args);
    if (status == -1) {
        perror("U_DMA_BUF_IOCTL_EXPORT");
        exit(-1);
    }
    printf("    fd             : %d\n", export_args.fd);
    u_dma_buf_start = mmap(NULL, u_dma_buf_size,
                           PROT_READ | PROT_WRITE,
                           MAP_SHARED,
                           export_args.fd,
                           0);
    if (u_dma_buf_start == MAP_FAILED) {
        perror("mmap");
        exit(-1);
    }
    clock_gettime(CLOCK_MONOTONIC, &(run_time_info.start));
    dma_buf_sync.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_RW;
    status = ioctl(export_args.fd, DMA_BUF_IOCTL_SYNC, &dma_buf_sync);
    if (status == -1) {
        perror("DMA_BUF_IOCTL_SYNC DMA_BUF_SYNC_START | DMA_BUF_SYNC_RW");
        exit(-1);
    }
    {
        uint64_t* word_ptr = u_dma_buf_start;
        size_t    words    = u_dma_buf_size/sizeof(uint64_t);
        uint64_t  sum      = 0;
        for(int i = 0; i < words; i++){
            word_ptr[i] = 0;
        }
        for(int i = 0; i < words; i++){
            sum += word_ptr[i];
        }
        if (sum != 0) {
            perror("checksum");
            exit(-1);
        }
    }
    dma_buf_sync.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_RW;
    status = ioctl(export_args.fd, DMA_BUF_IOCTL_SYNC, &dma_buf_sync);
    if (status == -1) {
        perror("DMA_BUF_IOCTL_SYNC DMA_BUF_SYNC_END | DMA_BUF_SYNC_RW");
        exit(-1);
    }
    clock_gettime(CLOCK_MONOTONIC, &(run_time_info.done));
    close(export_args.fd);
    proc_time_calc_elapsed(&run_time_info);
    run_time  = (double)(run_time_info.elapsed)/(1000.0*1000.0*1000.0);
    printf("    Run Time       : %.9f #[Second]\n"  , run_time);
    //
    // check export mmap & sync(O_SYNC=1)
    //
    printf("U_DMA_BUF_IOCTL_EXPORT: \n");
    printf("    fd_flags       : O_CLOEXEC | O_RDWR | O_SYNC\n");
    export_args.offset = 0;
    export_args.size   = u_dma_buf_size;
    fd_flags           = O_CLOEXEC | O_RDWR | O_SYNC;
    SET_U_DMA_BUF_IOCTL_FLAGS_EXPORT_FD_FLAGS(&export_args, fd_flags);
    status = xioctl(u_dma_buf_fd, U_DMA_BUF_IOCTL_EXPORT, &export_args);
    if (status == -1) {
        perror("U_DMA_BUF_IOCTL_EXPORT");
        exit(-1);
    }
    printf("    fd             : %d\n", export_args.fd);
    u_dma_buf_start = mmap(NULL, u_dma_buf_size,
                           PROT_READ | PROT_WRITE,
                           MAP_SHARED,
                           export_args.fd,
                           0);
    if (u_dma_buf_start == MAP_FAILED) {
        perror("mmap");
        exit(-1);
    }
    clock_gettime(CLOCK_MONOTONIC, &(run_time_info.start));
    dma_buf_sync.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_RW;
    status = ioctl(export_args.fd, DMA_BUF_IOCTL_SYNC, &dma_buf_sync);
    if (status == -1) {
        perror("DMA_BUF_IOCTL_SYNC DMA_BUF_SYNC_START | DMA_BUF_SYNC_RW");
        exit(-1);
    }
    {
        uint64_t* word_ptr = u_dma_buf_start;
        size_t    words    = u_dma_buf_size/sizeof(uint64_t);
        uint64_t  sum      = 0;
        for(int i = 0; i < words; i++){
            word_ptr[i] = 0;
        }
        for(int i = 0; i < words; i++){
            sum += word_ptr[i];
        }
        if (sum != 0) {
            perror("checksum");
            exit(-1);
        }
    }
    dma_buf_sync.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_RW;
    status = ioctl(export_args.fd, DMA_BUF_IOCTL_SYNC, &dma_buf_sync);
    if (status == -1) {
        perror("DMA_BUF_IOCTL_SYNC DMA_BUF_SYNC_END | DMA_BUF_SYNC_RW");
        exit(-1);
    }
    clock_gettime(CLOCK_MONOTONIC, &(run_time_info.done));
    proc_time_calc_elapsed(&run_time_info);
    run_time  = (double)(run_time_info.elapsed)/(1000.0*1000.0*1000.0);
    printf("    Run Time       : %.9f #[Second]\n"  , run_time);
    close(export_args.fd);
}

int main(int argc, char* argv[])
{
    int                         status;
    int                         u_dma_buf_fd;
    u_dma_buf_ioctl_drv_info    u_dma_buf_drv_info;
    u_dma_buf_ioctl_dev_info    u_dma_buf_dev_info;
    u_dma_buf_ioctl_sync_args   u_dma_buf_sync_args;
    uint64_t                    u_dma_buf_size;
    uint64_t                    u_dma_buf_phys_addr;
    char                        device_name[256];
    char                        device_file_name[1024];
    char*                       driver_version;
    int                         ioctl_version;
    uint64_t                    sync_offset;
    uint64_t                    sync_size;
    int                         sync_direction;
    int                         verbose         = 0;
    int                         opt;
    int                         optindex;
    struct option               longopts[] = {
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

    //
    // u_dma_buf_fd
    //
    sprintf(device_file_name, "/dev/%s", device_name);
    if ((u_dma_buf_fd = open(device_file_name, O_RDWR)) < 0) {
        perror(device_file_name);
        exit(-1);
    }
    printf("DEVICE_NAME: %s\n", device_name);
    //
    // U_DMA_BUF_IOCTL_GET_DRV_INFO
    //
    status = xioctl(u_dma_buf_fd, U_DMA_BUF_IOCTL_GET_DRV_INFO, &u_dma_buf_drv_info);
    if (status != 0) {
        perror("U_DMA_BUF_IOCTL_GET_DRV_INFO");
        exit(-1);
    } else {
        driver_version = &u_dma_buf_drv_info.version[0];
        ioctl_version  = GET_U_DMA_BUF_IOCTL_FLAGS_IOCTL_VERSION(&u_dma_buf_drv_info);
        printf("U_DMA_BUF_IOCTL_GET_DRV_INFO: \n");
        printf("    driver_version : %s\n", driver_version);
        printf("    ioctl_version  : %d\n", ioctl_version );
    }
    //
    // U_DMA_BUF_IOCTL_GET_DEV_INFO
    //
    status = xioctl(u_dma_buf_fd, U_DMA_BUF_IOCTL_GET_DEV_INFO, &u_dma_buf_dev_info);
    if (status != 0) {
        perror("U_DMA_BUF_IOCTL_GET_DEV_INFO");
        exit(-1);
    } else {
        printf("U_DMA_BUF_IOCTL_GET_DEV_INFO: \n");
        printf("    size           : %"   PRIu64 "\n" , u_dma_buf_dev_info.size);
        printf("    phys_addr      : 0x%" PRIx64 "\n" , u_dma_buf_dev_info.addr);
        printf("    dma_mask       : %d\n"    , GET_U_DMA_BUF_IOCTL_FLAGS_DMA_MASK(&u_dma_buf_dev_info));
    }
    //
    // U_DMA_BUF_IOCTL_GET_SIZE
    //
    status = xioctl(u_dma_buf_fd, U_DMA_BUF_IOCTL_GET_SIZE, &u_dma_buf_size);
    if (status != 0) {
        perror("U_DMA_BUF_IOCTL_GET_SIZE");
        exit(-1);
    } else {
        printf("U_DMA_BUF_IOCTL_GET_SIZE: \n");
        printf("    size           : %" PRIu64 "\n"   , u_dma_buf_size);
    }
    if (u_dma_buf_size != u_dma_buf_dev_info.size) {
        printf("mismatch buffer size\n");
        exit(-1);
    }
    //
    // U_DMA_BUF_IOCTL_GET_DMA_ADDR
    //
    status = xioctl(u_dma_buf_fd, U_DMA_BUF_IOCTL_GET_DMA_ADDR, &u_dma_buf_phys_addr);
    if (status != 0) {
        perror("U_DMA_BUF_IOCTL_GET_DMA_ADDR");
        exit(-1);
    } else {
        printf("U_DMA_BUF_IOCTL_GET_DMA_ADDR: \n");
        printf("    phys_addr      : 0x%" PRIx64 "\n" , u_dma_buf_phys_addr);
    }
    if (u_dma_buf_phys_addr != u_dma_buf_dev_info.addr) {
        printf("mismatch phys_addr\n");
        exit(-1);
    }
    //
    // check_driver_version()
    //
    status = check_driver_version(device_name, &u_dma_buf_drv_info.version[0]);
    //
    // check_device_info()
    //
    status = check_device_info(device_name, &u_dma_buf_dev_info);
    //
    // check_ioctl_set_sync_for_device() -> check_ioctl_set_sync_for_cpu()
    //
    sync_offset    = 0;
    sync_size      = u_dma_buf_dev_info.size;
    sync_direction = 0;
    status = check_ioctl_set_sync_for_device(device_name, u_dma_buf_fd, sync_offset, sync_size, sync_direction);
    status = check_ioctl_set_sync_for_cpu(device_name, u_dma_buf_fd, sync_offset, sync_size, sync_direction);
    //
    // check_ioctl_set_sync_for_device() -> check_ioctl_set_sync_for_cpu() 
    //
    sync_offset    = u_dma_buf_dev_info.size/2;
    sync_size      = u_dma_buf_dev_info.size/4;
    sync_direction = 3;
    status = check_ioctl_set_sync_for_device(device_name, u_dma_buf_fd, sync_offset, sync_size, sync_direction);
    status = check_ioctl_set_sync_for_cpu(device_name, u_dma_buf_fd, sync_offset, sync_size, sync_direction);
    //
    // check_ioctl_set_sync() -> check_ioctl_get_sync() 
    //
    memset(&u_dma_buf_sync_args, 0, sizeof(u_dma_buf_sync_args));
    u_dma_buf_sync_args.offset = 0;
    u_dma_buf_sync_args.size   = u_dma_buf_dev_info.size/2;
    SET_U_DMA_BUF_IOCTL_FLAGS_SYNC_CMD(&u_dma_buf_sync_args, U_DMA_BUF_IOCTL_FLAGS_SYNC_CMD_FOR_DEVICE);
    SET_U_DMA_BUF_IOCTL_FLAGS_SYNC_DIR(&u_dma_buf_sync_args, 2);
    status = check_ioctl_set_sync(device_name, u_dma_buf_fd, &u_dma_buf_sync_args);
    status = check_ioctl_get_sync(device_name, u_dma_buf_fd);
    //
    // check_ioctl_set_sync() -> check_ioctl_get_sync() 
    //
    memset(&u_dma_buf_sync_args, 0, sizeof(u_dma_buf_sync_args));
    u_dma_buf_sync_args.offset = 0;
    u_dma_buf_sync_args.size   = u_dma_buf_dev_info.size/2;
    SET_U_DMA_BUF_IOCTL_FLAGS_SYNC_CMD(&u_dma_buf_sync_args, U_DMA_BUF_IOCTL_FLAGS_SYNC_CMD_FOR_CPU);
    SET_U_DMA_BUF_IOCTL_FLAGS_SYNC_DIR(&u_dma_buf_sync_args, 2);
    status = check_ioctl_set_sync(device_name, u_dma_buf_fd, &u_dma_buf_sync_args);
    status = check_ioctl_get_sync(device_name, u_dma_buf_fd);
    //
    // check_ioctl_set_sync() -> check_ioctl_get_sync() 
    //
    memset(&u_dma_buf_sync_args, 0, sizeof(u_dma_buf_sync_args));
    u_dma_buf_sync_args.offset = u_dma_buf_dev_info.size/4;
    u_dma_buf_sync_args.size   = u_dma_buf_dev_info.size/4;
    SET_U_DMA_BUF_IOCTL_FLAGS_SYNC_CMD(&u_dma_buf_sync_args, U_DMA_BUF_IOCTL_FLAGS_SYNC_CMD_FOR_DEVICE);
    SET_U_DMA_BUF_IOCTL_FLAGS_SYNC_DIR(&u_dma_buf_sync_args, 0);
    status = check_ioctl_set_sync(device_name, u_dma_buf_fd, &u_dma_buf_sync_args);
    status = check_ioctl_get_sync(device_name, u_dma_buf_fd);
    //
    // check_ioctl_set_sync() -> check_ioctl_get_sync() 
    //
    memset(&u_dma_buf_sync_args, 0, sizeof(u_dma_buf_sync_args));
    u_dma_buf_sync_args.offset = u_dma_buf_dev_info.size/4;
    u_dma_buf_sync_args.size   = u_dma_buf_dev_info.size/4;
    SET_U_DMA_BUF_IOCTL_FLAGS_SYNC_CMD(&u_dma_buf_sync_args, U_DMA_BUF_IOCTL_FLAGS_SYNC_CMD_FOR_CPU);
    SET_U_DMA_BUF_IOCTL_FLAGS_SYNC_DIR(&u_dma_buf_sync_args, 0);
    status = check_ioctl_set_sync(device_name, u_dma_buf_fd, &u_dma_buf_sync_args);
    status = check_ioctl_get_sync(device_name, u_dma_buf_fd);

    status = check_ioctl_export(device_name, u_dma_buf_fd, u_dma_buf_size, ioctl_version);
	    
    close(u_dma_buf_fd);
    return status;
}
