#pragma once
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define BITS(X) (sizeof(X) * 8)

typedef struct huff_stream {
	// TODO: implement memory streams
	// TODO: have flag for write/read mode
	FILE *fp;
	size_t available;
	size_t offset;
	uint8_t fbuffer[0x1000];
} huff_stream_t;

static inline size_t bytepos(size_t offset) {
	return offset >> 3;
}

static inline size_t bitpos(size_t offset) {
	return offset & 7;
}

static inline void bitset(uint8_t *buffer, size_t offset, bool bit) {
	uint8_t foo = buffer[bytepos(offset)];

	foo &= ~(1 << bitpos(offset));
	foo |= bit << bitpos(offset);

	buffer[bytepos(offset)] = foo;
}

static inline bool bitget(uint8_t *buffer, size_t offset) {
	return !!(buffer[bytepos(offset)] & (1 << bitpos(offset)));
}

static inline void huff_stream_init_write(huff_stream_t *stream, FILE *fp) {
	stream->available = BITS(stream->fbuffer);
	stream->offset = 0;
	stream->fp = fp;
}

static inline void huff_stream_do_write(huff_stream_t *stream) {
	if (stream->offset > 0) {
		fwrite(stream->fbuffer, 1, bytepos(stream->offset), stream->fp);
		stream->offset = 0;
	}
}

static inline void huff_stream_write(huff_stream_t *stream, bool bit) {
	bitset(stream->fbuffer, stream->offset++, bit);

	if (stream->offset == stream->available) {
		huff_stream_do_write(stream);
	}
}

static inline
void huff_stream_write_bits(huff_stream_t *stream, unsigned bits, uint32_t x) {
	for (unsigned i = 0; i < bits; i++) {
		huff_stream_write(stream, !!(x & (1 << i)));
	}
}

static inline bool huff_stream_end(huff_stream_t *stream) {
	return (stream->offset == stream->available) && feof(stream->fp);
}

static inline bool huff_stream_read(huff_stream_t *stream) {
	if (stream->offset == stream->available) {
		stream->available = 8 * fread(&stream->fbuffer, 1, sizeof(stream->fbuffer),
		                              stream->fp);
		stream->offset = 0;
	}

	if (stream->offset < stream->available) {
		return bitget(stream->fbuffer, stream->offset++);
	} else {
		return 0;
	}
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
	huff_stream_do_write(stream);
	fflush(stream->fp);
}
