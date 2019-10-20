#ifndef __SUPPORT_H__
#define __SUPPORT_H__

#ifndef NULL
#define NULL 0
#endif


enum err_scopes_e {
	ERR_LEX,     /* error in lexical analyzer */
	ERR_SYNT,    /* error in syntax analyzer */
	ERR_EXEC,    /* error during execution */
	ERR_EXPR,    /* error evaluating an arithmetic expression */
	ERR_PATTERN, /* error during pattern match */
	ERR_DBG_PRINT_TOKENS,
	ERR_DBG_PRINT_TRACES,
	ERR_DBG_PRINT_TREE,
	ERR_DBG_PRINT_GRAMMAR,
	ERR_DBG_PRINT_EXEC,
	ERR_DBG_PRINT_PATTERN,
	ERR_DBG_PRINT_GENERAL,
	ERR_NONE     /* no error */
};

/* global error handling routine
 * when it is called for the first time, it outputs an error
 *   message; successive calls to this function will be
 *   ignored until clear_last is set to 1 */
void error (int err_scope, int clear_last, char *fmt, ...);




/* ------------------------ */

struct llist_s {
	void *data;                /* data held in the queue */
	void (*deleter) (void *);  /* pointer to a function that will safely delete the data (it must know what type the data is of) */
	struct llist_s *next;      /* predecessor in the queue */
	struct llist_s *prev;      /* successor in the queue */
	struct llist_s *first;     /* pointer to the head of the queue */
};

/* clears the whole llist and resets pointer to it to NULL */
void llist_clear (struct llist_s **q);

/* appends a new element at the tail of the llist
 * returns a pointer to the new last element of the llist */
struct llist_s* llist_add (struct llist_s *q, void *data, void (*deleter)(void *));

/* inserts a new item into the llist so that the llist
 * is alphabetically sorted
 * this only works on queues of type char* */
struct llist_s* llist_insert_alphabet (struct llist_s *q, void *data, void (*deleter)(void *));

/* removes given item from a given linked list */
void llist_remove (struct llist_s **q, struct llist_s *item);

/* returns a pointer to the next item */
struct llist_s* llist_nextitem (struct llist_s *q);

/* returns a pointer to the previous item */
struct llist_s* llist_previtem (struct llist_s *q);

/* prints the llist by calling the supplied printer function for
 * each item */
void llist_print (struct llist_s *q, void(*printer)(int, void*));

/* returns number of items in a llist */
int llist_size (struct llist_s *q);

/* ------------------------- */
#define queue_s llist_s

/* clears the whole queue and resets pointer to it to NULL */
#define queue_clear(x) llist_clear(x)

/* appends a new element at the tail of the queue
 * returns a pointer to the new last element of the queue */
#define queue_append(x,y,z) llist_add(x,y,z)

/* extracts the first element in the queue and stores its pointer in *data
 * if the queue is empty, then *data will be NULL
 * returns a pointer to the new queue */
struct queue_s* queue_extract (struct queue_s *q, void **data);

/* returns the first element in the queue but doesn't remove it
 * just returns the data of the head element and leaves the queue unchanged */
void* queue_peek (struct queue_s *q);

/* throws away the next item in queue */
void queue_throw_away (struct queue_s **q);

/* acts the same as queue_throw_away, but doesn't delete the data that the container holds
 * i.e. it doesn't call deleter(data); */
void queue_pop_keep_data (struct queue_s **q);

/* prints the queue calling the supplied printer function for
 * each item */
#define queue_print(x,y) llist_print(x,y)

/* returns number of items in a queue */
#define queue_size(x) llist_size (x);

int more_input (struct queue_s **q);

/*---------------------------*/

struct tree_s {
	int type;                   /* type of the node */
	void *data;                 /* data held in the node */
	int flags;
	void (*deleter) (void *);   /* pointer to a function that will safely delete the data (it must know what type the data is of) */
	struct queue_s *children;   /* list of subnodes or children of the node */
	struct queue_s *lru_child;  /* least recently used child, this variable is used when iterating consequently through the list of subnodes */
/*	struct tree_s *parent;    *//* parent of the current node */
};

/* clears the whole tree and resets pointer to it to NULL */
void tree_clear (struct tree_s **t);

/* creates a new node structure holding given data
 * this can be used either as the root node or can be appended
 * to some other node's list of children */
struct tree_s* tree_create_node (int type, void *data, int flags, void(*deleter)(void*));

/* appends a subnode (or subtree) child to the node pointed to by node */
int tree_add_subnode (struct tree_s *node, struct tree_s *child);

/* returns first branch of the given node, also sets the lru_child member
 *   to the first subnode, so that the iteration may begin
 * returns NULL if there's no child */
struct tree_s* tree_get_first_subnode (struct tree_s *node);
/* returns a next branch of the given node and moves the lru_childe member
 * returns NULL if there are no more children */
struct tree_s* tree_get_next_subnode (struct tree_s *node);

/* this function will safely delete a subtree */
void tree_deleter(void*);

/* prints the tree recursively calling the supplied printer function for
 * each node */
void tree_print (struct tree_s *node, int level, char *pad, int padlen, void(*printer)(char*, int, void*));

#endif
