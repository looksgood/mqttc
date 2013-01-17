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
#include <sys/wait.h>
#include <sys/socket.h>

#include "ae.h"
#include "anet.h"
#include "zmalloc.h"
#include "logger.h"
#include "packet.h"
#include "mqtt.h"

#define MAX_RETRIES 3

#define KEEPALIVE 300000

#define KEEPALIVE_TIMEOUT 600000

#define READ_BUFFER (1024*16)

static KeepAlive *keepalive_new(int period);

static void mqtt_send_connect(Mqtt *mqtt);

static void mqtt_send_publish(Mqtt *mqtt, MqttMsg *msg);

static void mqtt_send_ack(Mqtt *mqtt, int type, int msgid);

static void mqtt_send_subscribe(Mqtt *mqtt, int msgid, const char *topic, unsigned char qos);

static void mqtt_send_unsubscribe(Mqtt *mqtt, int msgid, const char *topic);

static void mqtt_send_ping(Mqtt *mqtt);

static void mqtt_send_disconnect(Mqtt *mqtt);

static void _mqtt_read(aeEventLoop *el, int fd, void *privdata, int mask);

static void mqtt_on_command(Mqtt *mqtt, int type, void *data, int id);

static int mqtt_heartbeat(aeEventLoop *el, long long id, void *clientdata);

Mqtt *mqtt_new() {
	int i;
	Mqtt *mqtt = NULL;

	mqtt = zmalloc(sizeof(Mqtt));
	mqtt->el = aeCreateEventLoop();
	mqtt->state = STATE_INIT;
	mqtt->server = NULL;
	mqtt->username = NULL;
	mqtt->password = NULL;
	mqtt->clientid = NULL;
	mqtt->port = 1883;
	mqtt->retries = 3;
	mqtt->error = 0;
	mqtt->msgid = 1;

	mqtt->keepalive = keepalive_new(60000);

	for(i = 0; i < 16; i++) {
		mqtt->callbacks[i] = NULL;
	}

	mqtt->msgcallback = NULL;
	
	return mqtt;
}

void mqtt_set_state(Mqtt *mqtt, int state) {
	mqtt->state = state;
}

void mqtt_set_clientid(Mqtt *mqtt, const char *clientid) {
	mqtt->clientid = zstrdup(clientid);
}

void mqtt_set_username(Mqtt *mqtt, const char *username) {
	mqtt->username = zstrdup(username);
}

void mqtt_set_passwd(Mqtt *mqtt, const char *passwd) {
	mqtt->password = zstrdup(passwd);
}

void mqtt_set_server(Mqtt *mqtt, const char *server) {
	mqtt->server = zstrdup(server);
}

void mqtt_set_port(Mqtt *mqtt, int port) {
	mqtt->port = port;
}

//MQTT Will
void mqtt_set_will(Mqtt *mqtt, MqttWill *will) {
	mqtt->will = will;
}

void mqtt_clear_will(Mqtt *mqtt) {
	if(mqtt->will) {
		mqtt_will_release(mqtt->will);
	}
	mqtt->will = NULL;
}

void mqtt_set_keepalive(Mqtt *mqtt, int period) {
	mqtt->keepalive->period = period;
}

//MQTT KeepAlive
static KeepAlive *keepalive_new(int period) {
	KeepAlive *ka = NULL;
	ka = zmalloc(sizeof(KeepAlive));
	ka->period = period;
	ka->timerid = 0;
	ka->timeoutid = 0;
	return ka;
}

void mqtt_set_command_callback(Mqtt *mqtt, unsigned char type, command_callback callback) {
	if(type >= 16) return;
	mqtt->callbacks[type] = callback;
}

void mqtt_clear_command_callback(Mqtt *mqtt, unsigned char type) {
	if(type >= 16) return;
	mqtt->callbacks[type] = NULL;
}

static void mqtt_on_command(Mqtt *mqtt, int type, void *data, int id) {
	command_callback cb = mqtt->callbacks[type];
	if(cb) cb(mqtt, data, id);
}

void mqtt_set_message_callback(Mqtt *mqtt, message_callback callback) {
	mqtt->msgcallback = callback;
}

