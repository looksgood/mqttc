/*
**
** client.c - mqtt client main.
**
** Copyright (c) 2013 Ery Lee <ery.lee at gmail dot com>
** All rights reserved.
**
*/

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "ae.h"
#include "anet.h"
#include "mqtt.h"
#include "logger.h"
#include "zmalloc.h"
#include "packet.h"
#include "client.h"

static Mqtt *mqtt;

static Client client;

static MqttWill will = {0, 0, "will", "WillMessage"};

static MqttMsg testmsg = {0, 0, 0, 0, "a/b/c", 5, "hello"};

static void
client_prepare() {
	
	mqtt = mqtt_new(aeCreateEventLoop());
	
	mqtt->state = 0;
	mqtt->server = "localhost";
	mqtt->username = "test";
	mqtt->password = "public";
	mqtt->clientid = "mqttc";
	mqtt->port = 1883;
	mqtt->retries = 3;
	mqtt->error = 0;
	mqtt->msgid = 1;
	mqtt->cleansess = 1;
	mqtt->keepalive = 60;
	mqtt->will = &will;

	client.mqtt = mqtt;
	client.pidfile = -1;
	client.daemonize = 0;
	client.shutdown_asap = 0;
}

static int 
client_cron(aeEventLoop *eventLoop,
    long long id, void *clientData) {
    if(client.shutdown_asap) {
        //TODO: ok???
		logger_info("MQTTC", "shutdown.");
        aeStop(mqtt->el);
        if(client.daemonize) {
            //unlink(client.pidfile);
        }
    }
    return 1000;
}

static void 
client_init() {
    signal(SIGCHLD, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    //setupSignalHandlers();

    aeCreateTimeEvent(mqtt->el, 100, client_cron, NULL, NULL);

    srand(time(NULL)^getpid());
}


static void 
before_sleep(struct aeEventLoop *eventLoop) {
}

static void 
client_run() {
    aeSetBeforeSleepProc(mqtt->el, before_sleep);
    aeMain(mqtt->el);
    aeDeleteEventLoop(mqtt->el);
}

int main(int argc, char **argv) {
	client_prepare();
	printf("mqttc is prepared\n");
	client_init();
	printf("mqttc is inited\n");
	if(mqtt_connect(mqtt) < 0) {
        logger_error("mqttc", "mqtt connect failed.");
        exit(-1);
    }
	printf("mqttc is running\n");

	mqtt_subscribe(mqtt, "c/d/e", QOS_0);
	mqtt_unsubscribe(mqtt, "c/d/e");
	mqtt_subscribe(mqtt, "c/d/e", QOS_0);

	mqtt_publish(mqtt, &testmsg);

	testmsg.qos = 1;

	mqtt_publish(mqtt, &testmsg);

	client_run();

	return 0;
}

