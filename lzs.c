#include <hz/bitstream.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

// compile-time option to toggle the very slow but low-memory encoder
#define LZS_FAST_ENCODER 1

// constants here for testing and maybe making really embedded variations
// easier in the future
#define MAX_WINDOW_BITS 11
#define MAX_WINDOW_SIZE (1 << MAX_WINDOW_BITS)

typedef struct lzs_window {
	uint8_t *window;
	uint16_t length;
	uint16_t start;
	uint16_t end;
} lzs_window_t;

typedef struct encoder_state {
	lzs_window_t *input;
	lzs_window_t *window;

	// offset into the file
	size_t offset;

#if LZS_FAST_ENCODER
	// since we can effectively compress sequences as small as 2 bytes, we can
	// use a directly indexed hashmap of all 2 byte prefixes for window lookups
	//
	// TODO: add collision detection
	size_t hashmap[0xffff];
#endif
} encoder_t;


static inline uint16_t encoder_hash(uint8_t a, uint8_t b) {
	return ((uint16_t)b << 8) | a;
}

lzs_window_t *window_create(uint16_t size) {
	uint16_t realsize = (size && size < MAX_WINDOW_SIZE)? size : MAX_WINDOW_SIZE;
	lzs_window_t *ret = calloc(1, sizeof(lzs_window_t));

	ret->window = calloc(1, sizeof(uint8_t[realsize]));
	ret->length = realsize;
	ret->start = ret->end = 0;

	return ret;
}

static inline uint16_t window_increment(lzs_window_t *window, uint16_t thing) {
	return (thing + 1) % window->length;
}

static inline bool window_full(lzs_window_t *window) {
	return window_increment(window, window->end) == window->start;
}

static inline bool window_empty(lzs_window_t *window) {
	return window->start == window->end;
}

static inline uint16_t window_available(lzs_window_t *window) {
	if (window->start <= window->end) {
		return window->end - window->start;

	} else {
		return window->end + (window->length - window->start);
	}
}

static inline uint16_t window_index(lzs_window_t *window, uint16_t index) {
	return window->window[(window->start + index) % window->length];
}

static inline uint8_t window_peek(lzs_window_t *window) {
	return window->window[window->start];
}

static inline uint8_t window_peek_last(lzs_window_t *window) {
	return window->window[window->end - 1];
}

bool window_append(lzs_window_t *window, uint8_t value) {
	bool ret = false;

	if (window_full(window)) {
		// silently erase old value, caller needs to check availability!
		window->start = window_increment(window, window->start);
		ret = true;
	}

	window->window[window->end] = value;
	window->end = window_increment(window, window->end);

	return ret;
}

uint8_t window_remove_front(lzs_window_t *window) {
	if (window_empty(window)) {
		// TODO
		return 0;
	}

	uint8_t ret = window->window[window->start];
	window->start = window_increment(window, window->start);

	return ret;
}

bool refill_input(lzs_window_t *input, FILE *fp) {
	while (!feof(fp) && !window_full(input)) {
		uint8_t c = fgetc(fp);
		if (feof(fp))
			break;

		window_append(input, c);
	}

	return !window_empty(input);
}

typedef struct prefix_pair {
	uint16_t index;
	uint16_t length;
	bool found;
	bool end_marker;
} prefix_pair_t;

prefix_pair_t make_end_marker(void) {
	return (prefix_pair_t) {
		.index = 0,
		.length = 0,
		.found = true,
		.end_marker = true,
	};
}

static inline uint16_t prefix_length(encoder_t *state, uint16_t index) {
	uint16_t ret = 0;

	for (uint16_t i = 0;
		 i < window_available(state->input)
		 && (i + index) < window_available(state->window);
		 i++)
	{
		if (window_index(state->window, i + index) == window_index(state->input, i)) {
			ret++;

		} else {
			break;
		}
	}

	return ret;
}

