/* 
 * mqtt.c - mqtt client library
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
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <assert.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include "ae.h"
#include "anet.h"
#include "zmalloc.h"
#include "packet.h"
#include "mqtt.h"

#define MAX_RETRIES 3

#define KEEPALIVE 300

#define KEEPALIVE_TIMEOUT (KEEPALIVE * 2)

#define MQTT_NOTUSED(V) ((void) V)

#define MQTT_BUFFER_SIZE (1024*16)

/*
 * Why Buffer? May be used on resource limited os?
 */
Mqtt *
mqtt_new(aeEventLoop *el) {
	int i = 0;
	Mqtt *mqtt = NULL;
	mqtt = zmalloc(sizeof(Mqtt));

	mqtt->el = el;
	mqtt->state = MQTT_STATE_INIT;
	mqtt->server = NULL;
	mqtt->username = NULL;
	mqtt->password = NULL;
	mqtt->clientid = NULL;
	mqtt->cleansess = true;
	mqtt->port = 1883;
	mqtt->retries = MAX_RETRIES;
	mqtt->error = 0;
	mqtt->msgid = 1;
	mqtt->keepalive = KEEPALIVE;
	for(i = 0; i < 16; i++) {
		mqtt->callbacks[i] = NULL;
	}
	mqtt->msgcallback = NULL;
	return mqtt;
}

void 
mqtt_set_state(Mqtt *mqtt, int state) {
	mqtt->state = state;
}

void
mqtt_set_clientid(Mqtt *mqtt, const char *clientid) {
	mqtt->clientid = zstrdup(clientid);
}

void 
mqtt_set_username(Mqtt *mqtt, const char *username) {
	mqtt->username = zstrdup(username);
}

void mqtt_set_passwd(Mqtt *mqtt, const char *passwd) {
	mqtt->password = zstrdup(passwd);
}

void 
mqtt_set_server(Mqtt *mqtt, const char *server) {
	mqtt->server = zstrdup(server);
}

void 
mqtt_set_port(Mqtt *mqtt, int port) {
	mqtt->port = port;
}

void 
mqtt_set_retries(Mqtt *mqtt, int retries) {
	mqtt->retries = retries;
}

void
mqtt_set_cleansess(Mqtt *mqtt, bool cleansess) {
	mqtt->cleansess = cleansess;
}

void 
mqtt_set_will(Mqtt *mqtt, MqttWill *will) {
	mqtt->will = will;
}

void 
mqtt_clear_will(Mqtt *mqtt) {
	if(mqtt->will) mqtt_will_release(mqtt->will);
	mqtt->will = NULL;
}

void 
mqtt_set_keepalive(Mqtt *mqtt, int keepalive) {
	mqtt->keepalive = keepalive;
}

void 
mqtt_set_callback(Mqtt *mqtt, uint8_t type, MqttCallback callback) {
	if(type < 0) return;
	type = (type >> 4) & 0x0F;
	if(type > 16) return;
	mqtt->callbacks[type] = callback;
}

static void 
_mqtt_callback(Mqtt *mqtt, int type, void *data, int id) {
	if(type < 0) return;
	type = (type >> 4) & 0x0F;
	if(type > 16) return;
	MqttCallback cb = mqtt->callbacks[type];
	if(cb) cb(mqtt, data, id);
}

void 
mqtt_clear_callback(Mqtt *mqtt, unsigned char type) {
	if(type >= 16) return;
	mqtt->callbacks[type] = NULL;
}

void 
mqtt_set_msg_callback(Mqtt *mqtt, MqttMsgCallback callback) {
	mqtt->msgcallback = callback;
}

static void 
_mqtt_msg_callback(Mqtt *mqtt, MqttMsg *msg) {
	if(mqtt->msgcallback) mqtt->msgcallback(mqtt, msg);
}

void 
mqtt_clear_msg_callback(Mqtt *mqtt) {
	mqtt->msgcallback = NULL;
}

static void
_mqtt_set_error(char *err, const char *fmt, ...) {
    va_list ap;
    if (!err) return;
    va_start(ap, fmt);
    vsnprintf(err, 1023, fmt, ap);
    va_end(ap);
}

