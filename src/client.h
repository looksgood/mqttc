#ifndef __CLIENT_H
#define __CLIENT_H

typedef struct _Client {
	int pidfile;
	int daemonize;
	int shutdown_asap;
	Mqtt *mqtt;
} Client;

#endif
