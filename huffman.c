#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <assert.h>

#include <hz/gentable.h>
#include <hz/bitstream.h>
#include <hz/queue.h>

#define END_OF_BLOCK 0xffff

typedef struct huff_node huff_node_t;
typedef struct huff_node {
	uint16_t symbol;
	unsigned weight;

	huff_node_t *left;
	huff_node_t *right;
	huff_node_t *parent;
} huff_node_t;

typedef struct huff_tree {
	//huff_table_sym_t *symbols;
	const huff_symbol_table_t *symbols;
	/* TODO: const */ huff_node_t *nodes;
} huff_tree_t;

huff_node_t *make_huffnode(uint16_t symbol,
                           huff_node_t *left,
                           huff_node_t *right,
                           uint16_t weight)
{
	huff_node_t *ret = calloc(1, sizeof(huff_node_t));

	ret->left = left;
	ret->right = right;
	ret->symbol = symbol;
	ret->weight = weight;

	if (ret->left)  ret->left->parent = ret;
	if (ret->right) ret->right->parent = ret;

	return ret;
}

int huff_node_compare(void *a, void *b){
	huff_node_t *x = a;
	huff_node_t *y = b;

	return (int)x->weight - (int)y->weight;
}

void dump_hufftree(huff_node_t *node, unsigned indent) {
	if (node) {
		dump_hufftree(node->left, indent + 1);

		for (unsigned i = 0; i < indent; i++) putchar(' ');
		printf("- %x (%c) (%u)\n", node->symbol, node->symbol, node->weight);

		dump_hufftree(node->right, indent + 1);
	}
}

/*
huff_symbol_table_t *load_symbol_file(const char *symfile) {
	huff_symbol_table_t *ret = NULL;

	FILE *fp = fopen(symfile, "r");

	if (!fp) {
		fprintf(stderr, "couldn't open symbol file \"%s\"\n", symfile);
		return ret;
	}

	ret = calloc(1, sizeof(huff_symbol_table_t));
	fseek(fp, 0L, SEEK_END);
	size_t fsize = ftell(fp);
	fseek(fp, 0L, SEEK_SET);

	ret->length = fsize / 2;
	ret->symbols = calloc(1, sizeof(huff_sym_table_ent_t[ret->length]) + 1);

	fread(ret->symbols, fsize, 1, fp);
	fclose(fp);

	return ret;
}
*/

//huff_tree_t *open_symfile(const char *symfile) {
huff_tree_t *huff_tree_create(const huff_symbol_table_t *sym_table) {
	//huff_symbol_table_t *sym_table = load_symbol_file(symfile);

	if (!sym_table) {
		fprintf(stderr, "couldn't load symbols!\n");
	}

	queue_t *input = queue_create();
	queue_t *output = queue_create();

	// add a non-data node that signals the end of input
	huff_node_t *node = make_huffnode(END_OF_BLOCK, NULL, NULL, 0);
	queue_push_back(input, node);

	for (unsigned k = 0; k < sym_table->length; k++) {
		huff_node_t *node = make_huffnode(sym_table->symbols[k].symbol,
		                                  NULL, NULL,
		                                  sym_table->symbols[k].weight);

		queue_push_back(input, node);
	}

	while (input->items + output->items > 1) {
		huff_node_t *left = queue_pop_min(input, output, huff_node_compare);
		huff_node_t *right = queue_pop_min(input, output, huff_node_compare);

		huff_node_t *foo = make_huffnode('?', left, right,
		                                 left->weight + right->weight);

		queue_push_back(output, foo);
	}

	huff_tree_t *blarg = calloc(1, sizeof(huff_tree_t));

	blarg->symbols = sym_table;
	blarg->nodes = queue_pop_min(input, output, huff_node_compare);

	return blarg;
}

bool is_internal(huff_node_t *node) {
	return node->left || node->right;
}

