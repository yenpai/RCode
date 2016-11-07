#ifndef EDEVT_RAW_IO_H_
#define EDEVT_RAW_IO_H_

#include "EDLoop.h"

#define EDEvtRawInfoMagic	0xFFCC0001

typedef struct EDEvtRawInfo EDEvtRawInfo; /* Extend EDEvtInfo */

struct EDEvtRawInfo {
	EDEvtInfo self;
	int       fd;
	short     flags; /* IO Flag: Read/Write */
	void    * pData;
	void   (* cb) (EDEvt *, EDEvtRawInfo * info, short revents);
};

EDEvt * EDEvtRawCreate();

#endif
