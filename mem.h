#ifndef _MEM_H_
#define _MEM_H_

extern int mem_init(unsigned long phy_addr, unsigned long size,      
                            unsigned long *base);

extern void mem_exit(int fd, unsigned long *base, unsigned long size);

#endif
