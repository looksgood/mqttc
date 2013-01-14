/* 
**
** packet.c - encode and decode mqtt packet.
**
** Copyright (c) 2013  Ery Lee <ery.lee at gmail dot com>
** All rights reserved.
**
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

#include "sds.h"
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
	string = sdsnewlen(*pptr, *len);
	*pptr += *len;
	return string;
}

