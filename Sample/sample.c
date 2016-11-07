#include <stdlib.h>
#include "obtype.h"
#include "CLogger.h"
#include "EDLoop.h"
#include "EDEvtSystemSignal.h"
#include "EDEvtDBus.h"

/*******************************************************************************/

#define SAMPLE_DBUS_NAME		"org.rcode.sample"
#define SAMPLE_DBUS_IFNAME		"sample.interface"
#define SAMPLE_DBUS_MET_DEBUG	"method_debug"
#define SAMPLE_DBUS_SIG_TEST1	"signal_test1"
#define SAMPLE_DBUS_SIG_TEST2	"signal_test2"

typedef struct {
	EDLoop * loop;
	EDEvt  * evtSysSig;
	EDEvt  * evtDBus;

	DBusConnection * dbus_conn;
	DBusError        dbus_error;
} WorkSpace;

WorkSpace * gWS = NULL;
CLogger     gLogger;

/*******************************************************************************/

static void OnSysSignal(EDEvt * UNUSED(evt), EDEvtSysSigInfo * info)
{
	EDLoop * loop = info->pData;

	switch (info->sig)
	{
		case SIGUSR1:
			LOG_D("EVT", "Recive System Signal [SIGUSR1][%d].", info->sig);
			break;

		case SIGUSR2:
			LOG_D("EVT", "Recive System Signal [SIGUSR2][%d].", info->sig);
			loop->Update(loop);
			break;

		case SIGTERM:
			LOG_D("EVT", "Recive System Signal [SIGTERM][%d].", info->sig);
			loop->Stop(loop);
			break;
	}
}

static void OnDBusMethod_Debug(EDEvt * UNUSED(evt), EDEvtDBusInfo * UNUSED(info),  DBusMessage * UNUSED(msg))
{
	LOG_D("EVT", "Recive DBus Method Call [Debug].");
}

static void OnDBusSignal_Test1(EDEvt * UNUSED(evt), EDEvtDBusInfo * UNUSED(info), DBusMessage * UNUSED(msg))
{
	LOG_D("EVT", "Recive DBus Signal [Test1].");
}

static void OnDBusSignal_Test2(EDEvt * UNUSED(evt), EDEvtDBusInfo * UNUSED(info), DBusMessage * UNUSED(msg))
{
	LOG_D("EVT", "Recive DBus Signal [Test2].");
}

/*******************************************************************************/

static int Dispatch()
{
	int ret;

	LOG_I("EDD", "=== Start Event-Driven Dispatch ===");
	ret = gWS->loop->Loop(gWS->loop);
	LOG_I("EDD", "=== Leave Event-Driven Dispatch, ret[%d] ===", ret);

	return 0;
}

#define SysSigSub(s)    evt->Subscribe(evt, (EDEvtInfo *) &(EDEvtSysSigInfo) \
		{{EDEvtSysSigInfoMagic}, s, loop, OnSysSignal})
#define DBusSubM(m, c)  evt->Subscribe(evt, (EDEvtInfo *) &(EDEvtDBusInfo) \
		{{EDEvtDBusInfoMagic}, EDEVT_DBUS_METHOD, SAMPLE_DBUS_IFNAME, m, loop, c})
#define DBusSubS(s, c)  evt->Subscribe(evt, (EDEvtInfo *) &(EDEvtDBusInfo) \
		{{EDEvtDBusInfoMagic}, EDEVT_DBUS_SIGNAL, SAMPLE_DBUS_IFNAME, s, loop, c})

static int InitEvtSysSig()
{
	int ret;
	EDLoop * loop = gWS->loop;
	EDEvt  * evt  = NULL;

	if ((evt = EDEvtSysSigCreate()) == NULL)
	{
		LOG_E("INIT", "EDEvtSysSigCreate Failed!");
		return -1;
	}

	if ((ret = SysSigSub(SIGUSR1)) < 0)
	{
		LOG_E("INIT", "RegSysSignal SIGUSR1 Failed! ret[%d]", ret);
		return -1;
	}

	if ((ret = SysSigSub(SIGUSR2)) < 0)
	{
		LOG_E("INIT", "RegSysSignal SIGUSR2 Failed! ret[%d]", ret);
		return -1;
	}

	if ((ret = SysSigSub(SIGTERM)) < 0)
	{
		LOG_E("INIT", "RegSysSignal SIGTERM Failed! ret[%d]", ret);
		return -1;
	}

	if ((ret = loop->AddEvt(loop, evt)) < 0)
	{
		LOG_E("INIT", "Add EDEvtSysSig Into Loop Failed! ret[%d]", ret);
		return -1;
	}

	gWS->evtSysSig = evt;
	LOG_V("INIT", "Init System Signal Event Finish");
	return 0;
}

