#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <malloc.h>

#include "mem.h"

unsigned long *mem_init(unsigned long phy_addr, unsigned long size)
{
    unsigned long *base;
    int fd;

    fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0)
        return NULL;


    base = (unsigned long *)mmap((unsigned long *)phy_addr, size, 
                    PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (!base) {
        close(fd);
        return NULL;
    }

    return base;
}

void mem_exit(int fd, unsigned long *base, unsigned long size)
{
    close(fd);
    munmap(base, size);
}
