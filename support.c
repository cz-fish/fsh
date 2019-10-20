#include <stdlib.h>
#include <string.h>

#include "support.h"

/* clears the whole llist and resets pointer to it to NULL */
void llist_clear (struct llist_s **q)
{
	struct llist_s *tmp;
	while (*q != NULL) {
		tmp = *q;
		if (tmp->data != NULL && tmp->deleter != NULL) tmp->deleter(tmp->data);
		*q = (*q)->next;
		free (tmp);
	}
}

/* appends a new element at the tail of the llist
 * returns a pointer to the new last element of the llist */
struct llist_s* llist_add (struct llist_s *q, void *data, void (*deleter)(void *))
{
	struct llist_s *res;
	res = (struct llist_s*) malloc (sizeof (struct llist_s));
	if (res == NULL) {
		return q;
	}

	res->data = data;
	res->deleter = deleter;
	res->next = q;
	res->prev = NULL;
	if (res->next == NULL) {
		res->first = res;
	} else {
		res->next->prev = res;
		res->first = res->next->first;
	}
	
	return res;
}

/* inserts a new item into the llist so that the llist
 * is alphabetically sorted
 * this only works on queues of type char* */
struct llist_s* llist_insert_alphabet (struct llist_s *q, void *data, void (*deleter)(void *))
{
	struct llist_s *ni, *res;
	int b;
	
	ni = (struct llist_s*) malloc (sizeof (struct llist_s));
	if (ni == NULL) {
		return q;
	}

	ni->data = data;
	ni->deleter = deleter;

	if (q == NULL) {
		ni->next = NULL;
		ni->prev = NULL;
		ni->first = ni;
		res = ni;
	} else {
		res = q;
		b = 0;
		while (q != NULL) {
			if (strcmp((char*)data, (char*)q->data) > 0) {
				/* append ni after q */
				b = 1;
				ni->prev = q->prev;
				ni->next = q;
				ni->first = q->first;
				q->prev = ni;
				if (ni->prev != NULL) {
					ni->prev->next = ni;
				} else {
					res = ni;
				}
				break;
			}
			q = q->next;
		}
		if (b == 0) {
			/* make ni the first item on the list */
			ni->next = NULL;
			ni->prev = res->first;
			if (ni->prev != NULL) {
				ni->prev->next = ni;
			}
			res->first = ni;
		}
	}
	
	return res;
}


/* returns a pointer to the next item */
struct llist_s* llist_nextitem (struct llist_s *q)
{
	if (q == NULL) return NULL;
	return q->next;
}

/* returns a pointer to the previous item */
struct llist_s* llist_previtem (struct llist_s *q)
{
	if (q == NULL) return NULL;
	return q->prev;
}

/* removes given item from a linked list */
void llist_remove (struct llist_s **q, struct llist_s *item)
{
	if (item == NULL) return;
	if (item->prev != NULL) {
		/* not the last item of q */
		item->prev->next = item->next;
	} else {
		/* the last item of q */
		*q = item->next;
	}
	
	if (item->next != NULL) {
		/* not the first item of q */
		item->next->prev = item->prev;
	} else {
		/* the first item of q */
		if (*q != NULL)
			(*q)->first = item->prev;
	}
	
	if (item->data != NULL && item->deleter != NULL) item->deleter (item->data);
	free (item);
}

/* prints the llist by calling the supplied printer function for
 * each item */
void llist_print (struct llist_s *node, void(*printer)(int, void*))
{
	int i=0;
	if (node == NULL || printer == NULL) return;

	node = node->first;
	while (node != NULL) {
		printer (i, node->data);
		node = node->prev;
		i++;
	}
}

/* returns number of items in a llist */
int llist_size (struct llist_s *q)
{
	int c=0;
	if (q == NULL) return 0;
	q = q->first;
	while (q != NULL) {
		c++;
		q = q->prev;
	}
	return c;
}


/* --------------------------------- */

/* extracts the first element in the queue and stores its pointer in *data
 * if the queue is empty, then *data will be NULL
 * returns a pointer to the new queue */
struct queue_s* queue_extract (struct queue_s *q, void **data)
{
	struct queue_s *h;
	
	if (q == NULL) {
		*data = NULL;
		return NULL;
	}

	h = q->first;
	*data = h->data;
	q->first = h->prev;
	if (q != h) h->prev->next = NULL;
	free (h);
	if (h==q) return NULL;
	return q;
}

/* returns the first element in the queue but doesn't remove it
 * just returns the data of the head element and leaves the queue unchanged */
void* queue_peek (struct queue_s *q)
{
	if (q == NULL) return NULL;
	q = q->first;
	return q->data;
}

