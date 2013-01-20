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
#include <stdbool.h>

#include "ae.h"

#define MQTT_OK 0
#define MQTT_ERR -1

#define MQTT_PROTO_MAJOR 3
#define MQTT_PROTO_MINOR 1

#define MQTT_PROTOCOL_VERSION "MQTT/3.1"

#define MQTT_ERR_SOCKET (-5)

/*
 * MQTT QOS
 */
#define MQTT_QOS0 0
#define MQTT_QOS1 1
#define MQTT_QOS2 2

/*
 * MQTT ConnAck
 */
typedef enum {
	CONNACK_ACCEPT  = 0,
	CONNACK_PROTO_VER, 
	CONNACK_INVALID_ID,
	CONNACK_SERVER,
	CONNACK_CREDENTIALS,
	CONNACK_AUTH
} ConnAck;

/*
 * MQTT State
 */
typedef enum {
	MQTT_STATE_INIT = 0,
	MQTT_STATE_CONNECTING,
	MQTT_STATE_CONNECTED,
	MQTT_STATE_DISCONNECTED
} MqttState;

/*
 * MQTT Will
 */
typedef struct {
	bool retain;
	uint8_t qos;
	const char *topic;
	const char *msg;
} MqttWill;

/*
 * MQTT Message
 */
typedef struct {
	uint16_t id;
	uint8_t qos;
	bool retain;
	bool dup;
	const char *topic;
	int payloadlen;
	const char *payload;
} MqttMsg;

typedef struct _Mqtt Mqtt;

typedef void (*MqttCallback)(Mqtt *mqtt, void *data, int id);

typedef void (*MqttMsgCallback)(Mqtt *mqtt, MqttMsg *message);

struct _Mqtt {

	aeEventLoop *el;

    int fd; //socket

	uint8_t state;

    int error;

	char errstr[1024];

    char *server;

    const char *username;

    const char *password;

	const char *clientid;

    int port;

    int retries;

	int msgid;

	bool cleansess;

    /* keep alive */

	unsigned int keepalive;

	long long keepalive_timer;

	long long keepalive_timeout_timer;

    void *userdata;

	MqttWill *will;

	MqttCallback callbacks[16];

	MqttMsgCallback msgcallback;

	bool shutdown_asap;

};

char *mqtt_packet_name(int type);

Mqtt *mqtt_new(aeEventLoop *el);

void mqtt_set_clientid(Mqtt *mqtt, const char *clientid);

void mqtt_set_username(Mqtt *mqtt, const char *username);

void mqtt_set_passwd(Mqtt *mqtt, const char *passwd);

void mqtt_set_server(Mqtt *mqtt, const char *server);

void mqtt_set_port(Mqtt *mqtt, int port);

void mqtt_set_retries(Mqtt *mqtt, int retries);

void mqtt_set_will(Mqtt *mqtt, MqttWill *will); 

void mqtt_clear_will(Mqtt *mqtt);

void mqtt_set_keepalive(Mqtt *mqtt, int keepalive);

void mqtt_set_callback(Mqtt *mqtt, uint8_t type, MqttCallback callback); 

void mqtt_clear_callback(Mqtt *mqtt, uint8_t type);

void mqtt_set_msg_callback(Mqtt *mqtt, MqttMsgCallback callback);

void mqtt_clear_msg_callback(Mqtt *mqtt);

//MQTT CONNECT
int mqtt_connect(Mqtt *mqtt);

//MQTT PUBLISH
int mqtt_publish(Mqtt *mqtt, MqttMsg *msg);

//PUBACK for QOS1, QOS2 
void mqtt_puback(Mqtt *mqtt, int msgid);

//PUBREC for QOS_2
void mqtt_pubrec(Mqtt *mqtt, int msgid);

//PUBREL for QOS_2
void mqtt_pubrel(Mqtt *mqtt, int msgid);

//PUBCOMP for QOS_2
void mqtt_pubcomp(Mqtt *mqtt, int msgid);

//SUBSCRIBE
int mqtt_subscribe(Mqtt *mqtt, const char *topic, uint8_t qos);

//UNSUBSCRIBE
int mqtt_unsubscribe(Mqtt *mqtt, const char *topic);

//PINGREQ
void mqtt_ping(Mqtt *mqtt);

//DISCONNECT
void mqtt_disconnect(Mqtt *mqtt);

//RUN Loop
void mqtt_run(Mqtt *mqtt);

//RELEASE
void mqtt_release(Mqtt *mqtt);

//Will create and release
MqttWill *mqtt_will_new(char *topic, char *msg, bool retain, uint8_t qos);

void mqtt_will_release(MqttWill *will);

//Message create and release
MqttMsg * mqtt_msg_new(int msgid, int qos, bool retain, bool dup, char *topic, int payloadlen, char *payload);

const char* mqtt_msg_name(uint8_t type);

void mqtt_msg_free(MqttMsg *msg);

#endif /* __MQTT_H__ */