void mqtt_clear_message_callback(Mqtt *mqtt) {
	mqtt->msgcallback = NULL;
}

int mqtt_connect(Mqtt *mqtt) {
    char err[1024] = {0};
    char server[1024] = {0};
    if(anetResolve(err, mqtt->server, server) != ANET_OK) {
        logger_error("MQTT", "cannot resolve %s, error: %s", mqtt->server, err);
		return -1;
    } 
    logger_debug("MQTT", "connect to %s", server);
    int fd = anetTcpConnect(err, server, mqtt->port);
    if (fd < 0) {
        logger_error("SOCKET", "failed to connect %s: %s\n", mqtt->server, err);
        return fd;
    }
    mqtt->fd = fd;
    aeCreateFileEvent(mqtt->el, fd, AE_READABLE, (aeFileProc *)_mqtt_read, (void *)mqtt); 
	mqtt_send_connect(mqtt);
    mqtt_set_state(mqtt, STATE_CONNECTING);
	mqtt_on_command(mqtt, CONNECT, NULL, STATE_CONNECTING);
	//TODO: FIXME LATER
	
	mqtt->keepalive->timerid = aeCreateTimeEvent(mqtt->el, 
		mqtt->keepalive->period, mqtt_heartbeat, mqtt, NULL);
    return fd;
}

static int 
mqtt_heartbeat(aeEventLoop *el, long long id, void *clientdata) {
	int period;
	Mqtt *mqtt = (Mqtt *)clientdata;
	period = mqtt->keepalive->period;
	mqtt_send_ping(mqtt);
	//TODO: TIMEOUT
    //mqtt->keepalive->timeoutid = aeCreateTimeEvent(el, 
    //   period*2, mqtt_keepalive_timeout, mqtt, NULL);
	return period;
}

static void mqtt_send_connect(Mqtt *mqtt) {
	int i, len = 0;
	char *ptr, *buffer=NULL;

	Header header;
	int remaining_count = 0;
	char remaining_length[4];
	ConnFlags flags;

	//header
	header.byte = 0;
	header.bits.type = CONNECT;
	header.bits.qos = QOS_1;
	
	//flags
	flags.byte = 0;
	flags.bits.cleansess = mqtt->cleansess;
	flags.bits.will = (mqtt->will) ? 1 : 0;
	if (flags.bits.will) {
		flags.bits.willqos = mqtt->will->qos;
		flags.bits.willretain = mqtt->will->retain;
	}
	if (mqtt->username) {
		flags.bits.username = 1;
	}
	if (mqtt->password) {
		flags.bits.password = 1;
	}

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
	
	remaining_count = encode_length(remaining_length, len);

	printf("len: %d, remaining_count: %d\n", len, remaining_count);
	
	ptr = buffer = zmalloc(1+remaining_count+len);
	
	*ptr++ = header.byte;
	for(i = 0; i < remaining_count; i++) {
		*ptr++ = remaining_length[i];
	}

	write_string_len(&ptr, PROTOCOL_MAGIC, 6);
	write_char(&ptr, MQTT_PROTO_MAJOR);
	write_char(&ptr, flags.byte);
	write_int(&ptr, mqtt->keepalive);
	write_string(&ptr, mqtt->clientid);

	if(mqtt->will) {
		write_string(&ptr, mqtt->will->topic);
		write_string(&ptr, mqtt->will->msg);
	}
	if (mqtt->username) {
		write_string(&ptr, mqtt->username);
	}
	if (mqtt->password) {
		write_string(&ptr, mqtt->password);
	}

	anetWrite(mqtt->fd, buffer, ptr-buffer);

	zfree(buffer);
}

int 
mqtt_reconnect(aeEventLoop *el, long long id, void *clientData)
{
    int fd;
    int timeout;
    Mqtt *mqtt = (Mqtt*)clientData;
    fd = mqtt_connect((Mqtt *)clientData);
    if(fd < 0) {
        if(mqtt->retries > MAX_RETRIES) {
            mqtt->retries = 1;
        } 
        timeout = ((2 * mqtt->retries) * 60) * 1000;
        logger_debug("MQTT", "reconnect after %d seconds", timeout/1000);
        aeCreateTimeEvent(el, timeout, mqtt_reconnect, mqtt, NULL);
        mqtt->retries++;
    } else {
        mqtt->retries = 1;
    }
    return AE_NOMORE;
}

