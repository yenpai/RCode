#include <stdlib.h>
#include "obtype.h"
#include "CLogger.h"
#include "EDLoop.h"

/*******************************************************************************/

#define SAMPLE_DBUS_NAME		"org.rcode.sample"
#define SAMPLE_DBUS_IFNAME		"sample.interface"
#define SAMPLE_DBUS_MET_DEBUG	"method_debug"
#define SAMPLE_DBUS_SIG_TEST1	"signal_test1"
#define SAMPLE_DBUS_SIG_TEST2	"signal_test2"

typedef struct {
	EDLoop * loop;
	DBusConnection * dbus_conn;
	DBusError        dbus_error;
} WorkSpace;

WorkSpace * gWS = NULL;
CLogger     gLogger;

/*******************************************************************************/

static void OnSysSignal(EDLoop * loop, int sig)
{
	switch (sig)
	{
		case SIGUSR1:
			LOG_D("EVT", "Recive System Signal [SIGUSR1][%d].", sig);
			break;

		case SIGUSR2:
			LOG_D("EVT", "Recive System Signal [SIGUSR2][%d].", sig);
			break;

		case SIGTERM:
			LOG_D("EVT", "Recive System Signal [SIGTERM][%d].", sig);
			loop->Stop(loop);
			break;
	}
}

static void OnDBusMethod_Debug(EDLoop * UNUSED(loop), DBusMessage * UNUSED(msg))
{
	LOG_D("EVT", "Recive DBus Method Call [Debug].");
}

static void OnDBusSignal_Test1(EDLoop * UNUSED(loop), DBusMessage * UNUSED(msg))
{
	LOG_D("EVT", "Recive DBus Signal [Test1].");
}

static void OnDBusSignal_Test2(EDLoop * UNUSED(loop), DBusMessage * UNUSED(msg))
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

static int InitSysSignal()
{
	int ret;
	EDLoop * loop = gWS->loop;

	if ((ret = loop->RegSysSignal(loop, SIGUSR1, OnSysSignal)) < 0)
	{
		LOG_E("INIT", "RegSysSignal SIGUSR1 Failed! ret[%d]", ret);
		return -1;
	}

	if ((ret = loop->RegSysSignal(loop, SIGUSR2, OnSysSignal)) < 0)
	{
		LOG_E("INIT", "RegSysSignal SIGUSR2 Failed! ret[%d]", ret);
		return -1;
	}

	if ((ret = loop->RegSysSignal(loop, SIGTERM, OnSysSignal)) < 0)
	{
		LOG_E("INIT", "RegSysSignal SIGTERM Failed! ret[%d]", ret);
		return -1;
	}

	LOG_V("INIT", "Init System Signal Finish");
	return 0;
}

static int InitDBusSubscribe()
{
	int ret;
	EDLoop * loop = gWS->loop;

	if ((ret = loop->RegDBusMethod(loop, SAMPLE_DBUS_IFNAME, SAMPLE_DBUS_MET_DEBUG, OnDBusMethod_Debug)) < 0)
	{
		LOG_E("INIT", "RegDBusMethod [%s][%s] Failed! ret[%d]", 
				SAMPLE_DBUS_IFNAME, SAMPLE_DBUS_MET_DEBUG, ret);
		return -1;
	}

	if ((ret = loop->RegDBusSignal(loop, SAMPLE_DBUS_IFNAME, SAMPLE_DBUS_SIG_TEST1, OnDBusSignal_Test1)) < 0)
	{
		LOG_E("INIT", "RegDBusSignal [%s][%s] Failed! ret[%d]", 
				SAMPLE_DBUS_IFNAME, SAMPLE_DBUS_SIG_TEST1, ret);
		return -1;
	}

	if ((ret = loop->RegDBusSignal(loop, SAMPLE_DBUS_IFNAME, SAMPLE_DBUS_SIG_TEST2, OnDBusSignal_Test2)) < 0)
	{
		LOG_E("INIT", "RegDBusSignal [%s][%s] Failed! ret[%d]", 
				SAMPLE_DBUS_IFNAME, SAMPLE_DBUS_SIG_TEST2, ret);
		return -1;
	}

	LOG_V("INIT", "Init DBusSubscribe Finish");
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

	if ((ret = gWS->loop->RegDBusConn(gWS->loop, *conn)) < 0)
	{
		LOG_E("INIT", "RegDBusConn into EDLoop Failed! ret[%d]", ret);
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

	if (InitSysSignal() < 0)
	{
		LOG_E("INIT", "InitSysSignal Failed!");
		exit(EXIT_FAILURE);
	}
	
	if (InitDBusConn() < 0)
	{
		LOG_E("INIT", "InitDBusConn Failed!");
		exit(EXIT_FAILURE);
	}

	if (InitDBusSubscribe() < 0)
	{
		LOG_E("INIT", "InitDBusSubscribe Failed!");
		exit(EXIT_FAILURE);
	}

	return Dispatch();
}
