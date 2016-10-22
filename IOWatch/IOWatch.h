#ifndef IOWATCH_H_
#define IOWATCH_H_

#include "obtype.h"

/*****************************************/
/** Interface Definition                **/
/*****************************************/

typedef struct  IOWatch		    IOWatch;
typedef struct  IOWatchEv	    IOWatchEv;
typedef uint8_t IOWatchEvents;

/*****************************************/
/** Enum Definition                     **/
/*****************************************/

enum IOWatchEventType {
	IOWATCH_READ   = (1 << 0),
	IOWATCH_WRITE  = (1 << 1),
	IOWATCH_EXCEPT = (1 << 2),
};

/*****************************************/
/** Struct Definition                   **/
/*****************************************/

struct IOWatch {
	int   (* const Bind)    (IOWatch *, IOWatchEv);
	int   (* const Unbind)  (IOWatch *, int fd);
	int   (* const Watch)   (IOWatch *, int msec); 
	void  (* const Reset)   (IOWatch *); 
	void  (* const Destroy) (IOWatch *);
};

struct IOWatchEv {
	int           fd;
	IOWatchEvents events;
	IOWatchEvents revents;
	void        * pData;
	bool       (* handle) (IOWatchEv *);
};

/****************************************
 * Constructor Declaration
 ****************************************/

IOWatch * IOWatchCreate();

#endif
