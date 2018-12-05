#ifndef _MEM_H_
#define _MEM_H_

extern unsigned long *mem_init(unsigned long phy_addr, unsigned long size);

extern void mem_exit(int fd, unsigned long *base, unsigned long size);

#endif
