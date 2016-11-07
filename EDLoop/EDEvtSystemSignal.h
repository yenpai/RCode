#ifndef EDEVT_SYSTEM_SIGNAL_H_
#define EDEVT_SYSTEM_SIGNAL_H_

#include <signal.h>
#include "EDLoop.h"

#define EDEvtSysSigInfoMagic  0x0ABC0002ul

typedef struct EDEvtSysSigInfo EDEvtSysSigInfo;
typedef void (*EDEvtSysSigCB) (EDEvt *, EDEvtSysSigInfo *);

struct EDEvtSysSigInfo {
	EDEvtInfo     self;
	int           sig;
	void        * pData;
	EDEvtSysSigCB cb;
};

#define EDEvtSysSigInfoSet(S,P,C) \
	(EDEvtInfo *) &(EDEvtSysSigInfo) { \
		.self = {EDEvtSysSigInfoMagic}, \
		.sig  = S, .pData = P, .cb = C}

EDEvt * EDEvtSysSigCreate();

#endif