#if !LZS_FAST_ENCODER
// TODO: more efficient string matching
prefix_pair_t find_prefix(encoder_t *state) {
	prefix_pair_t ret = (prefix_pair_t){
		.index = 0,
		.length = 0,
		.found = false,
		.end_marker = false,
	};

	for (uint16_t k = 0; k < window_available(state->window); k++) {
		uint16_t temp_len = 0;

		temp_len = prefix_length(state, k);
		uint16_t distance = window_available(state->window) - k;

		if (temp_len > ret.length) {
			ret.found = true;
			ret.index = distance;
			ret.length = temp_len;
		}
	}

	return ret;
}
#else /* if LZS_FAST_ENCODER */
prefix_pair_t find_prefix(encoder_t *state) {
	prefix_pair_t ret = (prefix_pair_t){
		.index = 0,
		.length = 0,
		.found = false,
		.end_marker = false,
	};

	if (window_available(state->input) <= 1) {
		return ret;
	}

	uint8_t a = window_index(state->input, 0);
	uint8_t b = window_index(state->input, 1);
	uint16_t hash = encoder_hash(a, b);

	if (!state->hashmap[hash]) {
		return ret;
	}

	uint16_t distance = state->offset - state->hashmap[hash];
	uint16_t index = window_available(state->window) - distance;
	uint16_t length = prefix_length(state, index);

	// TODO: rename `index` field to distance
	ret.found = true;
	ret.index = distance;
	ret.length = length;

	return ret;
}
#endif

void write_prefix(prefix_pair_t *prefix, huff_stream_t *out) {
	huff_stream_write(out, 1);

	if (prefix->index < 128) {
		huff_stream_write(out, 1);
		huff_stream_write_bits(out, 7, prefix->index);

	} else {
		huff_stream_write(out, 0);
		huff_stream_write_bits(out, MAX_WINDOW_BITS, prefix->index);
	}

	//huff_stream_write_bits(out, 11, prefix->length);
	if (prefix->length < 5) {
		huff_stream_write_bits(out, 2, prefix->length - 2);

	} else if (prefix->length < 8) {
		huff_stream_write_bits(out, 2, 3);
		huff_stream_write_bits(out, 2, prefix->length - 5);

	} else {
		// TODO: would it be more efficient to have a variable-length field
		//       like the index?
		//
		// NOTE: ok so probably not but definitely something to think about
		unsigned foo = (prefix->length + 7) / 15;
		unsigned bar = prefix->length - ((foo * 15) - 7);

		for (unsigned i = 0; i < foo; i++) {
			huff_stream_write_bits(out, 4, 0xf);
		}

		huff_stream_write_bits(out, 4, bar);
	}
}

void write_literal(uint8_t literal, huff_stream_t *out) {
	huff_stream_write(out, 0);
	huff_stream_write_bits(out, 8, literal);
}

// read functions assume you've already read the leading bit
prefix_pair_t read_prefix(huff_stream_t *in) {
	prefix_pair_t ret = (prefix_pair_t){
		.length = 0,
		.index = 0,
		.found = true,
		.end_marker = false,
	};

	bool is_small_offset = huff_stream_read(in);
	ret.index = huff_stream_read_bits(in, is_small_offset? 7 : MAX_WINDOW_BITS);
	ret.end_marker = ret.index == 0;

	unsigned lenbits = huff_stream_read_bits(in, 2);

	if (lenbits < 3) {
		ret.length = 2 + lenbits;

	} else {
		lenbits = huff_stream_read_bits(in, 2);

		if (lenbits < 3) {
			ret.length = 5 + lenbits;

		} else {
			unsigned c = 1;

			do {
				lenbits = huff_stream_read_bits(in, 4);
				c += lenbits == 0xf;
			} while (lenbits == 0xf);

			ret.length = ((c * 15) - 7) + lenbits;
		}
	}

	return ret;
}