//PUBLISH
//return msgid
int mqtt_publish(Mqtt *mqtt, MqttMsg *msg) {
	if(msg->id == 0) {
		msg->id = mqtt->msgid++;
	}
	mqtt_send_publish(mqtt, msg);
	mqtt_on_command(mqtt, PUBLISH, msg, msg->id);
	return msg->id;
}

static void mqtt_send_publish(Mqtt *mqtt, MqttMsg *msg) {
	int len = 0;
	Header header;
	char *ptr, *buffer;
	char remaining_length[4];
	int remaining_count;

	header.byte = 0;
	header.bits.type = PUBLISH;
	header.bits.retain = msg->retain;
	header.bits.qos = msg->qos;
	header.bits.dup = msg->dup;

	len += 2+strlen(msg->topic);

	if(msg->qos > QOS_0) {
		len += 2; //msgid
	}

	if(msg->payload) {
		len += msg->payloadlen;
	}
	
	remaining_count = encode_length(remaining_length, len);
	
	ptr = buffer = zmalloc(1 + remaining_count + len);

	write_header(&ptr, &header);
	write_length(&ptr, remaining_length, remaining_count);
	write_string(&ptr, msg->topic);
	if(msg->qos > QOS_0) {
		write_int(&ptr, msg->id);
	}
	if(msg->payload) {
		write_payload(&ptr, msg->payload, msg->payloadlen);
	}
	
	anetWrite(mqtt->fd, buffer, ptr-buffer);

	zfree(buffer);
}

//PUBACK for QOS_1, QOS_2
void mqtt_puback(Mqtt *mqtt, int msgid) {
	mqtt_send_ack(mqtt, PUBACK, msgid);
}

//PUBREC for QOS_2
void mqtt_pubrec(Mqtt *mqtt, int msgid) {
	mqtt_send_ack(mqtt, PUBREC, msgid);
}

//PUBREL for QOS_2
void mqtt_pubrel(Mqtt *mqtt, int msgid) {
	mqtt_send_ack(mqtt, PUBREL, msgid);
}

//PUBCOMP for QOS_2
void mqtt_pubcomp(Mqtt *mqtt, int msgid) {
	mqtt_send_ack(mqtt, PUBCOMP, msgid);
}

static void mqtt_send_ack(Mqtt *mqtt, int type, int msgid) {
	Header header;
	header.byte = 0;
	header.bits.type = type;
	char buffer[4] = {header.byte, 2, MSB(msgid), LSB(msgid)};
	anetWrite(mqtt->fd, buffer, 4);
}

//SUBSCRIBE
int mqtt_subscribe(Mqtt *mqtt, const char *topic, unsigned char qos) {
	int msgid = mqtt->msgid++;
	mqtt_send_subscribe(mqtt, msgid, topic, qos);
	mqtt_on_command(mqtt, SUBSCRIBE, (void *)topic, msgid);
	return msgid;
}

static void mqtt_send_subscribe(Mqtt *mqtt, int msgid, const char *topic, unsigned char qos) {
	int len = 0;
	Header header;
	char *ptr, *buffer;
	
	int remaining_count;
	char remaining_length[4];
	
	header.byte = 0;
	header.bits.qos = QOS_1;
	header.bits.type = SUBSCRIBE;

	len += 2; //msgid
	len += 2+strlen(topic)+1; //topic and qos

	remaining_count = encode_length(remaining_length, len);
	ptr = buffer = zmalloc(1 + remaining_count + len);
	
	write_header(&ptr, &header);
	write_length(&ptr, remaining_length, remaining_count);
	write_int(&ptr, msgid);
	write_string(&ptr, topic);
	write_char(&ptr, qos);

	anetWrite(mqtt->fd, buffer, ptr-buffer);

	zfree(buffer);
}

//UNSUBSCRIBE
int mqtt_unsubscribe(Mqtt *mqtt, const char *topic) {
	int msgid = mqtt->msgid++;
	mqtt_send_unsubscribe(mqtt, msgid, topic);
	mqtt_on_command(mqtt, UNSUBSCRIBE, (void *)topic, msgid);
	return msgid;
}