static void 
_mqtt_send_connect(Mqtt *mqtt) {
	int len = 0;
	char *ptr, *buffer=NULL;

	uint8_t header = CONNECT;
	uint8_t flags = 0;

	int remaining_count = 0;
	char remaining_length[4];

	//header
	header = SETQOS(header, MQTT_QOS1);
	
	//flags
	flags = FLAG_CLEANSESS(flags, mqtt->cleansess);
	flags = FLAG_WILL(flags, (mqtt->will) ? 1 : 0);
	if (mqtt->will) {
		flags = FLAG_WILLQOS(flags, mqtt->will->qos);
		flags = FLAG_WILLRETAIN(flags, mqtt->will->retain);
	}
	if (mqtt->username) flags = FLAG_USERNAME(flags, 1);
	if (mqtt->password) flags = FLAG_PASSWD(flags, 1);

	//length
	if(mqtt->clientid) {
		len = 12 + 2 + strlen(mqtt->clientid);
	}
	if(mqtt->will) {
		len += 2 + strlen(mqtt->will->topic);
		len += 2 + strlen(mqtt->will->msg);
	}
	if(mqtt->username) {
		len += 2 + strlen(mqtt->username);
	}
	if(mqtt->password) {
		len += 2 + strlen(mqtt->password);
	}
	
	remaining_count = _encode_remaining_length(remaining_length, len);

	ptr = buffer = zmalloc(1+remaining_count+len);
	
	_write_header(&ptr, header);
	_write_remaining_length(&ptr, remaining_length, remaining_count);
	_write_string_len(&ptr, PROTOCOL_MAGIC, 6);
	_write_char(&ptr, MQTT_PROTO_MAJOR);
	_write_char(&ptr, flags);
	_write_int(&ptr, mqtt->keepalive);
	_write_string(&ptr, mqtt->clientid);

	if(mqtt->will) {
		_write_string(&ptr, mqtt->will->topic);
		_write_string(&ptr, mqtt->will->msg);
	}
	if (mqtt->username) {
		_write_string(&ptr, mqtt->username);
	}
	if (mqtt->password) {
		_write_string(&ptr, mqtt->password);
	}

	anetWrite(mqtt->fd, buffer, ptr-buffer);

	zfree(buffer);
}

static void _mqtt_read(aeEventLoop *el, int fd, void *privdata, int mask);

int 
mqtt_connect(Mqtt *mqtt) {
    char server[1024] = {0};
    if(anetResolve(mqtt->errstr, mqtt->server, server) != ANET_OK) {
		return -1;
    } 
    int fd = anetTcpConnect(mqtt->errstr, server, mqtt->port);
    if (fd < 0) {
        return fd;
    }
    mqtt->fd = fd;
    aeCreateFileEvent(mqtt->el, fd, AE_READABLE, (aeFileProc *)_mqtt_read, (void *)mqtt); 
	_mqtt_send_connect(mqtt);
    mqtt_set_state(mqtt, MQTT_STATE_CONNECTING);
	_mqtt_callback(mqtt, CONNECT, NULL, MQTT_STATE_CONNECTING);

    return fd;
}

static int 
_mqtt_reconnect(aeEventLoop *el, long long id, void *clientData)
{
    int fd;
    int timeout;
	MQTT_NOTUSED(id);
    Mqtt *mqtt = (Mqtt*)clientData;
    fd = mqtt_connect((Mqtt *)clientData);
    if(fd < 0) {
        if(mqtt->retries > MAX_RETRIES) {
            mqtt->retries = 1;
        } 
        timeout = ((2 * mqtt->retries) * 60) * 1000;
        aeCreateTimeEvent(el, timeout, _mqtt_reconnect, mqtt, NULL);
        mqtt->retries++;
    } else {
        mqtt->retries = 1;
    }
    return AE_NOMORE;
}

static void 
_mqtt_send_publish(Mqtt *mqtt, MqttMsg *msg) {
	int len = 0;
	char *ptr, *buffer;
	char remaining_length[4];
	int remaining_count;

	uint8_t header = PUBLISH;
	header = SETRETAIN(header, msg->retain);
	header = SETQOS(header, msg->qos);
	header = SETDUP(header, msg->dup);

	len += 2+strlen(msg->topic);

	if(msg->qos > MQTT_QOS0) len += 2; //msgid

	if(msg->payload) len += msg->payloadlen;
	
	remaining_count = _encode_remaining_length(remaining_length, len);
	
	ptr = buffer = zmalloc(1 + remaining_count + len);

	_write_header(&ptr, header);
	_write_remaining_length(&ptr, remaining_length, remaining_count);
	_write_string(&ptr, msg->topic);
	if(msg->qos > MQTT_QOS0) {
		_write_int(&ptr, msg->id);
	}
	if(msg->payload) {
		_write_payload(&ptr, msg->payload, msg->payloadlen);
	}
	anetWrite(mqtt->fd, buffer, ptr-buffer);

	zfree(buffer);
}

