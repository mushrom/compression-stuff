#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <assert.h>

#define BITS(X) (sizeof(X) * 8)

typedef struct huff_sym_table_ent {
	uint8_t symbol;
	uint8_t weight;
} huff_sym_table_ent_t;

typedef struct huff_symbol_table {
	huff_sym_table_ent_t *symbols;
	size_t length;
} huff_symbol_table_t;

typedef struct huff_node huff_node_t;
typedef struct huff_node {
	uint8_t symbol;
	unsigned weight;

	huff_node_t *left;
	huff_node_t *right;
	huff_node_t *parent;
} huff_node_t;

typedef struct huff_tree {
	//huff_table_sym_t *symbols;
	huff_symbol_table_t *symbols;
	huff_node_t *nodes;
} huff_tree_t;

typedef struct huff_stream {
	FILE *fp;
	uint8_t buffer;
	unsigned index;
} huff_stream_t;

typedef struct queue_node queue_node_t;
typedef struct queue_node {
	void *data;

	queue_node_t *next;	
	queue_node_t *prev;	
} queue_node_t;

typedef struct queue {
	queue_node_t *front;
	queue_node_t *back;

	size_t items;
} queue_t;

queue_t *queue_create(void) {
	return calloc(1, sizeof(queue_t));
}

queue_node_t *queue_node_create(void *data) {
	queue_node_t *ret = calloc(1, sizeof(queue_node_t));
	ret->data = data;
	return ret;
}

void queue_push_front(queue_t *queue, void *data){
	queue_node_t *node = queue_node_create(data);

	node->next = queue->front;
	queue->front = node;
	queue->items++;

	if (!queue->back) {
		queue->back = node;

	} else {
		node->next->prev = node;
	}
}

void queue_push_back(queue_t *queue, void *data){
	queue_node_t *node = queue_node_create(data);

	node->prev = queue->back;
	queue->back = node;
	queue->items++;

	if (!queue->front) {
		queue->front = node;

	} else {
		node->prev->next = node;
	}
}

void *queue_pop_front(queue_t *queue) {
	queue_node_t *node = queue->front;

	if (!node) {
		return NULL;
	}

	queue->front = node->next;
	queue->items--;

	if (!queue->front) {
		// no more items in the queue
		assert(queue->items == 0);
		queue->back = NULL;

	} else {
		node->next->prev = NULL;
	}

	void *ret = node->data;
	free(node);
	return ret;
}

void *queue_pop_back(queue_t *queue) {
	queue_node_t *node = queue->back;

	if (!node) {
		return NULL;
	}

	queue->back = node->prev;
	queue->items--;

	if (!queue->back) {
		// no more items in the queue
		assert(queue->items == 0);
		queue->front = NULL;

	} else {
		node->prev->next = NULL;
	}

	void *ret = node->data;
	free(node);
	return ret;
}

void *queue_peek_front(queue_t *queue) {
	return queue->front;
}

void *queue_peek_back(queue_t *queue) {
	return queue->back;
}

typedef int (*queue_compare)(void *a, void *b);

void *queue_pop_min(queue_t *a, queue_t *b, queue_compare comp) {
	if (a->items == 0 && b->items == 0) {
		return NULL;
	}

	if (a->items == 0) return queue_pop_front(b);
	if (b->items == 0) return queue_pop_front(a);

	int diff = comp(queue_peek_front(a), queue_peek_front(b));

	return queue_pop_front((diff <= 0)? a : b);
}

huff_node_t *make_huffnode(uint8_t symbol,
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

huff_tree_t *open_symfile(const char *symfile) {
	huff_node_t *ret = NULL;
	huff_symbol_table_t *sym_table = load_symbol_file(symfile);

	if (!sym_table) {
		fprintf(stderr, "couldn't load symbols!\n");
	}

	queue_t *input = queue_create();
	queue_t *output = queue_create();

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

bool huff_stream_write(huff_stream_t *stream, bool bit) {
	stream->buffer |= bit << stream->index++;

	if (stream->index == BITS(stream->buffer)) {
		fwrite(&stream->buffer, 1, sizeof(stream->buffer), stream->fp);
		stream->index  = 0;
		stream->buffer = 0;
	}

	return true;
}

bool huff_stream_read(huff_stream_t *stream) {
	if (stream->index == 0) {
		fread(&stream->buffer, 1, sizeof(stream->buffer), stream->fp);
		stream->index = BITS(stream->buffer);
	}

	return !!(stream->buffer & (1 << (BITS(stream->buffer) - stream->index--)));
}

bool is_internal(huff_node_t *node) {
	return node->left || node->right;
}

bool is_leaf(huff_node_t *node) {
	return !is_internal(node);
}

bool huff_do_encode(huff_node_t *node,
                    huff_stream_t *stream,
                    uint8_t symbol,
                    uint32_t path,
                    uint32_t pathbits)
{
	if (!node) {
		return false;
	}

	if (is_leaf(node) && symbol == node->symbol) {
		for (unsigned k = pathbits; k;) {
			bool bit = !!(path & (1 << --k));
			huff_stream_write(stream, bit);
		}

		return true;
	}

	bool right = huff_do_encode(node->right, stream, symbol,
	                            (path << 1) | 1, pathbits + 1);
	bool left = right || huff_do_encode(node->left, stream, symbol,
	                                    (path << 1), pathbits + 1);

	return left || right;
}

void huff_do_decode(huff_node_t *node, huff_stream_t *stream) {
	if (!node) {
		return;
	}

	for (bool found = false; !found;) {
		bool dir = huff_stream_read(stream);

		node = dir? node->right : node->left;

		if (is_leaf(node)) {
			putchar(node->symbol);
			found = true;
		}
	}
}

void huff_encode(huff_tree_t *tree, FILE *fp) {
	huff_stream_t stream;
	memset(&stream, 0, sizeof(stream));
	stream.fp = stdout;

	while (!feof(fp)) {
		uint8_t sym = fgetc(fp);
		if (feof(fp)) break;

		if (!huff_do_encode(tree->nodes, &stream, sym, 0, 0)) {
			fprintf(stderr, "error: can't encode %02x, no symbol!\n", sym);
		}
	}
}

void huff_decode(huff_tree_t *tree, FILE *fp) {
	huff_stream_t stream;
	memset(&stream, 0, sizeof(stream));
	stream.fp = stdin;

	while (!feof(stream.fp)) {
		huff_do_decode(tree->nodes, &stream);
	}
}

int main(int argc, char *argv[]) {
	const char *symfile = "symbols.bin";
	huff_tree_t *hufftree = open_symfile(symfile);

	if (argc >= 2 && strcmp(argv[1], "-D") == 0) {
		dump_hufftree(hufftree->nodes, 0);

		for (unsigned i = 0; i < hufftree->symbols->length; i++) {
			putchar(hufftree->symbols->symbols[i].symbol);
		}
		putchar('\n');

	} else if (argc >= 2 && strcmp(argv[1], "-e") == 0) {
		huff_encode(hufftree, stdin);

	} else if (argc >= 2 && strcmp(argv[1], "-d") == 0) {
		huff_decode(hufftree, stdin);

	} else {
		puts("?");
	}
}
