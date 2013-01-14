/* 
 * packet.h - mqtt packet encode and decode header
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

#ifndef __MQTT_PACKET_H
#define __MQTT_PACKET_H

typedef unsigned int bool;

#define PROTOCOL_MAGIC "MQIsdp"
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

typedef union _Header
{
	char byte;	/**< the whole byte */
	struct
	{
		bool retain : 1;		/**< retained flag bit */
		unsigned int qos : 2;	/**< QoS value, 0, 1 or 2 */
		bool dup : 1;			/**< DUP flag bit */
		unsigned int type : 4;	/**< message type nibble */
	} bits;
} Header;


typedef	union _ConnFlags {
	unsigned char byte;	/**< all connect flags */
	struct
	{
		int : 1;				/**< unused */
		bool cleansess : 1;	/**< cleansession flag */
		bool will : 1;			/**< will flag */
		unsigned int willqos : 2;	/**< will QoS value */
		bool willretain : 1;		/**< will retain setting */
		bool password : 1; 			/**< 3.1 password */
		bool username : 1;			/**< 3.1 user name */
	} bits;
} ConnFlags;	/**< connect flags byte */

typedef struct _Packet {
	Header header;
} MqttPacket;

/**
 * connect packet.
 */
typedef struct _ConnectPacket
{
	Header header;	/**< MQTT header byte */
	union
	{
		unsigned char byte;	/**< all connect flags */
		struct
		{
			int : 1;				/**< unused */
			bool cleansess : 1;	/**< cleansession flag */
			bool will : 1;			/**< will flag */
			unsigned int willqos : 2;	/**< will QoS value */
			bool willretain : 1;		/**< will retain setting */
			bool password : 1; 			/**< 3.1 password */
			bool username : 1;			/**< 3.1 user name */
		} bits;
	} flags;	/**< connect flags byte */

	char *protocol, /**< MQTT protocol name */
		*clientid,	/**< string client id */
        *willtopic,	/**< will topic */
        *willmsg,	/**< will payload */
		*username, 
		*password; 

	int keepalive;		/**< keepalive timeout value in seconds */
	unsigned char version;	/**< MQTT version number */
} ConnectPacket;

/**
 * CONNACK packet
 */
typedef struct _ConnAckPacket {
	Header header;
	int rc;
} ConnAckPacket;

typedef struct _SubTopic {
	char *topic;
	unsigned char qos;
} SubTopic;

/**
 * SUBACK packet.
 */
typedef struct
{
	Header header;	/**< MQTT header byte */
	int msgid;		/**< MQTT message id */
	int *qoss;		/**< list of granted QoSs */
	int num;
} SubAckPacket;

/**
 * one of the ack packets.
 */
typedef struct
{
	Header header;	/**< MQTT header byte */
	int msgid;		/**< MQTT message id */
} AckPacket;

typedef AckPacket PubAckPacket;
typedef AckPacket PubRecPacket;
typedef AckPacket PubRelPacket;
typedef AckPacket PubCompPacket;
typedef AckPacket UnsubackPacket;

int encode_length(char *buf, int length);

void write_header(char **pptr, Header *header);

void write_length(char **ptr, char *bytes, int num);

void write_string(char **pptr, const char *string);

void write_string_len(char **pptr, const char *string, int len);

void write_char(char **pptr, char c);

void write_int(char **pptr, int i);

void write_payload(char **pptr, const char *payload, int length);

char read_char(char** pptr);

int read_int(char** pptr);

char* read_string_len(char **pptr, int *len);

char* read_string(char** pptr);

char* read_string_len(char **pptr, int *len);

#endif /* __MQTT_PACKET_H */

