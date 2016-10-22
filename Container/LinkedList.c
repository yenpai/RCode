#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "LinkedList.h"

/****************************************
 * Private Interface Definition 
 ****************************************/

typedef struct MyLinkedList MyLinkedList;
typedef struct MyEnumerator MyEnumerator;
typedef struct MyNode MyNode;

struct MyLinkedList {
	LinkedList self;
	uint32_t   count;
	MyNode   * first;
	MyNode   * last;
};

struct MyEnumerator {
	Enumerator     self;
	MyLinkedList * list;
	MyNode       * current;
	int            finished;
};

struct MyNode {
	void   *data;
	MyNode *prev;
	MyNode *next;
};

/****************************************
 * Private Function Definition
 ****************************************/

static MyNode * create_node(void *data)
{
	MyNode * node = NULL;
	node = (MyNode *) malloc(sizeof(MyNode));
	if (node != NULL)
	{
		node->data = data;
		node->next = NULL;
		node->prev = NULL;
	}

	return node;
}

static MyNode * remove_node(MyLinkedList * this, MyNode * node)
{
	MyNode *next, *prev;

	next = node->next;
	prev = node->prev;
	free(node);

	if (next)
		next->prev = prev;
	else
		this->last = prev;

	if (prev)
		prev->next = next;
	else
		this->first = next;

	if (--this->count == 0)
	{
		this->first = NULL;
		this->last = NULL;
	}

	return next;
}

/****************************************
 * Public Method Definition
 ****************************************/

static uint32_t M_GetCount(LinkedList * self)
{
	return ((MyLinkedList *) self)->count;
}

static void * M_GetFirst(LinkedList * self)
{
	MyLinkedList * this = (MyLinkedList *) self;
	if (this->count == 0)
		return NULL;
	return this->first->data;
}

static void * M_GetLast(LinkedList * self)
{
	MyLinkedList * this = (MyLinkedList *) self;
	if (this->count == 0)
		return NULL;
	return this->last->data;
}

static int M_InsertFirst(LinkedList * self, void *item)
{
	MyLinkedList * this = (MyLinkedList *) self;
	MyNode *node = NULL;

	node = create_node(item);
	if (node == NULL)
		return -1;

	if (this->count == 0)
	{
		/* first entry in list */
		this->first = node;
		this->last  = node;
	}
	else
	{
		node->next = this->first;
		this->first->prev = node;
		this->first = node;
	}
	this->count++;
	
	return 0;
}

static int M_InsertLast(LinkedList * self, void *item)
{
	MyLinkedList * this = (MyLinkedList *) self;
	MyNode *node = NULL;

	node = create_node(item);
	if (node == NULL)
		return -1;

	if (this->count == 0)
	{
		/* first entry in list */
		this->first = node;
		this->last  = node;
	}
	else
	{
		node->prev = this->last;
		this->last->next = node;
		this->last = node;
	}
	this->count++;
	
	return 0;
}

static void * M_RemoveFirst(LinkedList * self)
{
	MyLinkedList * this = (MyLinkedList *) self;
	void *item = NULL;

	item = M_GetFirst(self);
	if (item == NULL)
		return NULL;

	remove_node(this, this->first);
	return item;
}

static void * M_RemoveLast(LinkedList * self)
{
	MyLinkedList * this = (MyLinkedList *) self;
	void *item = NULL;

	item = M_GetLast(self);
	if (item == NULL)
		return NULL;

	remove_node(this, this->last);
	return item;
}

static void * M_RemoveAt(LinkedList * self, Enumerator * enumerator)
{
	MyLinkedList * this = (MyLinkedList *) self;
	MyEnumerator * e2   = (MyEnumerator *) enumerator;
	void *item = NULL;
	
	if (this != e2->list)
		return NULL;

	if (e2->current == NULL)
		return NULL;

	item = e2->current->data;

	remove_node(this, e2->current);
	return item;
}
	
static void M_RemoveAll(LinkedList * self, void (*method)(void *))
{
	MyLinkedList * this = (MyLinkedList *) self;
	void *item = NULL;

	while ((item = M_GetFirst(self)))
	{
		remove_node(this, this->first);
		if (method)
			method(item);
	}
}

static void M_Destroy(LinkedList * self)
{
	MyLinkedList * this = (MyLinkedList *) self;

	self->RemoveAll(self, NULL);
	free(this);
}

static void * M_Enumerator_Enumerate(Enumerator * self, ...)
{
	MyEnumerator * this   = (MyEnumerator *) self;
	if (this->finished)
		return NULL;

	if (this->current == NULL)
		this->current = this->list->first;
	else
		this->current = this->current->next;

	if (this->current == NULL)
	{
		this->finished = 1;
		return NULL;
	}

	return this->current->data;
}

static void M_Enumerator_Reset(Enumerator * self)
{
	MyEnumerator * this   = (MyEnumerator *) self;
	this->current = NULL;
	this->finished = 0;
}

static void M_Enumerator_Destroy(Enumerator * self)
{
	MyEnumerator * this   = (MyEnumerator *) self;
	free(this);
}

static Enumerator * M_CreateEnumerator(LinkedList * self)
{
	MyLinkedList * this = (MyLinkedList *) self;
	MyEnumerator * e1 = NULL;
	Enumerator     e2 = {
		.Enumerate = M_Enumerator_Enumerate,
		.Reset     = M_Enumerator_Reset,
		.Destroy   = M_Enumerator_Destroy,
	};

	if ((e1 = malloc(sizeof(MyEnumerator))) == NULL)
		return NULL;

	memset(e1, 0, sizeof(MyEnumerator));
	memcpy(&e1->self, &e2, sizeof(Enumerator));

	e1->list     = this;
	e1->current  = NULL;
	e1->finished = 0;

	return &e1->self;
}


/****************************************
 * Constructor Definition
 ****************************************/

LinkedList * LinkedListCreate(void)
{
	MyLinkedList * this = NULL;
	LinkedList     self = {
		.GetCount    = M_GetCount,
		.GetFirst    = M_GetFirst,
		.GetLast     = M_GetLast,
		.InsertFirst = M_InsertFirst,
		.InsertLast  = M_InsertLast,
		.RemoveFirst = M_RemoveFirst,
		.RemoveLast  = M_RemoveLast,
		.RemoveAt    = M_RemoveAt,
		.RemoveAll   = M_RemoveAll,
		.Destroy     = M_Destroy,
		.CreateEnumerator = M_CreateEnumerator,
	};

	if ((this = malloc(sizeof(MyLinkedList))) == NULL)
		return NULL;

	memset(this, 0, sizeof(MyLinkedList));
	memcpy(&this->self, &self, sizeof(LinkedList));

	/* Private Field */
	this->count = 0;
	this->first = NULL;
	this->last  = NULL;

	return &this->self;
}

