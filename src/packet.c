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
#include <string.h>

#include "zmalloc.h"
#include "packet.h"

int 
_encode_remaining_length(char *buffer, int length) {
	char d;
	int num = 0;
	do
	{
		d = length % 128;
		length /= 128;
		if (length > 0)
			d |= 0x80;
		buffer[num++] = d;
	} while (length > 0);
	return num;
}

int 
_decode_remaining_length(char **buffer, int *count) {
	int byte; 
	int val = 0, mul = 1;
	*count = 0;
    do {
		byte = **buffer;
		val += (byte & 127) * mul;
		mul *= 128;
		(*buffer)++;
		(*count)++;
    } while ((byte & 128) != 0);
	return val;
}

/*
 * read and write header
 */
void 
_write_header(char **pptr, uint8_t header) {
	**pptr = header;
	(*pptr)++;
}

uint8_t 
_read_header(char** pptr) {
	uint8_t header = **pptr;
	(*pptr)++;
	return header;
}

/*
 * write remaining length
 */
void 
_write_remaining_length(char **pptr, char *bytes, int count) {
	memcpy(*pptr, bytes, count);
	*pptr += count;
}

/*
 * write and read char
 */
void 
_write_char(char **pptr, char c) {
	**pptr = c;
	(*pptr)++;
}

char 
_read_char(char** pptr) {
	char c = **pptr;
	(*pptr)++;
	return c;
}

/*
 * write and read int
 */
void 
_write_int(char **pptr, int i) {
	**pptr = MSB(i);
	(*pptr)++;
	**pptr = LSB(i);
	(*pptr)++;
}

int 
_read_int(char** pptr) {
	int i = 0;
	char* ptr = *pptr;
	i = 256*((uint8_t)(*ptr)) + (uint8_t)(*(ptr+1));
	*pptr += 2;
	return i;
}

/*
 * write and read string
 */
void 
_write_string(char **pptr, const char *string) {
	_write_string_len(pptr, string, strlen(string));
}

char* 
_read_string(char** pptr) {
	int len = 0;
	return _read_string_len(pptr, &len);
}

/*
 * write and read string with len
 */
void 
_write_string_len(char **pptr, const char *string, int len) {
	_write_int(pptr, len);
	memcpy(*pptr, string, len);
	*pptr += len;
}

char* 
_read_string_len(char **pptr, int *len) {
	char* string = NULL;
	*len = _read_int(pptr);
	string = zmalloc(*len+1);
	memcpy(string, *pptr, *len);
	string[*len] = '\0';
	*pptr += *len;
	return string;
}

/*
 * write payload
 */
void 
_write_payload(char **pptr, const char *payload, int length) {
	memcpy(*pptr, payload, length);
	*pptr += length;
}