static int InitEvtDBus()
{
	int ret;
	EDLoop * loop = gWS->loop;
	EDEvt  * evt  = NULL;

	if ((gWS->evtDBus = EDEvtDBusCreate(gWS->dbus_conn)) == NULL)
	{
		LOG_E("INIT", "EDEvtDBusCreate Failed!");
		return -1;
	}

	evt = gWS->evtDBus;

	if ((ret = DBusSubM(SAMPLE_DBUS_MET_DEBUG, OnDBusMethod_Debug)) < 0)
	{
		LOG_E("INIT", "Subscribe DBus Method [%s] Failed! ret[%d]", 
				SAMPLE_DBUS_MET_DEBUG, ret);
		return -1;
	}

	if ((ret = DBusSubS(SAMPLE_DBUS_SIG_TEST1, OnDBusSignal_Test1)) < 0)
	{
		LOG_E("INIT", "Subscribe DBus Signal [%s] Failed! ret[%d]", 
				SAMPLE_DBUS_SIG_TEST1, ret);
		return -1;
	}

	if ((ret = DBusSubS(SAMPLE_DBUS_SIG_TEST2, OnDBusSignal_Test2)) < 0)
	{
		LOG_E("INIT", "Subscribe DBus Signal [%s] Failed! ret[%d]", 
				SAMPLE_DBUS_SIG_TEST2, ret);
		return -1;
	}

	if ((ret = loop->AddEvt(loop, evt)) < 0)
	{
		LOG_E("INIT", "Add EDEvtDBus Into Loop Failed! ret[%d]", ret);
		return -1;
	}

	LOG_V("INIT", "Init DBus Event Finish");
	return 0;
}

static int InitDBusConn()
{
	DBusConnection ** conn  = &gWS->dbus_conn;
	DBusError      *  error = &gWS->dbus_error;
	const char     *  name  = SAMPLE_DBUS_NAME;
	int               flags = DBUS_NAME_FLAG_REPLACE_EXISTING;
	int               ret;

    dbus_error_init(error);
    *conn = dbus_bus_get(DBUS_BUS_SESSION, error);

    if (dbus_error_is_set(error))
	{
		LOG_E("INIT", "dbus_bus_get Failed! error message : %s", error->message);
        dbus_error_free(error);
		return -1;
	}

	if (*conn == NULL)
	{
		LOG_E("INIT", "dbus_bus_get Failed! dbus_conn is NULL!");
		return -2;
	}

    ret = dbus_bus_request_name(*conn, name, flags, error);

    if (dbus_error_is_set(error))
	{
		LOG_E("INIT", "dbus_bus_request_name Failed! error message : %s", error->message);
        dbus_error_free(error);
		return -3;
	}

	if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
	{
		LOG_E("INIT", "dbus_bus_request_name Failed! Not Primary Owner! ret[%d]", ret);
		return -4;
	}

	LOG_V("INIT", "Init DBusConn Finish");
	LOG_V("INIT", "- DBusName[%s], UniqueName[%s]", name, dbus_bus_get_unique_name(*conn));

	return 0;
}

static int InitWorkSpace()
{
	if ((gWS = malloc(sizeof(WorkSpace))) == NULL)
	{
		LOG_E("INIT", "WorkSpace Memory Allocate Failed!");
		return -1;
	}

	memset(gWS, 0, sizeof(WorkSpace));

	if ((gWS->loop = EDLoopCreate()) == NULL)
	{
		LOG_E("INIT", "EDLoopCreate Failed!");
		return -2;
	}

	LOG_V("INIT", "Init WorkSpace Finish");
	return 0;
}

/*******************************************************************************/

int main(int UNUSED(argc), char * UNUSED(argv[]))
{
	gLogger.stream = stdout;
	gLogger.levels = CLOGGER_ALL;

	LOG_I("INIT", "=== Start Initial Process ===");

	if (InitWorkSpace() < 0)
	{
		LOG_E("INIT", "InitWorkSpace Failed!");
		exit(EXIT_FAILURE);
	}

	if (InitEvtSysSig() < 0)
	{
		LOG_E("INIT", "Init System Signal Event Failed!");
		exit(EXIT_FAILURE);
	}

	if (InitDBusConn() < 0)
	{
		LOG_E("INIT", "Init DBus Conn Failed!");
		exit(EXIT_FAILURE);
	}

	if (InitEvtDBus() < 0)
	{
		LOG_E("INIT", "Init DBus Event Failed!");
		exit(EXIT_FAILURE);
	}

	return Dispatch();
}
