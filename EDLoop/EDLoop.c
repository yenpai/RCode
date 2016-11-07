#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include "LinkedList.h"
#include "EDLoop.h"

typedef struct MyEDLoop MyEDLoop;
typedef struct MyEvtMap MyEvtMap;

struct MyEDLoop {
	EDLoop			self;
	uint8_t			status;  /* bit[0] running, bit[1] need_stop */
	LinkedList *	events;  /* EDEvt */

	struct pollfd * pfds;
	nfds_t          pfds_cur;
	nfds_t          pfds_max;
};

/**************************************************************/

#ifdef EDLOOP_SUPPORT_LOGGER
#include "CLogger.h"
#define TAG "EDLOOP"
#else
#define LOG_V(...) do {} while(0)
#define LOG_I(...) do {} while(0)
#define LOG_W(...) do {} while(0)
#define LOG_D(...) do {} while(0)
#define LOG_E(...) do {} while(0)
#endif

/**************************************************************/

#define STATUS_RUNNING_BIT	   (1 << 0)
#define STATUS_POLLING_BIT	   (1 << 1)
#define STATUS_NEED_STOP_BIT   (1 << 2)
#define STATUS_NEED_UPDATE_BIT (1 << 3)

#define IS_STATUS(this, bit) (this->status & bit)
#define IS_RUNNING(this)     IS_STATUS(this, STATUS_RUNNING_BIT)
#define IS_POLLING(this)     IS_STATUS(this, STATUS_POLLING_BIT)
#define IS_NEED_STOP(this)   IS_STATUS(this, STATUS_NEED_STOP_BIT)
#define IS_NEED_UPDATE(this) IS_STATUS(this, STATUS_NEED_UPDATE_BIT)

#define SET_STATUS(this, bit, val) \
	(this->status = (val) ? (this->status | bit) : (this->status & (~bit)))
#define SET_RUNNING(this, val)     SET_STATUS(this, STATUS_RUNNING_BIT, val)
#define SET_POLLING(this, val)     SET_STATUS(this, STATUS_POLLING_BIT, val)
#define SET_NEED_STOP(this, val)   SET_STATUS(this, STATUS_NEED_STOP_BIT, val)
#define SET_NEED_UPDATE(this, val) SET_STATUS(this, STATUS_NEED_UPDATE_BIT, val)

/**************************************************************/
/****                EDLOOP Private Method                *****/
/**************************************************************/

static int edloop_expand_fds(MyEDLoop * this, nfds_t nfds)
{
	if (nfds > this->pfds_max)
	{
		if (this->pfds == NULL)
		{
			if ((this->pfds = malloc(sizeof(struct pollfd) * nfds)) == NULL)
				return -1;
		}
		else
		{
			if ((this->pfds = realloc(this->pfds, sizeof(struct pollfd) * nfds)) == NULL)
				return -2;
		}

		this->pfds_max = nfds;
	}
	
	return 0;
}

static int edloop_update_fds(MyEDLoop * this)
{
	Enumerator    * enumerator;
	EDEvt         * event  = NULL;
	struct pollfd * pfd    = NULL;
	uint32_t idx;
	EDRtn    rtn;

	LOG_V(TAG, "edloop update all fds by events.");

	/* ReInitial and Setup poll fds */
	this->pfds_cur = 0;

	enumerator = this->events->CreateEnumerator(this->events);
	while ((event = enumerator->Enumerate(enumerator)))
	{
		idx = 0;

		do {

			/* Expand ones fds for event bind */
			if (edloop_expand_fds(this, this->pfds_cur + 1) < 0)
			{
				LOG_E(TAG, "edloop_expand_fds Failed!");
				return -1;
			}
			pfd = &this->pfds[this->pfds_cur];

			/* Bind event to current pfd */
			if ((rtn = event->Bind(event, pfd, &idx)) == EDRTN_ERROR)
			{
				LOG_E(TAG, "Event Bind Failed!");
				return -1;
			}

			if (rtn == EDRTN_EVT_BIND_IGNORE)
				break;
			
			this->pfds_cur++;

			if (rtn == EDRTN_SUCCESS)
				break;
			
			idx++;

		} while (rtn == EDRTN_EVT_BIND_NEXT);

	}
	enumerator->Destroy(enumerator);

	return 0;
}