//PUBLISH
int 
mqtt_publish(Mqtt *mqtt, MqttMsg *msg) {
	if(msg->id == 0) {
		msg->id = mqtt->msgid++;
	}
	_mqtt_send_publish(mqtt, msg);
	_mqtt_callback(mqtt, PUBLISH, msg, msg->id);
	return msg->id;
}

static void 
_mqtt_send_ack(Mqtt *mqtt, int type, int msgid) {
	char buffer[4] = {type, 2, MSB(msgid), LSB(msgid)};
	anetWrite(mqtt->fd, buffer, 4);
}

//PUBACK for QOS_1, QOS_2
void 
mqtt_puback(Mqtt *mqtt, int msgid) {
	_mqtt_send_ack(mqtt, PUBACK, msgid);
}

//PUBREC for QOS_2
void 
mqtt_pubrec(Mqtt *mqtt, int msgid) {
	_mqtt_send_ack(mqtt, PUBREC, msgid);
}

//PUBREL for QOS_2
void 
mqtt_pubrel(Mqtt *mqtt, int msgid) {
	_mqtt_send_ack(mqtt, PUBREL, msgid);
}

//PUBCOMP for QOS_2
void 
mqtt_pubcomp(Mqtt *mqtt, int msgid) {
	_mqtt_send_ack(mqtt, PUBCOMP, msgid);
}

static void
_mqtt_send_subscribe(Mqtt *mqtt, int msgid, const char *topic, uint8_t qos) {

	int len = 0;
	char *ptr, *buffer;

	int remaining_count;
	char remaining_length[4];

	uint8_t header = SETQOS(SUBSCRIBE, MQTT_QOS1);

	len += 2; //msgid
	len += 2 + strlen(topic) + 1; //topic and qos

	remaining_count = _encode_remaining_length(remaining_length, len);
	ptr = buffer = zmalloc(1 + remaining_count + len);
	
	_write_header(&ptr, header);
	_write_remaining_length(&ptr, remaining_length, remaining_count);
	_write_int(&ptr, msgid);
	_write_string(&ptr, topic);
	_write_char(&ptr, qos);

	anetWrite(mqtt->fd, buffer, ptr-buffer);

	zfree(buffer);
}

//SUBSCRIBE
int
mqtt_subscribe(Mqtt *mqtt, const char *topic, unsigned char qos) {
	int msgid = mqtt->msgid++;
	_mqtt_send_subscribe(mqtt, msgid, topic, qos);
	_mqtt_callback(mqtt, SUBSCRIBE, (void *)topic, msgid);
	return msgid;
}

static void 
_mqtt_send_unsubscribe(Mqtt *mqtt, int msgid, const char *topic) {
	int len = 0;
	char *ptr, *buffer;
	
	int remaining_count;
	char remaining_length[4];

	uint8_t header = SETQOS(UNSUBSCRIBE, MQTT_QOS1);
	
	len += 2; //msgid
	len += 2+strlen(topic); //topic

	remaining_count = _encode_remaining_length(remaining_length, len);
	ptr = buffer = zmalloc(1 + remaining_count + len);
	
	_write_header(&ptr, header);
	_write_remaining_length(&ptr, remaining_length, remaining_count);
	_write_int(&ptr, msgid);
	_write_string(&ptr, topic);

	anetWrite(mqtt->fd, buffer, ptr-buffer);

	zfree(buffer);
}

//UNSUBSCRIBE
int
mqtt_unsubscribe(Mqtt *mqtt, const char *topic) {
	int msgid = mqtt->msgid++;
	_mqtt_send_unsubscribe(mqtt, msgid, topic);
	_mqtt_callback(mqtt, UNSUBSCRIBE, (void *)topic, msgid);
	return msgid;
}

static void 
_mqtt_send_ping(Mqtt *mqtt) {
	char buffer[2] = {PINGREQ, 0};
	anetWrite(mqtt->fd, buffer, 2);
}

//PINGREQ
void 
mqtt_ping(Mqtt *mqtt) {
	_mqtt_send_ping(mqtt);
	_mqtt_callback(mqtt, PINGREQ, NULL, 0);
}

static void 
_mqtt_send_disconnect(Mqtt *mqtt) {
	char buffer[2] = {DISCONNECT, 0};
	anetWrite(mqtt->fd, buffer, 2);
}

