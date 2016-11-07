#ifndef EDEVT_DBUS_H_
#define EDEVT_DBUS_H_

#include "EDLoop.h"
#include "dbus/dbus.h"

typedef struct EDEvtDBusInfo EDEvtDBusInfo;
typedef void (*EDEvtDBusCB)   (EDEvt *, DBusMessage * msg);

typedef enum {
	EDEVT_DBUS_SIGNAL,
	EDEVT_DBUS_METHOD,
} EDEVT_DBUS_TYPE;

struct EDEvtDBusInfo {
	EDEVT_DBUS_TYPE type;
	char ifname[64];
	char mtname[64];
	EDEvtDBusCB cb;
};

EDEvt * EDEvtDBusCreate(DBusConnection *pConn);

#endif
