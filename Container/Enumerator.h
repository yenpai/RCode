#ifndef ENUMERATOR_H_
#define ENUMERATOR_H_

/****************************************
 * Interface Definition
 ****************************************/
typedef struct Enumerator Enumerator;
struct Enumerator {
	void* (*Enumerate) (Enumerator *, ...);
	void  (*Reset)     (Enumerator *);
	void  (*Destroy)   (Enumerator *);
};

/****************************************
 * Constructor Definition
 ****************************************/

#endif