static int edloop_handle_fds(MyEDLoop * this)
{
	Enumerator * enumerator;
	EDEvt      * event = NULL;
	EDRtn        rtn;
	nfds_t num;

	LOG_V(TAG, "edloop handle all fds by events.");

	for (num = 0 ; num < this->pfds_cur ; num++)
	{
		if (IS_NEED_STOP(this))
			return 0;

		if (this->pfds[num].revents == 0)
			continue;

		enumerator = this->events->CreateEnumerator(this->events);
		while ((event = enumerator->Enumerate(enumerator)))
		{
			if ((rtn = event->Handle(event, &this->pfds[num])) == EDRTN_ERROR)
			{
				LOG_E(TAG, "Event Handle Failed!");
				enumerator->Destroy(enumerator);
				return -1;
			}

			if (rtn == EDRTN_SUCCESS)
				break;
		}
		enumerator->Destroy(enumerator);
	}

	return 0;
}

static int edloop_loop(MyEDLoop * this, int timeout)
{
	int error_code = 0;
	int count, ret;

	SET_NEED_UPDATE(this, 1);

update_fds:

	/* Initial or Update fds */
	while (IS_NEED_UPDATE(this))
	{
		SET_NEED_UPDATE(this, 0);

		/* Update fds */
		if ((ret = edloop_update_fds(this) < 0))
		{
			LOG_E(TAG, "edloop_update_fds failed! ret[%d].", ret);
			error_code = -1;
			goto error_return;
		}
	}

	/* Check poll fds number */
	if (this->pfds_cur == 0)
	{
		LOG_E(TAG, "edloop not binding any pfd!");
		error_code = -2;
		goto error_return;
	}

	/* Poll loop */
	while (!IS_NEED_STOP(this))
	{
		LOG_D(TAG, "Start Poll, nfds[%d]", this->pfds_cur);

		SET_POLLING(this, 1);
		count = poll(this->pfds, this->pfds_cur, timeout);
		SET_POLLING(this, 0);

		if (count < 0)
		{
			error_code = -3;
			goto error_return;
		}

		LOG_D(TAG, "Got Poll event count[%d]", count);
		if (count == 0)
			continue;

		if ((ret = edloop_handle_fds(this)) < 0)
		{
			error_code = -4;
			goto error_return;
		}

		if (IS_NEED_UPDATE(this))
			goto update_fds;
	}
	
	return 0;

error_return:

	LOG_E(TAG, "Exit loop because something failed! error_code[%d]!", error_code);
	return error_code;
}

/**************************************************************/
/****                EDLOOP Public Method                 *****/
/**************************************************************/

static int M_AddEvt(EDLoop * self, EDEvt * evt)
{
	MyEDLoop * this = (MyEDLoop *) self;

	if (this->events->InsertLast(this->events, evt) < 0)
		return -1;

	SET_NEED_UPDATE(this, 1);
	return 0;
}

static int M_Loop(EDLoop * self)
{
	MyEDLoop * this = (MyEDLoop *) self;
	int error_code = 0;

	if (IS_RUNNING(this))
		return -1;

	SET_RUNNING(this, 1);
	error_code = edloop_loop(this, -1);
	SET_RUNNING(this,  0);

	SET_NEED_STOP(this, 0);
	return error_code;
}

static void M_Update(EDLoop * self)
{
	MyEDLoop * this = (MyEDLoop *) self;
	SET_NEED_UPDATE(this, 1);
}

static void M_Stop(EDLoop * self)
{
	MyEDLoop * this = (MyEDLoop *) self;

	if (IS_RUNNING(this))
	{
		SET_NEED_STOP(this, 1);
	}
}

static void M_Destroy(EDLoop * self)
{
	MyEDLoop * this = (MyEDLoop *) self;

	if (this == NULL)
		return;

	if (this->pfds)
		free(this->pfds);

	if (this->events)
		this->events->Destroy(this->events);

	free(this);
}

/**************************************************************/

EDLoop * EDLoopCreate()
{
	MyEDLoop * this = NULL;
	EDLoop     self = (EDLoop) {
		.AddEvt  = M_AddEvt,
		.Loop    = M_Loop,
		.Update  = M_Update,
		.Stop    = M_Stop,
		.Destroy = M_Destroy,
	};

	if ((this = malloc(sizeof(MyEDLoop))) == NULL)
		return NULL;
	memset(this, 0, sizeof(MyEDLoop));
	memcpy(this, &self, sizeof(EDLoop));

	if ((this->events = LinkedListCreate()) == NULL)
	{
		M_Destroy(&this->self);
		return NULL;
	}

	this->pfds     = NULL;
	this->pfds_cur = 0;
	this->pfds_max = 0;

	return &this->self;
}
