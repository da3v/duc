
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>

#include "private.h"
#include "buffer.h"
#include "varint.h"


/*
 * Generic buffer manipulation for serialization
 */


struct buffer *buffer_new(void *data, size_t len)
{
	struct buffer *b;

	b = duc_malloc(sizeof(struct buffer));
	b->ptr = 0;

	if(data) {
		b->max = len;
		b->len = len;
		b->data = data;
	} else {
		b->max = 1024;
		b->len = 0;
		b->data = duc_malloc(b->max);
	}

	return b;
}


static int buffer_prep(struct buffer *b, size_t n)
{
	if(b->ptr + n > b->max) {
		while(b->len + n > b->max) {
			b->max *= 2;
		}
		b->data = duc_realloc(b->data, b->max);
	}
	return 0;
}


void buffer_free(struct buffer *b)
{
	free(b->data);
	free(b);
}


void buffer_seek(struct buffer *b, size_t off)
{
	b->ptr = off;
}


int buffer_put(struct buffer *b, const void *data, size_t len)
{
	buffer_prep(b, len); 
	memcpy(b->data + b->ptr, data, len);
	b->ptr += len;
	if(b->ptr > b->len) b->len = b->ptr;
	return len;
}


int buffer_get(struct buffer *b, void *data, size_t len)
{
	if(b->ptr <= b->len - len) {
		memcpy(data, b->data + b->ptr, len);
		b->ptr += len;
		return len;
	} else {
		return 0;
	}
}


int buffer_put_varint(struct buffer *b, uint64_t v)
{
	uint8_t buf[9];
	int l = PutVarint64(buf, v);
	buffer_put(b, buf, l);
	return l;
} 


void buffer_dump(struct buffer *b)
{
	uint8_t *p = b->data;
	size_t i;
	for(i=0; i<b->len; i++) {
		printf("%02x ", *p++);
	}
	printf("\n");
}


int buffer_get_varint(struct buffer *b, uint64_t *v)
{
	uint8_t buf[9];
	int r = buffer_get(b, buf, 1);
	if(r == 0) return 0;

	int n = 0;
	if(buf[0] >= 249) {
		n = buf[0] - 247;
	} else if(buf[0] >= 241) {
		n = 1;
	}
	if(n > 0) buffer_get(b, buf+1, n);
	int l = GetVarint64(buf, n+1, v);
	return l;
}


int buffer_put_string(struct buffer *b, const char *s)
{
	size_t len = strlen(s);
	if(len < 256) {
		uint8_t l = len;
		buffer_put(b, &l, sizeof l);
		buffer_put(b, s, l);
		return l;
	} else {
		return 0;
	}
}


int buffer_get_string(struct buffer *b, char **sout)
{
	uint8_t len;
	buffer_get(b, &len, sizeof len);
	char *s = malloc(len + 1);
	if(s) {
		buffer_get(b, s, len);
		s[len] = '\0';
		*sout = s;
		return len;
	}
	return 0;
}

/*
 * End
 */