//DISCONNECT
void
mqtt_disconnect(Mqtt *mqtt) {
	_mqtt_send_disconnect(mqtt);
    if(mqtt->fd > 0) {
        aeDeleteFileEvent(mqtt->el, mqtt->fd, AE_READABLE);
        close(mqtt->fd);
        mqtt->fd = -1;
    }
    mqtt_set_state(mqtt, MQTT_STATE_DISCONNECTED);
	_mqtt_callback(mqtt, CONNECT, NULL, MQTT_STATE_DISCONNECTED);
}

static void 
_mqtt_sleep(struct aeEventLoop *evtloop) {
	MQTT_NOTUSED(evtloop);
	//what to do?
}

void 
mqtt_run(Mqtt *mqtt) {
    aeSetBeforeSleepProc(mqtt->el, _mqtt_sleep);
    aeMain(mqtt->el);
    aeDeleteEventLoop(mqtt->el);
}

//RELEASE
void 
mqtt_release(Mqtt *mqtt) {
	if(mqtt->server) zfree(mqtt->server);
	if(mqtt->username) zfree((void *)mqtt->username);
	if(mqtt->password) zfree((void *)mqtt->password);
	if(mqtt->clientid) zfree((void *)mqtt->clientid);
	if(mqtt->will) mqtt_will_release(mqtt->will);
	zfree(mqtt);
}

static int 
_mqtt_keepalive(aeEventLoop *el, long long id, void *clientdata) {
	assert(el);
	MQTT_NOTUSED(id);
	Mqtt *mqtt = (Mqtt *)clientdata;
	_mqtt_send_ping(mqtt);
	_mqtt_callback(mqtt, PINGREQ, NULL, 0);
	//FIXME: TIMEOUT
    //mqtt->keepalive->timeoutid = aeCreateTimeEvent(el, 
    //   period*2, mqtt_keepalive_timeout, mqtt, NULL);
	return mqtt->keepalive*1000;
}

/*--------------------------------------
** MQTT handler and reader.
--------------------------------------*/
static void
_mqtt_handle_connack(Mqtt *mqtt, int rc) {
	_mqtt_callback(mqtt, CONNACK, NULL, rc);
	if(rc == CONNACK_ACCEPT) {
		mqtt->keepalive_timer = aeCreateTimeEvent(mqtt->el, 
			mqtt->keepalive*1000, _mqtt_keepalive, mqtt, NULL);
		mqtt_set_state(mqtt, MQTT_STATE_CONNECTED);
		_mqtt_callback(mqtt, CONNECT, NULL, MQTT_STATE_CONNECTED);
	} 
}

static void
_mqtt_handle_publish(Mqtt *mqtt, MqttMsg *msg) {
	if(msg->qos == MQTT_QOS1) {
		mqtt_puback(mqtt, msg->id);
	} else if(msg->qos == MQTT_QOS2) {
		mqtt_pubrec(mqtt, msg->id);
	}
	_mqtt_msg_callback(mqtt, msg);
	mqtt_msg_free(msg);
}

static void
_mqtt_handle_puback(Mqtt *mqtt, int type, int msgid) {
	if(type == PUBREL) {
		mqtt_pubcomp(mqtt, msgid);
	}
	_mqtt_callback(mqtt, type, NULL, msgid);
}

static void
_mqtt_handle_suback(Mqtt *mqtt, int msgid, int qos) {
	MQTT_NOTUSED(qos);
	_mqtt_callback(mqtt, SUBACK, NULL, msgid);
}

static void
_mqtt_handle_unsuback(Mqtt *mqtt, int msgid) {
	_mqtt_callback(mqtt, UNSUBACK, NULL, msgid);
}

static void
_mqtt_handle_pingresp(Mqtt *mqtt) {
	_mqtt_callback(mqtt, PINGRESP, NULL, 0);
}

