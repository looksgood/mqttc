/* 
 * packet.c - encode and decode mqtt packet.
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

#include "anet.h"
#include "zmalloc.h"
#include "config.h"

#include "mqtt.h"
#include "packet.h"
/**
 * List of the predefined MQTT v3 packet names.
 */
static char* packet_names[] =
{
	"RESERVED", "CONNECT", "CONNACK", "PUBLISH", "PUBACK", "PUBREC", "PUBREL",
	"PUBCOMP", "SUBSCRIBE", "SUBACK", "UNSUBSCRIBE", "UNSUBACK",
	"PINGREQ", "PINGRESP", "DISCONNECT"
};

/**
 * Converts an MQTT packet code into its name
 * @param type mqtt message type
 * @return the corresponding string, or "UNKNOWN"
 */
char* mqtt_packet_name(int type)
{
	return (type >= 0 && type <= DISCONNECT) ? packet_names[type] : "UNKNOWN";
}

int encode_length(char *buf, int length) {
	int num = 0;
	do
	{
		char d = length % 128;
		length /= 128;
		/* if there are more digits to encode, set the top bit of this digit */
		if (length > 0)
			d |= 0x80;
		buf[num++] = d;
	} while (length > 0);
	return num;
}

void write_header(char **pptr, Header *header) {
	**pptr = header->byte;
	(*pptr)++;
}

void write_length(char **pptr, char *bytes, int num) {
	memcpy(*pptr, bytes, num);
	*pptr += num;
}

void write_char(char **pptr, char c) {
	**pptr = c;
	(*pptr)++;
}

void write_int(char **pptr, int i) {
	**pptr = MSB(i);
	(*pptr)++;
	**pptr = LSB(i);
	(*pptr)++;
}

void write_string(char **pptr, const char *string) {
	write_string_len(pptr, string, strlen(string));
}

void write_string_len(char **pptr, const char *string, int len) {
	write_int(pptr, len);
	memcpy(*pptr, string, len);
	*pptr += len;
}

void write_payload(char **pptr, const char *payload, int length) {
	memcpy(*pptr, payload, length);
	*pptr += length;
}

char read_char(char** pptr) {
	char c = **pptr;
	(*pptr)++;
	return c;
}

int read_int(char** pptr) {
	int i = 0;
	char* ptr = *pptr;
	i = 256*((unsigned char)(*ptr)) + (unsigned char)(*(ptr+1));
	*pptr += 2;
	return i;
}

char* read_string(char** pptr) {
	int len = 0;
	return read_string_len(pptr, &len);
}

char* read_string_len(char **pptr, int *len) {
	char* string = NULL;
	*len = read_int(pptr);
	string = zmalloc(*len+1);
	memcpy(string, *pptr, *len);
	string[*len] = '\0';
	*pptr += *len;
	return string;
}

