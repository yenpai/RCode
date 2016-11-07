#ifndef EDEVT_SYSTEM_SIGNAL_H_
#define EDEVT_SYSTEM_SIGNAL_H_

#include <signal.h>
#include "EDLoop.h"

typedef struct EDEvtSysSigInfo EDEvtSysSigInfo;
typedef void (*EDEvtSysSigCB) (EDEvt *, EDEvtSysSigInfo *);

struct EDEvtSysSigInfo {
	int           sig;
	EDEvtSysSigCB cb;
	void *        pData;
};

EDEvt * EDEvtSysSigCreate();

#endif
