#ifndef _TAP_H_
#define _TAP_H_

extern int tun_create(char *dev, int flags);
extern void tun_release(int fd);

#endif
