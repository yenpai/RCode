#ifndef EDEVT_RAW_IO_H_
#define EDEVT_RAW_IO_H_

#include "EDLoop.h"

typedef struct EDEvtRawInfo EDEvtRawInfo;

struct EDEvtRawInfo {
	int     fd;
	short   flags; /* IO Flag: Read/Write */
	void  * pData;
	void (* cb) (EDEvt *, EDEvtRawInfo * info, short revents);
};

EDEvt * EDEvtRawCreate();

#endif
