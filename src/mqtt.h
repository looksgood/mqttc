/* 
**
** mqtt.h - mqtt client api
**
** Copyright (c) 2013  Ery Lee <ery.lee at gmail dot com>
** All rights reserved.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License version 2 as
** published by the Free Software Foundation.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
*/

#ifndef __MQTT_H
#define __MQTT_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>

#include "ae.h"
#include "list.h"
#include "packet.h"

#define MQTT_OK 0
#define MQTT_ERR -1

#define PROTOCOL_MAGIC "MQIsdp"
#define PROTOCOL_VERSION "MQTT/3.1"

#define MQTT_PROTO_MAJOR 3
#define MQTT_PROTO_MINOR 1

#define QOS_0 0
#define QOS_1 1
#define QOS_2 2

#define LSB(A) (unsigned char)(A & 0x00FF)
#define MSB(A) (unsigned char)((A & 0xFF00) >> 8)

enum MsgType {
    CONNECT = 1,
	CONNACK,
	PUBLISH,
	PUBACK,
	PUBREC,
	PUBREL,
    PUBCOMP,
	SUBSCRIBE,
	SUBACK,
	UNSUBSCRIBE,
	UNSUBACK,
    PINGREQ,
	PINGRESP,
	DISCONNECT
};

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

//--------------------------------------
/* State for the mqtt frame reader */
//--------------------------------------
typedef struct _MqttReader {
    int err; /* Error flags, 0 when there is no error */
    char errstr[128]; /* String representation of error when applicable */

    char *buf; /* Read buffer */
    size_t pos; /* Buffer cursor */
    size_t len; /* Buffer length */
    size_t maxbuf; /* Max length of unused buffer */

    void *privdata;
} MqttReader;

typedef struct _Mqtt {

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

    int keepalive;

    long long keepalive_timeout;

    void *userdata;

    list *presences;

    list *conn_callbacks;

	//TODO: NEED?
    list *presence_callbacks;

    list *message_callbacks;

	MqttWill *will;

	MqttReader *reader;

} Mqtt;

Mqtt *mqtt_new(aeEventLoop *el);

void mqtt_set_clientid(Mqtt *mqtt, char *clientid);

void mqtt_set_username(Mqtt *mqtt, char *username);

void mqtt_set_passwd(Mqtt *mqtt, char *passwd);

void mqtt_set_user(Mqtt *mqtt, char *user, char *passwd);

void mqtt_set_server(Mqtt *mqtt, char *server);

void mqtt_set_port(Mqtt *mqtt, int port);

//MQTT Will
void mqtt_set_will(Mqtt *mqtt, MqttWill *will); 

void mqtt_clear_will(Mqtt *mqtt);

//CONNECT
int mqtt_connect(Mqtt *mqtt);

int mqtt_reconnect(aeEventLoop *el, long long id, void *clientData);

typedef void (*mqtt_conn_callback)(Mqtt *mqtt, int connack);

void mqtt_add_conn_callback(Mqtt *mqtt, mqtt_conn_callback callback); 

//MESSAGE CALLBACK
typedef void (*mqtt_message_callback)(Mqtt *mqtt, MqttMsg *message);

void mqtt_add_message_callback(Mqtt *mqtt, mqtt_message_callback callback);

void mqtt_remove_message_callback(Mqtt *mqtt, mqtt_message_callback callback);

//PUBLISH
//return msgid
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
int mqtt_unsubscribe(Mqtt *mqtt, char *topic);

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

MqttReader *mqtt_reader_new();

int mqtt_reader_feed(MqttReader *reader, char *buf, int len);

void mqtt_reader_free(MqttReader *reader);



#endif /* __MQTT_H__ */