/* throws away the next item in queue */
void queue_throw_away (struct queue_s **q)
{
	struct queue_s *h;
	
	if (*q == NULL) {
		return;
	}

	h = (*q)->first;
	(*q)->first = h->prev;
	if (*q != h) h->prev->next = NULL;
	if (h->data != NULL && h->deleter != NULL) h->deleter (h->data);
	free (h);
	if (*q==h) *q = NULL;
	return;
}

/* acts the same as queue_throw_away, but doesn't delete the data that the container holds
 * i.e. it doesn't call deleter(data); */
void queue_pop_keep_data (struct queue_s **q)
{
	struct queue_s *h;
	
	if (*q == NULL) {
		return;
	}

	h = (*q)->first;
	(*q)->first = h->prev;
	if (*q != h) h->prev->next = NULL;
	free (h);
	if (*q==h) *q = NULL;
	return;
}

/* ------------------------------- */

/* clears the whole tree and resets pointer to it to NULL */
void tree_clear (struct tree_s **t)
{
	tree_deleter (*t);
	*t = NULL;
}

/* creates a new node structure holding given data
 * this can be used either as the root node or can be appended
 * to some other node's list of children */
struct tree_s* tree_create_node (int type, void *data, int flags, void(*deleter)(void*))
{
	struct tree_s *t;
	t = (struct tree_s*) malloc (sizeof (struct tree_s));
	if (t == NULL) return NULL;

	t->type = type;
	t->data = data;
	t->flags = flags;
	t->deleter = deleter;
	t->children = NULL;
	t->lru_child = NULL;

	return t;
}

/* appends a subnode (or subtree) child to the node pointed to by node */
int tree_add_subnode (struct tree_s *node, struct tree_s *child)
{
	if (node == NULL) return 1;
	node->children = queue_append (node->children, (void*)child, &tree_deleter);
	if (node->children == NULL) return 1;
	return 0;
}

/* returns first branch of the given node, also sets the lru_child member
 *   to the first subnode, so that the iteration may begin
 * returns NULL if there's no child */
struct tree_s* tree_get_first_subnode (struct tree_s *node)
{
	struct tree_s *res;
	if (node == NULL) return NULL;
	if (node->children == NULL) return NULL;
	res = (struct tree_s*) queue_peek (node->children);
	node->lru_child = node->children->first;
	return res;
}

/* returns a next branch of the given node and moves the lru_childe member
 * returns NULL if there are no more children */
struct tree_s* tree_get_next_subnode (struct tree_s *node)
{
	struct tree_s *res;
	if (node == NULL) return NULL;
	if (node->children == NULL) return NULL;
	if (node->lru_child == NULL) {
		res = (struct tree_s*) queue_peek (node->children);
		node->lru_child = node->children->first;
	} else {
		if (node->lru_child->prev == NULL) return NULL;
		res = (struct tree_s*) (node->lru_child->prev->data);
		node->lru_child = node->lru_child->prev;
	}
	return res;
}

/* this function will safely delete a subtree */
void tree_deleter (void *data)
{
	struct tree_s *t = (struct tree_s*) data;
	if (t == NULL) return;
	if (t->data != NULL && t->deleter != NULL) t->deleter(t->data);
	if (t->children != NULL) queue_clear(&t->children);
	free (t);
}

/* prints the tree recursively calling the supplied printer function for
 * each node */
void tree_print (struct tree_s *node, int level, char *pad, int padlen, void(*printer)(char*, int, void*))
{
	struct tree_s *sub;
	struct tree_s *t;
	char c1, c2;
	
	if (node == NULL || printer == NULL || pad == NULL) return;

	/* print current node */
	if (level == 0) {
		pad[0] = 0;
		printer (pad, node->type, node->data);
	} else if (padlen > level*2) {
		c1 = pad[(level-1)*2];
		c2 = pad[(level-1)*2+1];
		pad[(level-1)*2] = '+';
		pad[(level-1)*2+1] = '-';
		printer (pad, node->type, node->data);
		pad[(level-1)*2] = c1;
		pad[(level-1)*2+1] = c2;
	} else {
		printer (pad, node->type, node->data);
	}
	
	/* recurse subnodes */
	sub = tree_get_first_subnode (node);
	while (sub != NULL) {
		t = tree_get_next_subnode (node);
		if (padlen > level*2+2) {
			pad[level*2] = (t==NULL)?' ':'|';
			pad[level*2+1] = ' ';
			pad[level*2+2] = 0;
		}
		tree_print (sub, level+1, pad, padlen, printer);
		sub = t;
	}
}