static void mqtt_send_unsubscribe(Mqtt *mqtt, int msgid, const char *topic) {
	int len = 0;
	Header header;
	char *ptr, *buffer;
	
	int remaining_count;
	char remaining_length[4];
	
	header.byte = 0;
	header.bits.qos = QOS_1;
	header.bits.type = UNSUBSCRIBE;

	len += 2; //msgid
	len += 2+strlen(topic); //topic

	remaining_count = encode_length(remaining_length, len);
	ptr = buffer = zmalloc(1 + remaining_count + len);
	
	write_header(&ptr, &header);
	write_length(&ptr, remaining_length, remaining_count);
	write_int(&ptr, msgid);
	write_string(&ptr, topic);

	anetWrite(mqtt->fd, buffer, ptr-buffer);

	zfree(buffer);
}

//PINGREQ
void mqtt_ping(Mqtt *mqtt) {
	mqtt_send_ping(mqtt);
	mqtt_on_command(mqtt, PINGREQ, NULL, 0);
}

static void mqtt_send_ping(Mqtt *mqtt) {
	Header header;
	header.byte = 0;
	header.bits.type = PINGREQ;
	char buffer[2] = {header.byte, 0};
	anetWrite(mqtt->fd, buffer, 2);
}

//DISCONNECT
void mqtt_disconnect(Mqtt *mqtt) {
	mqtt_send_disconnect(mqtt);
	
    logger_debug("MQTT", "mqtt is disconnected");
    if(mqtt->fd > 0) {
        aeDeleteFileEvent(mqtt->el, mqtt->fd, AE_READABLE);
        close(mqtt->fd);
        mqtt->fd = -1;
    }
    mqtt_set_state(mqtt, STATE_DISCONNECTED);
	mqtt_on_command(mqtt, CONNECT, NULL, STATE_DISCONNECTED);
}

static void mqtt_send_disconnect(Mqtt *mqtt) {
	Header header;
	header.byte = 0;
	header.bits.type = DISCONNECT;
	char buffer[2] = {header.byte, 0};
	anetWrite(mqtt->fd, buffer, 2);
}

static void 
_mqtt_sleep(struct aeEventLoop *eventLoop) {
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
	return;
}

/*--------------------------------------
** MQTT handler and reader.
--------------------------------------*/
static void
_mqtt_handle_connack(Mqtt *mqtt, int rc) {
	logger_info("MQTT", "connack code: %d", rc);
	if(rc == CONNACK_ACCEPT) {
		mqtt_set_state(mqtt, STATE_CONNECTED);
		mqtt_on_command(mqtt, CONNECT, NULL, STATE_CONNECTED);
	} else {
		//TODO: fixme later
		exit(1);
	}
}

static void
_mqtt_handle_publish(Mqtt *mqtt, MqttMsg *msg) {
	logger_info("MQTT", "got publish: %s", msg->topic); 	
	mqtt_on_command(mqtt, PUBLISH, msg, msg->id);
	mqtt_msg_free(msg);
}

static void
_mqtt_handle_puback(Mqtt *mqtt, int type, int msgid) {
	logger_info("MQTT", "%s: msgid=%d", _packet_name(type), msgid);
	mqtt_on_command(mqtt, type, NULL, msgid);
}

static void
_mqtt_handle_suback(Mqtt *mqtt, int msgid, int qos) {
	logger_info("MQTT", "SUBACK: msgid=%d, qos=%d", msgid, qos);
	mqtt_on_command(mqtt, SUBACK, NULL, msgid);
}

static void
_mqtt_handle_unsuback(Mqtt *mqtt, int msgid) {
	logger_info("MQTT", "UNSUBACK: msgid=%d", msgid);
	mqtt_on_command(mqtt, UNSUBACK, NULL, msgid);
}

static void
_mqtt_handle_pingresp(Mqtt *mqtt) {
	logger_info("MQTT", "PINGRESP");
	mqtt_on_command(mqtt, PINGRESP, NULL, 0);
}

