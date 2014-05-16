#ifndef _QHASH_H
#define _QHASH_H

#include <stdint.h>

#define DEFINE_HASH_ITEM_TYPE(name, data)  \
struct name {   \
	struct qhash_item *hnext;   \
	struct qhash_item *hprev;   \
	struct qhash_item *qnext;   \
	struct qhash_item *qprev;   \
	uint64_t hashcode;	\
	data    \
};	\

struct qhash;
struct qhash_item;

typedef int (*qhash_equal)(struct qhash_item*, void *);

struct qhash *qhash_init(int *errno, uint32_t max_size, float load_factor, uint16_t item_size, qhash_equal equal_func, int lru);
struct qhash_item *qhash_pool_get(struct qhash *qhash);
void qhash_pool_put(struct qhash *qhash, struct qhash_item *item);
struct qhash_item *qhash_find(struct qhash *qhash, uint64_t hashcode, void *data);
int qhash_insert(struct qhash *qhash, uint64_t hashcode, struct qhash_item *item);
struct qhash_item *qhash_delete(struct qhash *qhash, uint64_t hashcode, void *data);
void qhash_clear(struct qhash *qhash);
int qhash_free(struct qhash *qhash);
char *qhash_err_msg(int errno);

#endif
