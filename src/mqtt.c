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
#include <sys/stat.h>

#include "ae.h"
#include "anet.h"
#include "mqtt.h"
#include "logger.h"
#include "zmalloc.h"

#include "packet.h"

#define MAX_RETRIES 3

#define HEARTBEAT 120000

#define HEARTBEAT_TIMEOUT 20000

static void mqtt_send_connect(Mqtt *mqtt);

static void mqtt_send_publish(Mqtt *mqtt, MqttMsg *msg);

static void mqtt_send_ack(Mqtt *mqtt, int type, int msgid);

static void mqtt_send_disconnect(Mqtt *mqtt);

static void mqtt_read(aeEventLoop *el, int fd, void *privdata, int mask);

Mqtt *mqtt_new(aeEventLoop *el) {
	Mqtt *mqtt = NULL;
	mqtt = zmalloc(sizeof(Mqtt));
	mqtt->el = el;
	mqtt->state = STATE_INIT;
	mqtt->server = NULL;
	mqtt->username = NULL;
	mqtt->password = NULL;
	mqtt->clientid = NULL;
	mqtt->port = 1883;
	mqtt->retries = 3;
	mqtt->error = 0;
	mqtt->msgid = 1;
	mqtt->keepalive = 60;
	//mqtt->presences = listCreate();
	//mqtt->conn_callbacks = listCreate();
	//mqtt->message_callbacks = listCreate();
	
	mqtt->reader = mqtt_reader_new();
	
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
    aeCreateFileEvent(mqtt->el, fd, AE_READABLE, (aeFileProc *)mqtt_read, (void *)mqtt); 
	mqtt_send_connect(mqtt);
    mqtt_set_state(mqtt, STATE_CONNECTING);
    return fd;
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

//TODO: one callback or list?
void mqtt_add_conn_callback(Mqtt *mqtt, mqtt_conn_callback callback) {
	return;
}

//TODO: one callback or list?
void mqtt_add_message_callback(Mqtt *mqtt, mqtt_message_callback callback) {
	return;
}

void mqtt_remove_message_callback(Mqtt *mqtt, mqtt_message_callback callback) {
	return;
}

//PUBLISH
//return msgid
int mqtt_publish(Mqtt *mqtt, MqttMsg *msg) {
	if(msg->id == 0) {
		msg->id = mqtt->msgid++;
	}
	mqtt_send_publish(mqtt, msg);
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
	int len = 0;
	int msgid;
	Header header;
	char *ptr, *buffer;
	
	int remaining_count;
	char remaining_length[4];
	
	msgid = mqtt->msgid++;
	
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
	
	return msgid;
}

//UNSUBSCRIBE
int mqtt_unsubscribe(Mqtt *mqtt, const char *topic) {
	int len = 0;
	int msgid;
	Header header;
	char *ptr, *buffer;
	
	int remaining_count;
	char remaining_length[4];
	
	msgid = mqtt->msgid++;
	
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
	
	return msgid;
}

//PINGREQ
void mqtt_ping(Mqtt *mqtt) {
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

	/*
    if(mqtt->presences) {
        //listRelease(mqtt->presences);
        //mqtt->presences = listCreate();
        //listSetMatchMethod(mqtt->presences, strmatch); 
    }

    if(mqtt->message_callbacks) {
        //hash_release(mqtt->message_callbacks);
        //mqtt->message_callbacks = hash_new(8, NULL);
    }
	*/
}

static void mqtt_send_disconnect(Mqtt *mqtt) {
	Header header;
	header.byte = 0;
	header.bits.type = DISCONNECT;
	char buffer[2] = {header.byte, 0};
	anetWrite(mqtt->fd, buffer, 2);
}

MqttWill *mqtt_will_new(char *topic, char *msg, int retain, int qos) {
	MqttWill *will = zmalloc(sizeof(MqttWill));
	will->topic = zstrdup(topic);
	will->msg = zstrdup(msg);
	will->retain = retain;
	will->qos = qos;
	return will;
}

void mqtt_will_release(MqttWill *will) {
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
MqttMsg *mqtt_msg_new() {
	return NULL;
}

void mqtt_msg_free(MqttMsg *msg) {
	return;
}

/*--------------------------------------
** MQTT Protocal Reader
--------------------------------------*/
MqttReader *mqtt_reader_new() {
	MqttReader *reader = NULL;
	reader = zmalloc(sizeof(MqttReader));
	reader->err = 0;
	reader->buf = NULL;
	reader->pos = 0;
	reader->len = 0;
	reader->maxbuf = 0xFFFF;
	return reader;
}

int mqtt_reader_feed(MqttReader *r, char *buf, int len) {
    char *newbuf;

    /* Return early when this reader is in an erroneous state. */
    if (r->err)
        return MQTT_ERR;

    /* Copy the provided buffer. */
    if (buf != NULL && len >= 1) {
        /* Destroy internal buffer when it is empty and is quite large. */
        if (r->len == 0 && r->maxbuf != 0 ) { //&& sdsavail(r->buf) > r->maxbuf) {
            zfree(r->buf);
            r->buf = NULL;
            r->pos = 0;

            /* r->buf should not be NULL since we just free'd a larger one. */
            //assert(r->buf != NULL);
        }

        //newbuf = sdscatlen(r->buf,buf,len);
        if (newbuf == NULL) {
            //__redisReaderSetErrorOOM(r);
            return MQTT_ERR;
        }

        r->buf = newbuf;
        //r->len = sdslen(r->buf);
    }

    return MQTT_OK;
	
}

void mqtt_reader_free(MqttReader *reader) {
	if(reader->buf != NULL) {
		zfree(reader->buf);
	}
	zfree(reader);
}

//Internal 
static void mqtt_read(aeEventLoop *el, int fd, void *privdata, int mask) {
    int nread, timeout;
    char buf[1024*16];
    Mqtt *mqtt = (Mqtt*)privdata;

    /* Return early when the context has seen an error. */
    if (mqtt->err)
        return;

    nread = read(fd, buf, sizeof(buf));
    if (nread == -1) {
        if (errno == EAGAIN) {
            logger_warning("MQTT", "TCP EAGAIN");
            return;
        } else {
			//TODO: 
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
        if(mqtt_reader_feed(mqtt->reader, buf, nread) != MQTT_OK) {
            //__redisSetError(c,c->reader->err,c->reader->errstr);
        }
    }
}

//RELEASE
void mqtt_release(Mqtt *mqtt) {
	return;
}
