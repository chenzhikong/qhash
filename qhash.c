#include <stdlib.h>
#include <memory.h>
#include <stdint.h>

#include "qhash.h"

struct qhash
{
	uint32_t size;
	struct qhash_item **htable;
	uint32_t htsize;
	struct qhash_item *list;
	struct qhash_item *free_list;
	uint32_t lsize;
	uint16_t item_size;
	struct qhash_item *qhead;
	struct qhash_item *qtail;
	qhash_equal equal_func;
	int lru;
};

enum qhash_errno
{
	QHASH_INIT_PARAM_LOAD_FACTOR_ERROR = 1, 
	QHASH_INIT_NO_MEMORY,
};

char *qhash_err_msgs[10] =
{
        [1] = "param loadfactor x shoud be 0 < x <=1",
        [2] = "can't allocate memory"
};

struct qhash_item
{
	struct qhash_item *hnext;
	struct qhash_item *hprev;
	struct qhash_item *qnext;
	struct qhash_item *qprev;
	uint64_t hashcode;
};

static void _queue_delete(struct qhash *qhash, struct qhash_item *item)
{
	if (item->qprev)
		item->qprev->qnext = item->qnext;
	if (item->qnext)
		item->qnext->qprev = item->qprev;
	if (item == qhash->qhead)
		qhash->qhead = item->qnext;
	if (item == qhash->qtail)
		qhash->qtail = item->qprev;
	return;
}

static struct qhash_item *_queue_head_delete(struct qhash *qhash)
{
	struct qhash_item *item;
	
	item = qhash->qhead;
	qhash->qhead = item->qnext;
	if (qhash->qhead)
		qhash->qhead->qprev = NULL;
	else
		qhash->qtail = NULL;


	//item->qnext = NULL;
	return item;
}

static void _queue_tail_insert(struct qhash *qhash, struct qhash_item *item)
{
	if (qhash->qtail)
	{
		qhash->qtail->qnext = item;
		item->qprev = qhash->qtail;
		item->qnext = NULL;
		qhash->qtail = item;
	}
	else
	{
		item->qprev = item->qnext = NULL;
		qhash->qtail = qhash->qhead = item;
	}
	return;
}

static void _htable_delete(struct qhash *qhash, struct qhash_item *item)
{
	struct qhash_item *head;

	head = qhash->htable[item->hashcode % qhash->htsize];

	if (item == head)
		qhash->htable[item->hashcode % qhash->htsize] = head->hnext;
	else
		item->hprev->hnext = item->hnext;

	if (item->hnext)
		item->hnext->hprev = item->hprev;

	return;

}

static void  _pool_init(struct qhash *qhash)
{
	struct qhash_item *cur_item;
	struct qhash_item *next_item;
	int i;

	for(i = 0; i < qhash->lsize - 1; i++)
	{
		cur_item = (struct qhash_item *)((uint8_t *)qhash->list + qhash->item_size * i);
		memset((void *)cur_item, 0, qhash->item_size);
		next_item = (struct qhash_item *)((uint8_t *)qhash->list + qhash->item_size * (i + 1));
		cur_item->qnext = next_item;
	}

	cur_item = (struct qhash_item *)((uint8_t *)qhash->list + (qhash->lsize - 1) * qhash->item_size);
	memset((void *)cur_item, 0, qhash->item_size);
	cur_item->qnext = NULL;

	qhash->free_list = qhash->list;
	qhash->qhead = qhash->qtail = NULL;
	return;
}

struct qhash *qhash_init(int *errno, uint32_t max_size, float load_factor, uint16_t item_size, qhash_equal equal_func, int lru)
{
	struct qhash *qhash;
	struct qhash_item *cur_item;
	struct qhash_item *next_item;
	int i;
	
	if (load_factor <= 0 || load_factor > 1)
	{
		*errno = QHASH_INIT_PARAM_LOAD_FACTOR_ERROR;
		return NULL;
	}

	qhash = calloc(1, sizeof(struct qhash));
	if (!qhash)
	{
		*errno = QHASH_INIT_NO_MEMORY;
		return NULL;
	}

	qhash->htsize = (uint32_t)((double) max_size / load_factor);
	qhash->htable = calloc(qhash->htsize, sizeof(struct qhash_item *));
	
	if (!qhash->htable)
	{
		*errno = QHASH_INIT_NO_MEMORY;
		return NULL;
	}

	qhash->list = malloc(max_size * item_size);
	if (!qhash->list)
	{
		*errno = QHASH_INIT_NO_MEMORY;
		return NULL;
	}

	qhash->lsize = max_size;
	qhash->item_size = item_size;

	_pool_init(qhash);

	qhash->equal_func = equal_func;

	qhash->lru = lru;

	return qhash;
}
// qtail=>[]->[]->[]->[] <= qhead
struct qhash_item *qhash_pool_get(struct qhash *qhash)
{
	struct qhash_item *item;

	item = qhash->free_list;
	if (item)
		qhash->free_list = item->qnext;
	else
	{
		item = _queue_head_delete(qhash);
		_htable_delete(qhash, item);
	}
	return item;
}

void qhash_pool_put(struct qhash *qhash, struct qhash_item *item)
{
	item->qnext = qhash->free_list;
	qhash->free_list = item;
	return;
}

struct qhash_item *qhash_find(struct qhash *qhash, uint64_t hashcode, void *data)
{
	struct qhash_item *item;

	item = qhash->htable[hashcode % qhash->htsize];
	while(item)
	{
		if (qhash->equal_func(item, data))
			break;
		else
			item = item->hnext;
	}

	if (item && qhash->lru)
	{
		_queue_delete(qhash, item);
		_queue_tail_insert(qhash, item);
	}

	return item;
}

int qhash_insert(struct qhash *qhash, uint64_t hashcode, struct qhash_item *item)
{
	struct qhash_item *head;
	
	head = qhash->htable[hashcode % qhash->htsize];
	qhash->htable[hashcode % qhash->htsize] = item;
	item->hprev = NULL;
	item->hnext = head;
	item->hashcode = hashcode;
	if (head)
		head->hprev = item;

	_queue_tail_insert(qhash, item);

	return 0;
}

struct qhash_item *qhash_delete(struct qhash *qhash, uint64_t hashcode, void *data)
{
	struct qhash_item *item;

	item = qhash_find(qhash, hashcode, data);

	if (!item)
		return NULL;

	_htable_delete(qhash, item);
	_queue_delete(qhash, item);
	return item;
}


void qhash_clear(struct qhash *qhash)
{
	uint32_t i;
	
	_pool_init(qhash);
	qhash->size = 0;
	for (i = 0; i < qhash->htsize; i++)
		qhash->htable[i] = NULL;
	return;
}

int qhash_free(struct qhash *qhash)
{
	free(qhash->htable);
	free(qhash->list);
	free(qhash);
	return 0;
}

char *qhash_err_msg(int errno)
{
	return qhash_err_msgs[errno];
}
