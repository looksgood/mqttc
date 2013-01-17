/*
 *
 * client.c - mqtt client main.
 *
 * Copyright (c) 2013 Ery Lee <ery.lee at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of mqttc nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
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
	
	mqtt = mqtt_new();
	
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
	//mqtt->keepalive = 60;
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
on_connect(Mqtt *mqtt, void *data, int state) {
	logger_info("MQTT", "on_connect: %d", state);
}

static void on_connack(Mqtt *mqtt, void *data, int rc) {
	logger_info("MQTT", "on_connack: %d", rc);
}

static void on_publish(Mqtt *mqtt, void *data, int msgid) {
	MqttMsg *msg = (MqttMsg *)data;
	logger_info("MQTT", "on_publish: topic=%s, msgid=%d, payload=%s", msg->topic, msgid, msg->payload);
}

static void on_puback(Mqtt *mqtt, void *data, int msgid) {
	logger_info("MQTT", "on_puback: msgid=%d", msgid);
}

static void on_pubrec(Mqtt *mqtt, void *data, int msgid) {
	logger_info("MQTT", "on_pubrec: msgid=%d", msgid);
}

static void on_pubrel(Mqtt *mqtt, void *data, int msgid) {
	logger_info("MQTT", "on_pubrel: msgid=%d", msgid);
}

static void on_pubcomp(Mqtt *mqtt, void *data, int msgid) {
	logger_info("MQTT", "on_pubcomp: msgid=%d", msgid);
}

static void on_subscribe(Mqtt *mqtt, void *data, int msgid) {
	char *topic = (char *)data;
	logger_info("MQTT", "on_subscribe: topic=%s, msgid=%d", topic, msgid);
}

static void on_suback(Mqtt *mqtt, void *data, int msgid) {
	logger_info("MQTT", "on_suback: msgid=%d", msgid);
}

static void on_unsubscribe(Mqtt *mqtt, void *data, int msgid) {
	logger_info("MQTT", "on_unsubscribe: msgid=%d", msgid);
}

static void on_unsuback(Mqtt *mqtt, void *data, int msgid) {
	logger_info("MQTT", "on_unsuback: msgid=%d", msgid);
}

static void on_pingreq(Mqtt *mqtt, void *data, int id) {
	logger_info("MQTT", "on_pingreq");
}

static void on_pingresp(Mqtt *mqtt, void *data, int id) {
	logger_info("MQTT", "on_pingresp");
}

static void on_disconnect(Mqtt *mqtt, void *data, int id) {
	logger_info("MQTT", "on_disconnect");
}

static void on_message(Mqtt *mqtt, MqttMsg *msg) {

}

void set_callbacks(Mqtt *mqtt) {
	int i = 0;
	command_callback callbacks[15] = {
		NULL,
		on_connect,
		on_connack,
		on_publish,
		on_puback,
		on_pubrec,
		on_pubrel,
		on_pubcomp,
		on_subscribe,
		on_suback,
		on_unsubscribe,
		on_unsuback,
		on_pingreq,
		on_pingresp,
		on_disconnect
	};
	for(i = 0; i < 15; i++) {
		mqtt_set_command_callback(mqtt, i, callbacks[i]);
	}
	mqtt_set_message_callback(mqtt, on_message);
}

int main(int argc, char **argv) {
	client_prepare();
	printf("mqttc is prepared\n");
	client_init();
	printf("mqttc is inited\n");
	set_callbacks(mqtt);
	printf("mqttc set callbacks \n");
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

	mqtt_run(mqtt);

	return 0;
}