uint8_t read_literal(huff_stream_t *in) {
	//return huff_stream_read_bits(in, 8)
	uint8_t ret = 0;

	for (unsigned i = 0; i < 8; i++) {
		ret |= huff_stream_read(in) << i;
	}

	return ret;
}

static inline void encoder_shift(encoder_t *state) {
#if LZS_FAST_ENCODER
	uint8_t a = window_peek(state->window);
	uint8_t x = window_peek_last(state->window);
	uint8_t y = window_peek(state->input);
	uint16_t new_hash = encoder_hash(x, y);
#endif

	bool wrap = window_append(state->window, window_remove_front(state->input));
	state->offset += 1;

#if LZS_FAST_ENCODER
	if (wrap) {
		uint8_t b = window_peek(state->window);
		uint16_t hash = encoder_hash(a, b);

		// TODO: collision detection
		state->hashmap[hash] = 0;
	}

	// new_hash is only valid if there was something in the window to hash
	if (window_available(state->window) > 1) {
		state->hashmap[new_hash] = state->offset - 2;
	}
#endif
}

void encode(FILE *fp, unsigned window_size) {
	huff_stream_t out;
	memset(&out, 0, sizeof(huff_stream_t));
	out.fp = stdout;

	encoder_t state;
	memset(&state, 0, sizeof(encoder_t));

	state.input  = window_create(window_size);
	state.window = window_create(window_size);

	while (refill_input(state.input, fp)) {
		prefix_pair_t prefix = find_prefix(&state);

		if (prefix.found && prefix.length > 1) {
			write_prefix(&prefix, &out);

			for (unsigned k = 0; k < prefix.length; k++) {
				encoder_shift(&state);
			}

		} else {
			write_literal(window_peek(state.input), &out);
			encoder_shift(&state);
		}
	}

	prefix_pair_t end = make_end_marker();
	write_prefix(&end, &out);
	huff_stream_flush(&out);
	fclose(out.fp);
}

void decode(FILE *fp) {
	huff_stream_t in;
	memset(&in, 0, sizeof(huff_stream_t));
	in.fp = fp;

	lzs_window_t *window = window_create(0);

	while (!feof(in.fp)) {
		bool is_literal = !huff_stream_read(&in);

		if (is_literal) {
			uint8_t value = read_literal(&in);
			putchar(value);
			window_append(window, value);

		} else {
			prefix_pair_t prefix = read_prefix(&in);
			uint16_t index = window_available(window) - prefix.index;

			if (prefix.end_marker) {
				break;
			}

			for (unsigned i = 0; i < prefix.length; i++) {
				putchar(window_index(window, index + i));
			}

			unsigned adjust = 0;
			for (unsigned i = 0; i < prefix.length; i++) {
				uint8_t value = window_index(window, i + index - adjust);
				adjust += window_append(window, value);
			}
		}
	}
}

static unsigned translate_compression_level(int level) {
	// compression level from 1-9, same as zip
	return 1 << (2 + level);
}

void print_help(void) {
	puts("Usage: lzs [-edh] [-c level]\n"
	     "\t-h: print this help\n"
	     "\t-e: encode input from stdin, the default if no options are given\n"
	     "\t-d: decode input from stdin\n"
	     "\t-c: specify compression level for the encoder, ranging from 1-9\n"
	     "\t    with 1 being the lowest and 9 being the highest.");
}

int main(int argc, char *argv[]) {
	unsigned window_size = 0;
	bool do_encode = true;

	for (int opt; (opt = getopt(argc, argv, "edhc:")) != -1;) {
		switch (opt) {
			case 'e':
				do_encode = true;
				break;

			case 'd':
				do_encode = false;
				break;

			case 'c':
				window_size = translate_compression_level(atoi(optarg));
				break;

			case 'h':
				print_help();
				exit(0);
				break;

			default:
				print_help();
				exit(EXIT_FAILURE);
				break;
		}
	}

	// TODO: filename

	if (do_encode) {
		encode(stdin, window_size);
	} else {
		decode(stdin);
	}

	return 0;
}