static void 
_mqtt_handle_packet(Mqtt *mqtt, uint8_t header, char *buffer, int buflen) {
	int qos, msgid=0;
	bool retain, dup;
	int topiclen = 0;
	char *topic = NULL;
	char *payload = NULL;
	int payloadlen = buflen;
	MqttMsg *msg = NULL;
	uint8_t type = GETTYPE(header); 
	switch (type) {
	case CONNACK:
		_read_char(&buffer);
		_mqtt_handle_connack(mqtt, _read_char(&buffer));
		break;
	case PUBLISH:
		qos = GETQOS(header);;
		retain = GETRETAIN(header);
		dup = GETDUP(header);
		topic = _read_string_len(&buffer, &topiclen);
		payloadlen -= (2+topiclen);
		if( qos > 0) {
			msgid = _read_int(&buffer);
			payloadlen -= 2;
		}
		payload = zmalloc(payloadlen+1);
		memcpy(payload, buffer, payloadlen);
		payload[payloadlen] = '\0';
		msg = mqtt_msg_new(msgid, qos, retain, dup, topic, payloadlen, payload);
		_mqtt_handle_publish(mqtt, msg);
		break;
	case PUBACK:
	case PUBREC:
	case PUBREL:
	case PUBCOMP:
		msgid = _read_int(&buffer);
		_mqtt_handle_puback(mqtt, type, msgid);
		break;
	case SUBACK:
		msgid = _read_int(&buffer);
		qos = _read_char(&buffer);
		_mqtt_handle_suback(mqtt, msgid, qos);
		break;
	case UNSUBACK:
		msgid = _read_int(&buffer);
		_mqtt_handle_unsuback(mqtt, msgid);
		break;
	case PINGRESP:
		_mqtt_handle_pingresp(mqtt);
		break;
	default:
		_mqtt_set_error(mqtt->errstr, "badheader: %d", type);
	}
}

static void 
_mqtt_reader_feed(Mqtt *mqtt, char *buffer, int len) {
	uint8_t header;
	char *ptr = buffer;
	int remaining_length;
	int remaining_count;
	
	header = _read_header(&ptr);
	
	remaining_length = _decode_remaining_length(&ptr, &remaining_count);
	if( (1+remaining_count+remaining_length) != len ) {
		_mqtt_set_error(mqtt->errstr, "badpacket: remaing_length=%d, remaing_count=%d, len=%d",
			remaining_length, remaining_count, len);	
		return;
	}
	_mqtt_handle_packet(mqtt, header, ptr, remaining_length);
}

static void 
_mqtt_read(aeEventLoop *el, int fd, void *privdata, int mask) {
    int nread, timeout;
	Mqtt *mqtt = (Mqtt *)privdata;
	char buffer[MQTT_BUFFER_SIZE];

	MQTT_NOTUSED(mask);

    nread = read(fd, buffer, MQTT_BUFFER_SIZE);
    if (nread < 0) {
        if (errno == EAGAIN) {
            return;
        } else {
			mqtt->error = errno;
			_mqtt_set_error(mqtt->errstr, "socket error: %d.", errno);
        }
    } else if (nread == 0) {
        mqtt_disconnect(mqtt);
		timeout = (random() % 300) * 1000,
		aeCreateTimeEvent(el, timeout, _mqtt_reconnect, mqtt, NULL);
    } else {
        _mqtt_reader_feed(mqtt, buffer, nread);
    }
}

MqttWill *
mqtt_will_new(char *topic, char *msg, bool retain, uint8_t qos) {
	MqttWill *will = zmalloc(sizeof(MqttWill));
	will->topic = zstrdup(topic);
	will->msg = zstrdup(msg);
	will->retain = retain;
	will->qos = qos;
	return will;
}

void 
mqtt_will_release(MqttWill *will) {
	if(will->topic) zfree((void *)will->topic);
	if(will->msg) zfree((void *)will->msg);
	zfree(will);
}

MqttMsg *
mqtt_msg_new(int msgid, int qos, bool retain, bool dup, 
			 char *topic, int payloadlen, char *payload) {
	MqttMsg *msg = zmalloc(sizeof(MqttMsg));
	msg->id = msgid;
	msg->qos = qos;
	msg->retain = retain;
	msg->dup = dup;
	msg->topic = topic;
	msg->payloadlen = payloadlen;
	msg->payload = payload;
	return msg;
}

static const char* msg_names[] = {
	"RESERVED",
	"CONNECT",
	"CONNACK",
	"PUBLISH",
	"PUBACK",
	"PUBREC",
	"PUBREL",
	"PUBCOMP",
	"SUBSCRIBE",
	"SUBACK",
	"UNSUBSCRIBE",
	"UNSUBACK",
	"PINGREQ",
	"PINGRESP",
	"DISCONNECT"
};

const char* mqtt_msg_name(uint8_t type) {
	type = (type >> 4) & 0x0F;
	return (type >= 0 && type <= DISCONNECT) ? msg_names[type] : "UNKNOWN";
}

void 
mqtt_msg_free(MqttMsg *msg) {
	if(msg->topic) zfree((void *)msg->topic);
	if(msg->payload) zfree((void *)msg->payload);
	zfree(msg);
}

