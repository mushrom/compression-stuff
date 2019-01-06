#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
	uint8_t  symbol;
	uint64_t frequency;
} huff_symbol_t;

uint64_t count_file(const char *name, unsigned symbits, huff_symbol_t *symtab) {
	uint64_t ret = 0;
	FILE *fp = fopen(name, "r");

	if (!fp) {
		fprintf(stderr, "couldn't open \"%s\"\n", name);
		return ret;
	}

	while (!feof(fp)) {
		// TODO: bits and stuff
		uint8_t c = fgetc(fp);
		if (feof(fp)) break;

		ret++;
		symtab[c].frequency++;
		symtab[c].symbol = c;
	}

	fclose(fp);
	return ret;
}

uint64_t symbol_max_freq(huff_symbol_t *symtab, unsigned num_symbols) {
	uint64_t ret = 0;

	for (unsigned i = 0; i < num_symbols; i++) {
		if (symtab[i].symbol) {
			ret = (symtab[i].frequency > ret)? symtab[i].frequency : ret;
		}
	}

	return ret;
}

uint8_t symbol_weight(uint64_t max_freq, huff_symbol_t *symbol) {
	return (uint8_t)(0xff * (symbol->frequency / (max_freq * 1.0)));
}

void debug_symtab(unsigned symbits, huff_symbol_t *symtab, uint64_t symsum) {
	uint64_t max_freq = symbol_max_freq(symtab, symbits);

	for (unsigned i = 0; i < symbits; i++) {
		if (symtab[i].symbol) {
			printf("%02x : %lu (%u)\n",
				   symtab[i].symbol,
				   symtab[i].frequency,
				   symbol_weight(max_freq, symtab + i));
		}
	}
}

void print_symtab(unsigned symbits, huff_symbol_t *symtab, uint64_t symsum) {
	uint64_t max_freq = symbol_max_freq(symtab, symbits);

	for (unsigned i = 0; i < symbits; i++) {
		if (symtab[i].frequency) {
			uint8_t weight = symbol_weight(max_freq, symtab + i);

			fwrite(&symtab[i].symbol, sizeof(symtab[i].symbol), 1, stdout);
			fwrite(&weight, sizeof(weight), 1, stdout);
		}
	}
}

int huff_frequency_compare(const void *a, const void *b) {
	const huff_symbol_t *x = a;
	const huff_symbol_t *y = b;

	return (int32_t)x->frequency - (int32_t)y->frequency;
}

int main(int argc, char *argv[]) {
	unsigned symbols = 256;
	const char *output = "/dev/stdout";
	bool debug_output = false;

	int i = 1;

	for (; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
				case 's':
					symbols = atoi(argv[++i]);
					break;

				case 'd':
					debug_output = true;
					break;

				default:
					break;
			}
		}

		else break;
	}

	uint64_t symsum = 0;
	huff_symbol_t symtab[symbols];
	memset(symtab, 0, sizeof(symtab));

	if (i == argc) {
		symsum += count_file("/dev/stdin", symbols, symtab);

	} else {
		for (; i < argc; i++) {
			symsum += count_file(argv[i], symbols, symtab);
		}
	}

	qsort(symtab, symbols, sizeof(huff_symbol_t), huff_frequency_compare);

	if (debug_output) {
		debug_symtab(symbols, symtab, symsum);

	} else {
		print_symtab(symbols, symtab, symsum);
	}

	return 0;
}