bool is_leaf(huff_node_t *node) {
	return !is_internal(node);
}

bool huff_do_encode(huff_node_t *node,
                    bit_stream_t *stream,
                    uint16_t symbol,
                    uint32_t path,
                    uint32_t pathbits)
{
	if (!node) {
		return false;
	}

	if (is_leaf(node) && symbol == node->symbol) {
		for (unsigned k = pathbits; k;) {
			bool bit = !!(path & (1 << --k));
			bit_stream_write(stream, bit);
		}

		return true;
	}

	bool right = huff_do_encode(node->right, stream, symbol,
	                            (path << 1) | 1, pathbits + 1);
	bool left = right || huff_do_encode(node->left, stream, symbol,
	                                    (path << 1), pathbits + 1);

	return left || right;
}

bool huff_do_decode(huff_node_t *node, bit_stream_t *stream) {
	if (!node) {
		return true;
	}

	for (bool found = false; !found;) {
		bool dir = bit_stream_read(stream);

		node = dir? node->right : node->left;

		if (is_leaf(node)) {
			if (node->symbol == END_OF_BLOCK)
				return true;

			putchar(node->symbol);
			found = true;
		}
	}

	return false;
}

void huff_encode(huff_tree_t *tree, FILE *fp) {
	bit_stream_t stream;
	bit_stream_init_write(&stream, stdout);
	/*
	memset(&stream, 0, sizeof(stream));
	stream.fp = stdout;
	*/

	while (!feof(fp)) {
		uint16_t sym = fgetc(fp) & 0xff;
		if (feof(fp)) {
			break;
		}

		if (!huff_do_encode(tree->nodes, &stream, sym, 0, 0)) {
			fprintf(stderr, "error: can't encode %02x, no symbol!\n", sym);
		}
	}

	// XXX: why do we need two?
	huff_do_encode(tree->nodes, &stream, END_OF_BLOCK, 0, 0);
	huff_do_encode(tree->nodes, &stream, END_OF_BLOCK, 0, 0);
}

void huff_decode(huff_tree_t *tree, FILE *fp) {
	bit_stream_t stream;
	memset(&stream, 0, sizeof(stream));
	stream.fp = stdin;
	bool block_end = false;

	while (!block_end && !feof(stream.fp)) {
		block_end = huff_do_decode(tree->nodes, &stream);
	}
}

void write_signature(FILE *fp) {
	fprintf(fp, "hzpk");
}

bool check_signature(FILE *fp) {
	char sig[5];
	fgets(sig, 5, fp);

	return strcmp(sig, "hzpk") == 0;
}

int main(int argc, char *argv[]) {
	/*
	const char *symfile = "symbols.bin";
	huff_tree_t *hufftree = open_symfile(symfile);
	*/

	/*
	if (argc >= 2 && strcmp(argv[1], "-D") == 0) {
		dump_hufftree(hufftree->nodes, 0);

		for (unsigned i = 0; i < hufftree->symbols->length; i++) {
			putchar(hufftree->symbols->symbols[i].symbol);
		}
		putchar('\n');

	} else */
	if (argc >= 3 && strcmp(argv[1], "-e") == 0) {
		FILE *fp = fopen(argv[2], "r");
		assert(fp != NULL); // TODO: proper errors and stuff

		write_signature(stdout);
		huff_symbol_table_t *symtab = generate_symtab(fp);

		assert(symtab != NULL);
		write_packed_symtab(stdout, symtab);

		huff_tree_t *hufftree = huff_tree_create(symtab);
		huff_encode(hufftree, fp);

	} else if (argc >= 2 && strcmp(argv[1], "-d") == 0) {
		assert(check_signature(stdin));
		huff_symbol_table_t *symtab = read_packed_symtab(stdin);
		huff_tree_t *hufftree = huff_tree_create(symtab);

		huff_decode(hufftree, stdin);

	} else {
		puts("?");
	}
}
