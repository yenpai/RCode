#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "EDLoop.h"
#include "EDEvtRawIO.h"

typedef struct MyEDEvt MyEDEvt;
typedef EDEvtRawInfo   MyEvent;

struct MyEDEvt {
	EDEvt     self;
	MyEvent * event;
};

#ifdef EDLOOP_SUPPORT_LOGGER
#include "CLogger.h"
#define TAG "EvtSysSig"
#else
#define LOG_V(...) do {} while(0)
#define LOG_I(...) do {} while(0)
#define LOG_W(...) do {} while(0)
#define LOG_D(...) do {} while(0)
#define LOG_E(...) do {} while(0)
#endif

/**************************************************************/
/****                EDEvtRaw Public Method               *****/
/**************************************************************/

static EDRtn M_Subscribe(EDEvt * self, void * pInfo)
{
	EDEvtRawInfo * info = pInfo;
	MyEDEvt * this  = (MyEDEvt *) self;

	if (info->fd < 0)
		return EDRTN_ERROR;

	/* Create new event */
	if (this->event == NULL)
	{
		if (info->cb == NULL)
			return EDRTN_ERROR;

		if ((this->event = malloc(sizeof(MyEvent))) == NULL)
			return EDRTN_ERROR;

		memcpy(this->event, info, sizeof(MyEvent));
		return EDRTN_SUCCESS;
	}

	/* Check same fd */
	if (this->event->fd != info->fd)
		return EDRTN_ERROR;

	/* Remove event if callback was empty */
	if (info->cb == NULL)
	{
		free(this->event);
		this->event = NULL;
		return EDRTN_SUCCESS;
	}

	/* Update Only */
	this->event->cb    = info->cb;
	this->event->flags = info->flags;
	this->event->pData = info->pData;

	return EDRTN_SUCCESS;
}

static EDRtn M_Bind(EDEvt * self, struct pollfd * pfd, uint32_t * idx)
{
	MyEDEvt * this  = (MyEDEvt *) self;

	if (*idx > 0)
		return EDRTN_EVT_BIND_IGNORE;

	/* If not events need to bind */
	if (this->event == NULL)
		return EDRTN_EVT_BIND_IGNORE;

	/* Bind into current fds */
	pfd->fd      = this->event->fd;
	pfd->events  = this->event->flags;
	pfd->revents = 0;

	return EDRTN_SUCCESS;
}

static EDRtn M_Handle(EDEvt * self, struct pollfd * pfd)
{
	MyEDEvt * this  = (MyEDEvt *) self;

	if (pfd->fd != this->event->fd)
		return EDRTN_EVT_HANDLE_NOT_YET;

	if (pfd->revents & POLLERR)
		return EDRTN_ERROR;

	this->event->cb(&this->self, this->event, pfd->revents);

	return EDRTN_SUCCESS;
}

static void M_Destroy(EDEvt * self)
{
	MyEDEvt * this = (MyEDEvt *) self;

	if (this == NULL)
		return;

	if (this->event)
		free(this->event);

	free(this);
}

EDEvt * EDEvtRawCreate()
{
	MyEDEvt * this = NULL;
	EDEvt     self = {
		.Subscribe = M_Subscribe,
		.Bind      = M_Bind,
		.Handle    = M_Handle,
		.Destroy   = M_Destroy,
	};

	if ((this = malloc(sizeof(MyEDEvt))) == NULL)
		return NULL;
	memset(this, 0,     sizeof(MyEDEvt));
	memcpy(this, &self, sizeof(EDEvt));

	return &this->self;
}

