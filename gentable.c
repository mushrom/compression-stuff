#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <hz/gentable.h>

typedef struct {
	uint8_t  symbol;
	uint64_t frequency;
} huff_symbol_t;

uint64_t count_file(FILE *fp, unsigned symbits, huff_symbol_t *symtab) {
	uint64_t ret = 0;

	while (!feof(fp)) {
		// TODO: bits and stuff
		uint8_t c = fgetc(fp);
		if (feof(fp)) break;

		ret++;
		symtab[c].frequency++;
		symtab[c].symbol = c;
	}

	rewind(fp);
	return ret;
}

uint64_t symbol_max_freq(huff_symbol_t *symtab, unsigned num_symbols) {
	uint64_t ret = 0;

	for (unsigned i = 0; i < num_symbols; i++) {
		if (symtab[i].frequency) {
			ret = (symtab[i].frequency > ret)? symtab[i].frequency : ret;
		}
	}

	return ret;
}

uint8_t symbol_weight(uint64_t max_freq, huff_symbol_t *symbol) {
	return (uint8_t)(0xff * (symbol->frequency / (max_freq * 1.0)));
}

void debug_symtab(huff_symbol_t *symtab, unsigned symbits, uint64_t symsum) {
	uint64_t max_freq = symbol_max_freq(symtab, symbits);

	for (unsigned i = 0; i < symbits; i++) {
		if (symtab[i].frequency) {
			printf("%02x : %lu (%u)\n",
				   symtab[i].symbol,
				   symtab[i].frequency,
				   symbol_weight(max_freq, symtab + i));
		}
	}
}

void print_symtab(huff_symbol_t *symtab, unsigned symbits, int64_t symsum) {
	uint64_t max_freq = symbol_max_freq(symtab, symbits);

	for (unsigned i = 0; i < symbits; i++) {
		if (symtab[i].frequency) {
			uint8_t weight = symbol_weight(max_freq, symtab + i);

			fwrite(&symtab[i].symbol, 1, 1, stdout);
			fwrite(&weight, 1, 1, stdout);
		}
	}
}

unsigned pack_symtab(huff_symbol_table_t *outtab,
                     huff_symbol_t *symtab,
                     unsigned symbits,
                     uint64_t symsum)
{
	uint64_t max_freq = symbol_max_freq(symtab, symbits);
	unsigned ret = 0;

	for (unsigned i = 0; i < symbits; i++) {
		if (symtab[i].frequency) {
			uint8_t weight = symbol_weight(max_freq, symtab + i);

			outtab->symbols[ret].symbol = symtab[i].symbol;
			outtab->symbols[ret].weight = weight;
			ret += 1;
		}
	}

	return ret;
}

void write_packed_symtab(FILE *fp, huff_symbol_table_t *table) {
	fwrite(&table->length, 1, 2, fp);

	for (unsigned i = 0; i < table->length; i++) {
		fwrite(&table->symbols[i].symbol, 1, 1, fp);
		fwrite(&table->symbols[i].weight, 1, 1, fp);
	}
}

huff_symbol_table_t *read_packed_symtab(FILE *fp) {
	// TODO: leaving this here in case symbol size is ever configurable
	//       (will it ever be? seems kinda silly tbh)
	unsigned symbols = 256;

	huff_symbol_table_t *ret = calloc(1, sizeof(huff_symbol_table_t));
	ret->symbols = calloc(1, sizeof(huff_sym_table_ent_t[symbols]));

	fread(&ret->length, 1, 2, fp);

	for (unsigned i = 0; i < ret->length; i++) {
		fread(&ret->symbols[i].symbol, 1, 1, fp);
		fread(&ret->symbols[i].weight, 1, 1, fp);
	}

	return ret;
}

int huff_frequency_compare(const void *a, const void *b) {
	const huff_symbol_t *x = a;
	const huff_symbol_t *y = b;

	return (int32_t)x->frequency - (int32_t)y->frequency;
}

huff_symbol_table_t *generate_symtab(FILE *input){
	unsigned symbols = 256;

	huff_symbol_table_t *ret = calloc(1, sizeof(huff_symbol_table_t));
	huff_symbol_t symtab[symbols];
	memset(symtab, 0, sizeof(symtab));

	uint64_t symsum = count_file(input, symbols, symtab);
	qsort(symtab, symbols, sizeof(huff_symbol_t), huff_frequency_compare);

	ret->symbols = calloc(1, sizeof(huff_sym_table_ent_t[symbols]));
	ret->length  = pack_symtab(ret, symtab, symbols, symsum);

	return ret;
}
