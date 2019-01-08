#pragma once
#include <stdint.h>

typedef struct huff_sym_table_ent {
	uint8_t symbol;
	uint8_t weight;
} huff_sym_table_ent_t;

typedef struct huff_symbol_table {
	uint16_t length;
	huff_sym_table_ent_t *symbols;
} huff_symbol_table_t;

//uint64_t count_file(FILE *fp, unsigned symbits, huff_symbol_t *symtab);
huff_symbol_table_t *generate_symtab(FILE *input);
huff_symbol_table_t *read_packed_symtab(FILE *fp);
void write_packed_symtab(FILE *fp, huff_symbol_table_t *table);
