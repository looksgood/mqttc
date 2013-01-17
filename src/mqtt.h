/* 
 * mqtt.h - mqtt client api
 *
 * Copyright (c) 2013  Ery Lee <ery.lee at gmail dot com>
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

#ifndef __MQTT_H
#define __MQTT_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>

#include "ae.h"
#include "packet.h"

#define MQTT_OK 0
#define MQTT_ERR -1

#define PROTOCOL_VERSION "MQTT/3.1"

#define MQTT_PROTO_MAJOR 3
#define MQTT_PROTO_MINOR 1

#define QOS_0 0
#define QOS_1 1
#define QOS_2 2

enum ConnAckCode {
	CONNACK_ACCEPT  = 0,
	CONNACK_PROTO_VER, 
	CONNACK_INVALID_ID,
	CONNACK_SERVER,
	CONNACK_CREDENTIALS,
	CONNACK_AUTH
};

enum MqttState {
	STATE_INIT = 0,
	STATE_CONNECTING,
	STATE_CONNECTED,
	STATE_DISCONNECTED
};

//mqtt will.
typedef struct _MqttWill {
	int retain;
	int qos;
	char *topic;
	char *msg;
} MqttWill;

typedef struct _MqttMsg {
	int id;
	int qos;
	int retain;
	int dup;
	char *topic;
	int payloadlen;
	char *payload;
} MqttMsg;

typedef struct _KeepAlive {
	int period;
	long long timerid;
	long long timeoutid;
} KeepAlive;


typedef struct _Mqtt Mqtt;

//Command Callback
typedef void (*command_callback)(Mqtt *mqtt, void *data, int id);

//Message Callback
typedef void (*message_callback)(Mqtt *mqtt, MqttMsg *message);

struct _Mqtt {

	aeEventLoop *el;

    int fd; //socket

	int state;

	char *err;

    char *server;

    char *username;

    char *password;

	char *clientid;

    int port;

    int retries;

    int error;

	int msgid;

    /* keep alive */

	int cleansess;

    KeepAlive *keepalive;

    void *userdata;

	MqttWill *will;

	command_callback callbacks[16];

	message_callback msgcallback;

};

Mqtt *mqtt_new();

void mqtt_run(Mqtt *mqtt);

void mqtt_set_clientid(Mqtt *mqtt, const char *clientid);

void mqtt_set_username(Mqtt *mqtt, const char *username);

void mqtt_set_passwd(Mqtt *mqtt, const char *passwd);

void mqtt_set_server(Mqtt *mqtt, const char *server);

void mqtt_set_port(Mqtt *mqtt, int port);

//MQTT Will
void mqtt_set_will(Mqtt *mqtt, MqttWill *will); 

void mqtt_clear_will(Mqtt *mqtt);

//MQTT KeepAlive
void mqtt_set_keepalive(Mqtt *mqtt, int period);

void mqtt_set_command_callback(Mqtt *mqtt, unsigned char type, command_callback callback); 

void mqtt_clear_command_callback(Mqtt *mqtt, unsigned char type);

void mqtt_set_message_callback(Mqtt *mqtt, message_callback callback);

void mqtt_clear_message_callback(Mqtt *mqtt);

//CONNECT
int mqtt_connect(Mqtt *mqtt);

int mqtt_reconnect(aeEventLoop *el, long long id, void *clientData);

//PUBLISH return msgid
int mqtt_publish(Mqtt *mqtt, MqttMsg *msg);

//PUBACK for QOS_2
void mqtt_puback(Mqtt *mqtt, int msgid);

//PUBREC for QOS_2
void mqtt_pubrec(Mqtt *mqtt, int msgid);

//PUBREL for QOS_2
void mqtt_pubrel(Mqtt *mqtt, int msgid);

//PUBCOMP for QOS_2
void mqtt_pubcomp(Mqtt *mqtt, int msgid);

//SUBSCRIBE
int mqtt_subscribe(Mqtt *mqtt, const char *topic, unsigned char qos);

//UNSUBSCRIBE
int mqtt_unsubscribe(Mqtt *mqtt, const char *topic);

//PINGREQ
void mqtt_ping(Mqtt *mqtt);

//DISCONNECT
void mqtt_disconnect(Mqtt *mqtt);

//RELEASE
void mqtt_release(Mqtt *mqtt);

//Will create and release
MqttWill *mqtt_will_new(char *topic, char *msg, int retain, int qos);

void mqtt_will_release(MqttWill *will);

//-------------------
MqttMsg *mqtt_msg_new();

void mqtt_msg_free(MqttMsg *msg);

#endif /* __MQTT_H__ */

