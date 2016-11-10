#include <stdlib.h>
#include "AppCtx.h"
#include "CLogger.h"
#include "EDEvtSystemSignal.h"

/*******************************************************************************/

AppCtx * gAppCtx = NULL;
CLogger  gLogger;
#define  TAG "APP"

/*******************************************************************************/

static int ServerSetup()
{
	int ret;
	Server * server = gAppCtx->server;

	if ((ret = server->Init(server)) < 0)
	{
		LOG_E(TAG, "Init Server Failed! ret[%d]", ret);
		return -1;
	}

	if ((ret = server->Listen(server)) < 0)
	{
		LOG_E(TAG, "Start Server Failed! ret[%d]", ret);
		return -2;
	}

	return 0;
}

/*******************************************************************************/

static void OnSysSignal(EDEvt * UNUSED(evt), EDEvtSysSigInfo * info)
{
	EDLoop * loop = info->pData;

	switch (info->sig)
	{
		case SIGUSR1:
			LOG_D(TAG, "Recive System Signal [SIGUSR1][%d].", info->sig);
			break;

		case SIGUSR2:
			LOG_D(TAG, "Recive System Signal [SIGUSR2][%d].", info->sig);
			loop->Update(loop);
			break;

		case SIGTERM:
			LOG_D(TAG, "Recive System Signal [SIGTERM][%d].", info->sig);
			loop->Stop(loop);
			break;
	}
}

#define SysSigSub(sig)  evt->Subscribe(evt, EDEvtSysSigInfoSet(sig, loop, OnSysSignal))

static int SysSigSetup()
{
	int ret;
	EDLoop * loop = gAppCtx->loop;
	EDEvt  * evt  =	gAppCtx->syssig;

	if ((ret = SysSigSub(SIGUSR1)) < 0)
	{
		LOG_E(TAG, "RegSysSignal SIGUSR1 Failed! ret[%d]", ret);
		return -1;
	}

	if ((ret = SysSigSub(SIGUSR2)) < 0)
	{
		LOG_E(TAG, "RegSysSignal SIGUSR2 Failed! ret[%d]", ret);
		return -1;
	}

	if ((ret = SysSigSub(SIGTERM)) < 0)
	{
		LOG_E(TAG, "RegSysSignal SIGTERM Failed! ret[%d]", ret);
		return -1;
	}

	if ((ret = loop->AddEvt(loop, evt)) < 0)
	{
		LOG_E(TAG, "Add EDEvtSysSig Into Loop Failed! ret[%d]", ret);
		return -1;
	}

	LOG_V(TAG, "Setup system signal success.");
	return 0;
}

static int AppCtxInit()
{
	if ((gAppCtx = malloc(sizeof(AppCtx))) == NULL)
	{
		LOG_E(TAG, "AppCtx Allocate Memory Failed!");
		return -1;
	}

	memset(gAppCtx, 0, sizeof(AppCtx));

	if ((gAppCtx->loop = EDLoopCreate()) == NULL)
	{
		LOG_E(TAG, "EDLoopCreate Failed!");
		return -2;
	}

	if ((gAppCtx->syssig = EDEvtSysSigCreate()) == NULL)
	{
		LOG_E(TAG, "Create EDEvtSysSig Failed!");
		return -3;
	}

	if ((gAppCtx->server = ServerCreate(gAppCtx)) == NULL)
	{
		LOG_E(TAG, "Create Server Failed!");
		return -4;
	}

	LOG_V(TAG, "Init AppCtx success.");
	return 0;
}

static void AppCtxDestroy()
{
	if (gAppCtx == NULL)
		return;

	if (gAppCtx->server)
		gAppCtx->server->Destroy(gAppCtx->server);

	if (gAppCtx->syssig)
		gAppCtx->syssig->Destroy(gAppCtx->syssig);

	if (gAppCtx->loop)
		gAppCtx->loop->Destroy(gAppCtx->loop);
}

/*******************************************************************************/

int main(int UNUSED(argc), char * UNUSED(argv[]))
{
	int ret;
	gLogger.stream = stdout;

	LOG_I(TAG, "=== Start Initial Process ===");

	if ((ret = AppCtxInit()) < 0)
	{
		LOG_E(TAG, "Init AppCtx Failed! ret[%d]", ret);
		exit(EXIT_FAILURE);
	}

	if ((ret = SysSigSetup()) < 0)
	{
		LOG_E(TAG, "Setup EvtSySig Failed! ret[%d]", ret);
		exit(EXIT_FAILURE);
	}

	if ((ret = ServerSetup()) < 0)
	{
		LOG_E(TAG, "Setup MyServer Failed! ret[%d]", ret);
		exit(EXIT_FAILURE);
	}

	LOG_I(TAG, "=== Start Event-Driven Dispatch ===");
	ret = gAppCtx->loop->Loop(gAppCtx->loop);
	LOG_I(TAG, "=== Leave Event-Driven Dispatch, ret[%d] ===", ret);

	/* Destroy and Exit */
	AppCtxDestroy();

	return ret;
}
