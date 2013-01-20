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
#include <stdbool.h>
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
#include "zmalloc.h"
#include "packet.h"
#include "client.h"

#define _NOTUSED(V) ((void)V)

static Client client;

static const char *PROMPT = "mqttc> ";

static const char *COMMANDS[3] = {
	"publish topic qos message\n",
	"subscribe topic qos\n",
	"unsubscribe topic\n"
};

static void
print_usage() {
	printf("usage: mqttc -h host -p port -u username -P password -k keepalive\n");
}

static void 
print_prompt() {
	write(STDOUT_FILENO, PROMPT, strlen(PROMPT));
}

static void
print_help() {
	int i;
	const char *cmd;
	const char *help = "commands are: \n";
	write(STDOUT_FILENO, help, strlen(help));
	for(i = 0; i < 3; i++) {
		cmd = COMMANDS[i];
		write(STDOUT_FILENO, cmd, strlen(cmd));
	}
}

static int 
client_cron(aeEventLoop *el, long long id, void *clientData) {
	Client *client = (Client *)clientData;
	_NOTUSED(el);
	_NOTUSED(id);
    if(client->shutdown_asap) {
		printf("mqttc is shutdown...");
        aeStop(el);
    }
    return 1000;
}

static void
client_prepare() {
    srand(time(NULL)^getpid());
}

static void
mqtt_init(Mqtt *mqtt) {
	char clientid[23];
	sprintf(clientid, "mqttc%d", rand());
	mqtt->state = 0;
	mqtt_set_clientid(mqtt, clientid);
	mqtt->port = 1883;
	mqtt->retries = 3;
	mqtt->error = 0;
	mqtt->msgid = 1;
	mqtt->cleansess = 1;
	mqtt->keepalive = 60;
}

static void
client_init() {
	aeEventLoop *el;
	el = aeCreateEventLoop();
	client.el = el;
	client.mqtt = mqtt_new(el);
	client.shutdown_asap = false;
	mqtt_init(client.mqtt);

    signal(SIGCHLD, SIG_IGN);
    //signal(SIGPIPE, SIG_IGN);

    aeCreateTimeEvent(el, 100, client_cron, &client, NULL);
}

static void 
on_connect(Mqtt *mqtt, void *data, int state) {
	_NOTUSED(data);
	switch(state) {
	case MQTT_STATE_CONNECTING:
		printf("mqttc is connecting to %s:%d...\n", mqtt->server, mqtt->port);
		break;
	case MQTT_STATE_CONNECTED:
		printf("mqttc is connected.\n");
		print_prompt();
		break;
	case MQTT_STATE_DISCONNECTED:
		printf("mqttc is disconnected.\n");
		break;
	default:
		printf("mqttc is in badstate.\n");
	}
}

static void 
on_connack(Mqtt *mqtt, void *data, int rc) {
	_NOTUSED(mqtt);
	_NOTUSED(data);
	printf("received connack: code=%d\n", rc);
}

static void 
on_publish(Mqtt *mqtt, void *data, int msgid) {
	_NOTUSED(mqtt);
	_NOTUSED(msgid);
	MqttMsg *msg = (MqttMsg *)data;
	printf("publish to %s: %s\n", msg->topic, msg->payload);
}

static void 
on_puback(Mqtt *mqtt, void *data, int msgid) {
	_NOTUSED(mqtt);
	_NOTUSED(data);
	printf("received puback: msgid=%d\n", msgid);
}

static void 
on_pubrec(Mqtt *mqtt, void *data, int msgid) {
	_NOTUSED(mqtt);
	_NOTUSED(data);
	printf("received pubrec: msgid=%d\n", msgid);
}

static void 
on_pubrel(Mqtt *mqtt, void *data, int msgid) {
	_NOTUSED(mqtt);
	_NOTUSED(data);
	printf("received pubrel: msgid=%d\n", msgid);
}

static void 
on_pubcomp(Mqtt *mqtt, void *data, int msgid) {
	_NOTUSED(mqtt);
	_NOTUSED(data);
	printf("received pubcomp: msgid=%d\n", msgid);
}

static void 
on_subscribe(Mqtt *mqtt, void *data, int msgid) {
	_NOTUSED(mqtt);
	char *topic = (char *)data;
	printf("subscribe to %s: msgid=%d\n", topic, msgid);
}

static void 
on_suback(Mqtt *mqtt, void *data, int msgid) {
	_NOTUSED(mqtt);
	_NOTUSED(data);
	printf("received suback: msgid=%d\n", msgid);
}

static void 
on_unsubscribe(Mqtt *mqtt, void *data, int msgid) {
	_NOTUSED(mqtt);
	_NOTUSED(data);
	printf("unsubscribe %s: msgid=%d\n", (char *)data, msgid);
}

