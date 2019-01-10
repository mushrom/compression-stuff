#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct queue_node queue_node_t;
typedef struct queue_node {
	union {
		void *data;
		uintptr_t value;
	};

	queue_node_t *next;	
	queue_node_t *prev;	
} queue_node_t;

typedef struct queue {
	queue_node_t *front;
	queue_node_t *back;

	size_t items;
} queue_t;

queue_t *queue_create(void);
queue_node_t *queue_node_create(void *data);
void queue_push_front(queue_t *queue, void *data);
void queue_push_back(queue_t *queue, void *data);
void *queue_pop_front(queue_t *queue);
void *queue_pop_back(queue_t *queue);
void *queue_peek_front(queue_t *queue);
void *queue_peek_back(queue_t *queue);
typedef int (*queue_compare)(void *a, void *b);
void *queue_pop_min(queue_t *a, queue_t *b, queue_compare comp);
