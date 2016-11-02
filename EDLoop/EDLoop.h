#ifndef _EDLOOP_H_
#define _EDLOOP_H_

#define EDLOOP_SUPPORT_LOGGER
#define EDLOOP_SUPPORT_SYSSIG
//#define EDLOOP_SUPPORT_CUSSIG
//#define EDLOOP_SUPPORT_IOEVT
//#define EDLOOP_SUPPORT_DBUS

typedef struct EDLoop  EDLoop;
typedef struct EDEvent EDEvent;

#if (defined(EDLOOP_SUPPORT_SYSSIG) || defined(EDLOOP_SUPPORT_CUSSIG))
typedef void (*EDLoopSignalCB) (EDLoop *, int sig);
#endif

#ifdef EDLOOP_SUPPORT_IOEVT
typedef void (*EDLoopIOEvtCB)  (EDLoop *, int fd);
#endif

#ifdef EDLOOP_SUPPORT_DBUS
#include "dbus/dbus.h"
typedef void (*EDLoopDBusCB)   (EDLoop *, DBusMessage * msg);
#endif

struct EDLoop {

	int  (* const Loop)          (EDLoop *);
	void (* const Stop)          (EDLoop *);
	void (* const Destroy)       (EDLoop *);

#ifdef EDLOOP_SUPPORT_SYSSIG
	int  (* const RegSysSignal)  (EDLoop *, int sig, EDLoopSignalCB cb);
#endif

#ifdef EDLOOP_SUPPORT_CUSSIG
	int  (* const RegCusSignal)  (EDLoop *, int sig, EDLoopSignalCB cb);
#endif

#ifdef EDLOOP_SUPPORT_IOEVT
	int  (* const RegIOEvt)      (EDLoop *, int fd,  int events, EDLoopIOEvtCB cb);
#endif
	
#ifdef EDLOOP_SUPPORT_DBUS
	int  (* const RegDBusMethod) (EDLoop *, char * ifname, char * method, EDLoopDBusCB cb);
	int  (* const RegDBusSignal) (EDLoop *, char * ifname, char * signal, EDLoopDBusCB cb);
#endif

};

struct EDEvent {
	enum {
		EDEVT_TYPE_MIN		   = 0,
#ifdef EDLOOP_SUPPORT_SYSSIG
		EDEVT_TYPE_SYSSIG      = 1,
#endif
#ifdef EDLOOP_SUPPORT_CUSSIG
		EDEVT_TYPE_CUSSIG      = 2,
#endif
#ifdef EDLOOP_SUPPORT_IOEVT
		EDEVT_TYPE_IOEVT       = 3,
#endif
#ifdef EDLOOP_SUPPORT_DBUS
		EDEVT_TYPE_DBUS_SIGNAL = 4,
		EDEVT_TYPE_DBUS_METHOD = 5,
#endif
		EDEVT_TYPE_MAX,
	} type;

    union {
#if (defined(EDLOOP_SUPPORT_SYSSIG) || defined(EDLOOP_SUPPORT_CUSSIG))
		int  signal_id;
#endif
#ifdef EDLOOP_SUPPORT_IOEVT
		int  ioevt_fd;
#endif
#ifdef EDLOOP_SUPPORT_DBUS
		char dbus_id[2][64]; /* For DBus Interface and Signal(Method) */ 
#endif
    };

	union {
#if (defined(EDLOOP_SUPPORT_SYSSIG) || defined(EDLOOP_SUPPORT_CUSSIG))
		EDLoopSignalCB signal_cb;
#endif
#ifdef EDLOOP_SUPPORT_IOEVT
		EDLoopIOEvtCB  ioevt_cb;
#endif
#ifdef EDLOOP_SUPPORT_DBUS
		EDLoopDBusCB   dbus_cb;
#endif
	};
};

EDLoop * EDLoopCreate();

#endif
