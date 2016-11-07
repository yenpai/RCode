#ifndef _EDLOOP_H_
#define _EDLOOP_H_

#define EDLOOP_SUPPORT_LOGGER

#include <stdint.h>
#include <poll.h>

typedef struct EDLoop          EDLoop;
typedef struct EDEvt           EDEvt;
typedef enum {
	EDRTN_ERROR         = -1,
	EDRTN_SUCCESS		= 0,
	EDRTN_EVT_BIND_IGNORE,
	EDRTN_EVT_BIND_NEXT,
	EDRTN_EVT_HANDLE_NOT_YET,
} EDRtn;

struct EDLoop {
	int  (* const AddEvt)        (EDLoop *, EDEvt *);
	int  (* const Loop)          (EDLoop *);
	void (* const Stop)          (EDLoop *);
	void (* const Destroy)       (EDLoop *);
};

struct EDEvt {
	EDRtn (* const Subscribe) (EDEvt *, void * pInfo);
	EDRtn (* const Bind)      (EDEvt *, struct pollfd *pfd, uint32_t *idx);
	EDRtn (* const Handle)    (EDEvt *, struct pollfd *pfd);
	void  (* const Destroy)   (EDEvt *);
};

EDLoop * EDLoopCreate();

#endif
