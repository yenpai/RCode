#ifndef EDEVT_RAW_IO_H_
#define EDEVT_RAW_IO_H_

#include "EDLoop.h"

#define EDEvtRawInfoMagic	0xFFCC0001

typedef struct EDEvtRawInfo EDEvtRawInfo; /* Extend EDEvtInfo */
typedef void (*EDEvtRawCB) (EDEvt *, EDEvtRawInfo * info, short revents);

struct EDEvtRawInfo {
	EDEvtInfo  self;
	int        fd;
	short      flags; /* IO Flag: Read/Write */
	void     * pData;
	EDEvtRawCB cb;
};

#define EDEvtRawInfoSet(FD,FL,D,C) \
	(EDEvtInfo *) &(EDEvtRawInfo) {{EDEvtRawInfoMagic}, FD, FL, D, C}

EDEvt * EDEvtRawCreate();

#endif
