/*
 * Tun-Tap.
 *
 * (C) 2019.05.14 <buddy.zhang@aliyun.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/types.h>
#include <errno.h>
#include <net/route.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

static int interface_up(char *interface_name)
{
	int s;
	struct ifreq ifr;
	short flag;

	if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Error create socket :%d\n", errno);
		return -1;
	}

	strcpy(ifr.ifr_name, interface_name);

	flag = IFF_UP;
	if (ioctl(s, SIOCGIFFLAGS, &ifr) < 0) {
		printf("Error up %s :%d\n", interface_name, errno);
		return -1;
	}

	ifr.ifr_ifru.ifru_flags |= flag;

	if (ioctl(s, SIOCSIFFLAGS, &ifr) < 0) {
		printf("Error up %s :%d\n", interface_name, errno);
		return -1;
	}
	return 0;
}

static int set_ipaddr(char *interface_name, char *ip)
{
	int s;
	struct ifreq ifr;
	struct sockaddr_in addr;

	if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Error up %s :%d\n", interface_name, errno);
		return -1;
	}

	strcpy(ifr.ifr_name, interface_name);

	bzero(&addr, sizeof(struct sockaddr_in));
	addr.sin_family = PF_INET;
	inet_aton(ip, &addr.sin_addr);

	memcpy(&ifr.ifr_ifru.ifru_addr, &addr, sizeof(struct sockaddr_in));

	if (ioctl(s, SIOCSIFADDR, &ifr) < 0) {
		printf("Error set %s ip :%d\n", interface_name, errno);
		return -1;
	}
	return 0;
}

static int netmask_set(char *interface_name, char *netmask)
{
	int sock_netmask;
	struct ifreq ifr_mask;
	struct sockaddr_in *sin_net_mask;

	sock_netmask = socket(AF_INET, SOCK_STREAM, 0);
	if( sock_netmask == -1)
		return -1;

	memset(&ifr_mask, 0, sizeof(ifr_mask));
	strncpy(ifr_mask.ifr_name, interface_name, 
				sizeof(ifr_mask.ifr_name) -1);
	sin_net_mask = (struct sockaddr_in *)&ifr_mask.ifr_addr;
	sin_net_mask->sin_family = AF_INET;
	inet_pton(AF_INET, netmask, &sin_net_mask->sin_addr);

	if(ioctl(sock_netmask, SIOCSIFNETMASK, &ifr_mask) < 0)
		return -1;

	return 0;
}

static int route_add(char *interface_name)
{
	int skfd;
	struct rtentry rt;
	struct sockaddr_in dst;
	struct sockaddr_in genmask;

	memset(&rt, 0, sizeof(rt));

	genmask.sin_addr.s_addr = inet_addr("255.255.255.255");
	bzero(&dst, sizeof(struct sockaddr_in));
	dst.sin_family = PF_INET;
	dst.sin_addr.s_addr = inet_addr("10.0.0.1");

	rt.rt_metric = 0;
	rt.rt_dst = *(struct sockaddr *)&dst;
	rt.rt_genmask = *(struct sockaddr *)&genmask;

	rt.rt_dev = interface_name;
	rt.rt_flags = RTF_UP | RTF_HOST;

	skfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (ioctl(skfd, SIOCADDRT, &rt) < 0) {
		printf("Error route add :%d\n", errno);
		return -1;
	}
	return 0;
}

/*
 * Establish a tun device.
 */
int tun_create(char *dev, int flags, char *ip)
{
	struct ifreq ifr;
	int fd, err;

	if ((fd = open("/dev/net/tun", O_RDWR | O_NONBLOCK)) < 0) {
		printf("Error :%d\n", errno);
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags |= flags;

	if (*dev != '\0') {
		strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	}

	if ((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0) {
		printf("Error :%d\n", errno);
		close(fd);
		return -1;
	}

	strcpy(dev, ifr.ifr_name);

	/* Set up interface */
	interface_up(dev);
	/* Set up route table */
	route_add(dev);
	/* Set up ip */
	set_ipaddr(dev, ip);

	/* Set netmask */
	netmask_set(dev, "255.255.255.0");

	return fd;
}

/* Release a tun device */
void tun_release(int fd)
{
	close(fd);
}