static void 
on_unsuback(Mqtt *mqtt, void *data, int msgid) {
	_NOTUSED(mqtt);
	_NOTUSED(data);
	printf("received unsuback: msgid=%d\n", msgid);
}

static void 
on_pingreq(Mqtt *mqtt, void *data, int id) {
	_NOTUSED(mqtt);
	_NOTUSED(data);
	_NOTUSED(id);
	//printf("send pingreq\n");
}

static void 
on_pingresp(Mqtt *mqtt, void *data, int id) {
	_NOTUSED(mqtt);
	_NOTUSED(data);
	_NOTUSED(id);
	//printf("received pingresp\n");
}

static void 
on_disconnect(Mqtt *mqtt, void *data, int id) {
	_NOTUSED(mqtt);
	_NOTUSED(data);
	_NOTUSED(id);
	printf("disconnect\n");
}

static void 
on_message(Mqtt *mqtt, MqttMsg *msg) {
	_NOTUSED(mqtt);
	printf("received message: topic=%s, payload=%s\n", msg->topic, msg->payload);
}

static void 
set_callbacks(Mqtt *mqtt) {
	int i = 0, type;
	MqttCallback callbacks[15] = {
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
		type = (i << 4) & 0xf0;
		mqtt_set_callback(mqtt, type, callbacks[i]);
	}
	mqtt_set_msg_callback(mqtt, on_message);
}

static int 
setargs(char *args, char **argv) {
	int argc = 0;
	while (isspace(*args)) ++args;
	while (*args) {
		if (argv) argv[argc] = args;
		while (*args && !isspace(*args)) ++args;
		if (argv && *args) *args++ = '\0';
		while (isspace(*args)) ++args;
		argc++;
	}
	return argc;
}

static void
client_read(aeEventLoop *el, int fd, void *clientdata, int mask) {
	int nread = 0;
	char buffer[1024] = {0};
	const char *badcmd = "Invalid Command. try 'help'\n";
	int argc;
	char *argv[1024];
	MqttMsg *msg;
	_NOTUSED(el);
	_NOTUSED(mask);
	_NOTUSED(clientdata);
	nread = read(fd, buffer, 1024);
	if(nread <= 0) {
		client.shutdown_asap = true;
		return;
	}
	if(!strncmp(buffer, "help", 4) || !strncmp(buffer, "?", 1)) {
		print_help();
	} else if(!strncmp(buffer, "subscribe ", strlen("subscribe "))) {
		argc = setargs(buffer+strlen("subscribe "), argv); 
		if(argc == 2) {
			mqtt_subscribe(client.mqtt, argv[0], atoi(argv[1]));
		} else {
			print_help();
		}
	} else if(!strncmp(buffer, "unsubscribe ", strlen("unsubscribe "))) {
		argc = setargs(buffer+strlen("unsubscribe "), argv);
		if(argc == 1) {
			mqtt_unsubscribe(client.mqtt, argv[0]);
		} else {
			print_help();
		}
	} else if(!strncmp(buffer, "publish ", strlen("publish "))) {
		argc = setargs(buffer+strlen("publish "), argv);
		if(argc == 3) {
			msg = mqtt_msg_new(0, atoi(argv[1]), false, false,
				zstrdup(argv[0]), strlen(argv[2]), zstrdup(argv[2]));
			mqtt_publish(client.mqtt, msg);
			mqtt_msg_free(msg);
		} else {
			print_help();
		}
	} else if (!strncmp(buffer, "\n", 1)){
		//ignore
	} else {
		write(STDOUT_FILENO, badcmd, strlen(badcmd));
	}
	print_prompt();
}

static void
client_open() {
	aeCreateFileEvent(client.el, STDIN_FILENO, AE_READABLE, client_read, &client);
}

static void
client_setup(int argc, char **argv) {
	char c;
	Mqtt *mqtt = client.mqtt;
	while ((c = getopt(argc, argv, "Hh:p:u:P:k:")) != -1) {
        switch (c) {
        case 'h':
			mqtt_set_server(mqtt, optarg);
            break;
        case 'p':
			mqtt_set_port(mqtt, atoi(optarg));
            break;
        case 'u':
			mqtt_set_username(mqtt, optarg);
            break;
		case 'P':
			mqtt_set_passwd(mqtt, optarg);
            break;
		case 'k':
			mqtt_set_keepalive(mqtt, atoi(optarg));
			break;
		case 'H':
            print_usage();
			exit(0);
		}
    }
	if(!mqtt->server) mqtt_set_server(mqtt, "localhost");
}

int main(int argc, char **argv) {
	client_prepare();
	//init
	client_init();

	//parse args
	client_setup(argc, argv);

	//set stdin event
	client_open();

	//set callbacks
	set_callbacks(client.mqtt);
	
	if(mqtt_connect(client.mqtt) < 0) {
        printf("mqttc connect failed.\n");
        exit(-1);
    }

	mqtt_run(client.mqtt);

	return 0;
}