static void 
_mqtt_handle_packet(Mqtt *mqtt, Header *header, char *buffer, int buflen) {
	int rc, qos, msgid=0;
	int type = header->bits.type;
	bool retain, dup;
	int topiclen = 0;
	char *topic = NULL;
	char *payload = NULL;
	int payloadlen = buflen;
	MqttMsg *msg = NULL;
	switch (type) {
	case CONNACK:
		read_char(&buffer);
		_mqtt_handle_connack(mqtt, read_char(&buffer));
		break;
	case PUBLISH:
		
		qos = header->bits.qos;
		retain = header->bits.retain;
		dup = header->bits.dup;
		logger_info("MQTT", "publish length: %d, qos: %d", buflen, qos);
		topic = read_string_len(&buffer, &topiclen);
		logger_info("MQTT", "topiclen: %d", topiclen);
		logger_info("MQTT", "topic: %s", topic);
		payloadlen -= (2+topiclen);
		if( qos > 0) {
			msgid = read_int(&buffer);
			payloadlen -= 2;
		}
		logger_info("MQTT", "payloadlen: %d", payloadlen);
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
		msgid = read_int(&buffer);
		_mqtt_handle_puback(mqtt, type, msgid);
		break;
	case SUBACK:
		msgid = read_int(&buffer);
		qos = read_char(&buffer);
		_mqtt_handle_suback(mqtt, msgid, qos);
		break;
	case UNSUBACK:
		msgid = read_int(&buffer);
		_mqtt_handle_unsuback(mqtt, msgid);
		break;
	case PINGRESP:
		_mqtt_handle_pingresp(mqtt);
		break;
	default:
		logger_error("MQTT", "error type: %d", header->bits.type);
	}
}

static void 
_mqtt_reader_feed(Mqtt *mqtt, char *buffer, int len) {
	Header header;

	char *ptr = buffer;
	int remaining_length;
	int remaining_count;
	
	header.byte = *ptr++;
	
	remaining_length = decode_length(&ptr, &remaining_count);
	if( (1+remaining_count+remaining_length) != len ) {
		logger_error("MQTT", "badpacket: remaing_length=%d, remaing_count=%d, len=%d",
			remaining_length, remaining_count, len);	
		return;
	}
	_mqtt_handle_packet(mqtt, &header, ptr, remaining_length);
}

static void 
_mqtt_read(aeEventLoop *el, int fd, void *privdata, int mask) {
    int nread, timeout;
	char buffer[READ_BUFFER];
	Mqtt *mqtt = (Mqtt *)privdata;

    nread = read(fd, buffer, READ_BUFFER);
    if (nread < 0) {
        if (errno == EAGAIN) {
            logger_warning("MQTT", "TCP EAGAIN");
            return;
        } else {
			//TODO: set error, disconnect, exit?
            //__redisSetError(c,REDIS_ERR_IO,NULL);
        }
    } else if (nread == 0) {
        logger_error("MQTT", "mqtt broker is disconnected.");
        mqtt_disconnect(mqtt);
		timeout = (random() % 120) * 1000,
		logger_info("MQTT", "reconnect after %d seconds", timeout/1000);
		aeCreateTimeEvent(el, timeout, mqtt_reconnect, mqtt, NULL);
    } else {
        logger_debug("SOCKET", "RECV: %d", nread);
		//TODO: fix later
        _mqtt_reader_feed(mqtt, buffer, nread);
    }
}

MqttWill *
mqtt_will_new(char *topic, char *msg, int retain, int qos) {
	MqttWill *will = zmalloc(sizeof(MqttWill));
	will->topic = zstrdup(topic);
	will->msg = zstrdup(msg);
	will->retain = retain;
	will->qos = qos;
	return will;
}

void 
mqtt_will_release(MqttWill *will) {
	if(will->topic) {
		zfree(will->topic);
	}
	if(will->msg) {
		zfree(will->msg);
	}
	zfree(will);
}

//-------------------
//TODO Later
//-------------------
MqttMsg *
mqtt_msg_new(int msgid, int qos, bool retain, bool dup, char *topic, int payloadlen, char *payload) {
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

void 
mqtt_msg_free(MqttMsg *msg) {
	if(msg->topic) {
		zfree(msg->topic);
	}
	if(msg->payload) {
		zfree(msg->payload);
	}
	zfree(msg);
}

