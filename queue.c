#include <hz/queue.h>
#include <stdlib.h>

// TODO: remove this at some point
#include <assert.h>

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

