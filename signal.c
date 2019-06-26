/*
 * Signal
 *
 * (C) 2019.05.14 <buddy.zhang@aliyun.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>

#include <base.h>

/* Handle signal from system */
static void do_signal(int sig)
{
	struct vc_node *vc;

	switch (sig) {
	case SIGSTOP:
		/* CTRL Z */
	case SIGINT:
		/* CTRL C */
	case SIGQUIT:
		/* CTRL \ */
	case SIGTERM:
		/* Termnal */
	case SIGKILL:
		/* Kill -9 */
		/* Relase */
		vc = vc_root();

		vc->flags = 0;	
		break;
	default:
		fprintf(stderr, "Undefined signal.\n");
	}
}

/* Initialize all signal */
void signal_init(void)
{
	signal(SIGSTOP, do_signal);
	signal(SIGINT,  do_signal);
	signal(SIGQUIT, do_signal);
	signal(SIGKILL, do_signal);
	signal(SIGTERM, do_signal);
}
