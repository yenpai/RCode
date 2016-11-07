#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/signalfd.h>
#include "LinkedList.h"
#include "EDLoop.h"
#include "EDEvtSystemSignal.h"

typedef struct MyEDEvt  MyEDEvt;
typedef EDEvtSysSigInfo MyEvent;

struct MyEDEvt {
	EDEvt        self;
	int          fd;
	sigset_t     mask;
	LinkedList * events; /* MyEvent */
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
/****                SYSSIG Private Method                *****/
/**************************************************************/
static MyEvent * syssig_find_event(LinkedList * events, int sig)
{
	Enumerator * enumerator;
	MyEvent    * event = NULL;

	enumerator = events->CreateEnumerator(events);
	while ((event = enumerator->Enumerate(enumerator)))
	{
		if (event->sig == sig)
			break;
	}
	enumerator->Destroy(enumerator);

	return event;
}

static int syssig_remove_event(LinkedList * events, int sig)
{
	Enumerator * enumerator;
	MyEvent    * event = NULL;

	enumerator = events->CreateEnumerator(events);
	while ((event = enumerator->Enumerate(enumerator)))
	{
		if (event->sig == sig)
		{
			events->RemoveAt(events, enumerator);
			enumerator->Destroy(enumerator);
			free(event);
			return 0;
		}
	}
	enumerator->Destroy(enumerator);

	return -1;
}

static int syssig_register_event(LinkedList * events, MyEvent * info)
{
	MyEvent * event = NULL;

	/* Create new event for this setting */
	if ((event = malloc(sizeof(MyEvent))) == NULL)
		return -1;
	memcpy(event, info, sizeof(MyEvent));

	if (events->InsertLast(events, event) < 0)
	{
		free(event);
		return -2;
	}

	return 0;
}

/**************************************************************/
/****                SYSSIG Private Method                *****/
/**************************************************************/

static EDRtn M_Subscribe(EDEvt * self, void * pInfo)
{
	EDEvtSysSigInfo * info = pInfo;
	MyEDEvt * this  = (MyEDEvt *) self;
	MyEvent * event = NULL;

	if (info->sig < 1 || info->sig > SIGRTMAX)
		return EDRTN_ERROR;

	if ((event = syssig_find_event(this->events, info->sig)) == NULL)
	{
		if (info->cb == NULL)
			return EDRTN_ERROR;

		if (syssig_register_event(this->events, info) < 0)
			return EDRTN_ERROR;

		sigaddset(&this->mask, info->sig);
		return EDRTN_SUCCESS;
	}

	if (info->cb == NULL)
	{
		if (syssig_remove_event(this->events, info->sig) < 0)
			return EDRTN_ERROR;

		sigdelset(&this->mask, info->sig);
		return EDRTN_SUCCESS;
	}

	event->cb    = info->cb;
	event->pData = info->pData;
	return EDRTN_SUCCESS;
}

static EDRtn M_Bind(EDEvt * self, struct pollfd * pfd, uint32_t * idx)
{
	MyEDEvt * this  = (MyEDEvt *) self;

	if (*idx > 0)
		return EDRTN_EVT_BIND_IGNORE;

	/* If not events need to bind */
	if (this->events->GetCount(this->events) == 0)
	{
		sigemptyset(&this->mask);
		if (this->fd > 0)
		{
			close(this->fd);
			this->fd = -1;
		}
		return EDRTN_EVT_BIND_IGNORE;
	}

	/* Block the signals that we handle using signalfd */
	if (sigprocmask(SIG_BLOCK, &this->mask, NULL) < 0)
		return EDRTN_ERROR;

	/* Recreate signalfd */
	if (this->fd > 0)
		close(this->fd);
	if ((this->fd = signalfd(-1, &this->mask, 0)) < 0)
		return EDRTN_ERROR;

	/* Bind into current fds */
	pfd->fd      = this->fd;
	pfd->events  = POLLIN;
	pfd->revents = 0;

	return EDRTN_SUCCESS;
}

static EDRtn M_Handle(EDEvt * self, struct pollfd * pfd)
{
	MyEDEvt * this  = (MyEDEvt *) self;
	MyEvent * event = NULL;
	struct signalfd_siginfo si;
	ssize_t size;

	if (pfd->fd != this->fd)
		return EDRTN_EVT_HANDLE_NOT_YET;

	if (pfd->revents & POLLERR)
		return EDRTN_ERROR;

	if (!(pfd->revents & POLLIN))
		return EDRTN_SUCCESS;

	if ((size = read(pfd->fd, &si, sizeof(si))) != sizeof(si))
		return EDRTN_ERROR;

	if ((event = syssig_find_event(this->events, si.ssi_signo)))
	{
		if (event->cb)
			event->cb(&this->self, event);
	}

	return EDRTN_SUCCESS;
}

static void M_Destroy(EDEvt * self)
{
	MyEDEvt * this = (MyEDEvt *) self;

	if (this == NULL)
		return;

	if (this->fd > 0)
		close(this->fd);

	if (this->events)
	{
		this->events->RemoveAll(this->events, free);
		this->events->Destroy(this->events);
	}

	free(this);
}

EDEvt * EDEvtSysSigCreate()
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

	if ((this->events = LinkedListCreate()) == NULL)
	{
		M_Destroy(&this->self);
		return NULL;
	}

	this->fd  = -1;
	sigemptyset (&this->mask);

	return &this->self;
}

