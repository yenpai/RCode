#ifndef EDEVT_DBUS_H_
#define EDEVT_DBUS_H_

#include "EDLoop.h"
#include "dbus/dbus.h"

#define EDEvtDBusInfoMagic	0xFFCC0003

typedef struct EDEvtDBusInfo EDEvtDBusInfo;

typedef enum {
	EDEVT_DBUS_SIGNAL,
	EDEVT_DBUS_METHOD,
} EDEVT_DBUS_TYPE;

struct EDEvtDBusInfo {
	EDEvtInfo       self;
	EDEVT_DBUS_TYPE type;
	char            ifname[64];
	char            mtname[64];
	void          * pData;
	void         (* cb) (EDEvt *, EDEvtDBusInfo * info, DBusMessage * msg);
};

EDEvt * EDEvtDBusCreate(DBusConnection *pConn);

#endif
