#ifndef __CLIENT_H
#define __CLIENT_H

#include "ae.h"
#include "anet.h"
#include "mqtt.h"
#include "logger.h"
#include "zmalloc.h"
#include "packet.h"

typedef struct _Client {
	int pidfile;
	int daemonize;
	int shutdown_asap;
	Mqtt *mqtt;
} Client;

#endif
