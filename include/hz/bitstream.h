#pragma once
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define BITS(X) (sizeof(X) * 8)

typedef struct huff_stream {
	FILE *fp;
	uint8_t buffer;
	unsigned index;
} huff_stream_t;

static inline bool huff_stream_write(huff_stream_t *stream, bool bit) {
	stream->buffer |= bit << stream->index++;

	if (stream->index == BITS(stream->buffer)) {
		fwrite(&stream->buffer, 1, sizeof(stream->buffer), stream->fp);
		stream->index  = 0;
		stream->buffer = 0;
	}

	return true;
}

static inline
void huff_stream_write_bits(huff_stream_t *stream, unsigned bits, uint32_t x) {
	for (unsigned i = 0; i < bits; i++) {
		huff_stream_write(stream, !!(x & (1 << i)));
	}
}

static inline bool huff_stream_read(huff_stream_t *stream) {
	if (stream->index == 0) {
		fread(&stream->buffer, 1, sizeof(stream->buffer), stream->fp);
		stream->index = BITS(stream->buffer);
	}

	return !!(stream->buffer & (1 << (BITS(stream->buffer) - stream->index--)));
}

static inline
uint32_t huff_stream_read_bits(huff_stream_t *stream, unsigned bits) {
	uint32_t ret = 0;

	for (unsigned i = 0; i < bits; i++) {
		ret |= huff_stream_read(stream) << i;
	}

	return ret;
}

static inline void huff_stream_flush(huff_stream_t *stream) {
	if (stream->index > 0) {
		fwrite(&stream->buffer, 1, sizeof(stream->buffer), stream->fp);
		stream->index = 0;
		stream->buffer = 0;
	}

	fflush(stream->fp);
}
