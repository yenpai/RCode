#ifndef EDEVT_SYSTEM_SIGNAL_H_
#define EDEVT_SYSTEM_SIGNAL_H_

#include <signal.h>
#include "EDLoop.h"

#define EDEvtSysSigInfoMagic   0xFFCC0002

typedef struct EDEvtSysSigInfo EDEvtSysSigInfo;

struct EDEvtSysSigInfo {
	EDEvtInfo self;
	int       sig;
	void    * pData;
	void   (* cb) (EDEvt *, EDEvtSysSigInfo *);
};

EDEvt * EDEvtSysSigCreate();

#endif
