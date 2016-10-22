#ifndef LINKED_LIST_H_
#define LINKED_LIST_H_

#include <stdint.h>
#include "Enumerator.h"

/****************************************
 * Interface Definition
 ****************************************/
typedef struct LinkedList LinkedList;

struct LinkedList {
	uint32_t (* const GetCount)    (LinkedList *);
	void *   (* const GetFirst)    (LinkedList *);
	void *   (* const GetLast)     (LinkedList *);

	int      (* const InsertFirst) (LinkedList *, void *);
	int      (* const InsertLast)  (LinkedList *, void *);
	void *   (* const RemoveFirst) (LinkedList *);
	void *   (* const RemoveLast)  (LinkedList *);
	void *   (* const RemoveAt)	   (LinkedList *, Enumerator *);
	void     (* const RemoveAll)   (LinkedList *, void (*method)(void *));
	void     (* const Destroy)     (LinkedList *);

	Enumerator * (* const CreateEnumerator) (LinkedList *);
};

/****************************************
 * Constructor Declaration
 ****************************************/
LinkedList * LinkedListCreate(void);

#endif
