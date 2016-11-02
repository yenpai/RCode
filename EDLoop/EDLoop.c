#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include "LinkedList.h"
#include "EDLoop.h"

typedef struct MyEDLoop MyEDLoop;
struct MyEDLoop {
	EDLoop			self;
	uint8_t			status; /* bit[0] running, bit[1] need_stop */
	LinkedList *	events;

	struct pollfd * poll_fds;
	nfds_t          poll_cur_nfds;
	nfds_t          poll_max_nfds;

#ifdef EDLOOP_SUPPORT_SYSSIG
	int				syssig_fd;		/* system signalfd of kernel */
#endif
};

/**************************************************************/

#ifdef EDLOOP_SUPPORT_LOGGER
#include "CLogger.h"
#define TAG "EDLOOP"
#else
#define LOG_D(...) do {} while(0)
#define LOG_E(...) do {} while(0)
#endif

/**************************************************************/

#define STATUS_RUNNING_BIT	(1 << 0)
#define STATUS_NEEDSTOP_BIT (1 << 1)

#define IS_STATUS(this, bit) (this->status & bit)
#define IS_RUNNING(this)     IS_STATUS(this, STATUS_RUNNING_BIT)
#define IS_NEEDSTOP(this)    IS_STATUS(this, STATUS_NEEDSTOP_BIT)

#define SET_STATUS(this, bit, val) \
	(this->status = (val) ? (this->status | bit) : (this->status & (~bit)))
#define SET_RUNNING(this, val)  SET_STATUS(this, STATUS_RUNNING_BIT, val)
#define SET_NEEDSTOP(this, val) SET_STATUS(this, STATUS_NEEDSTOP_BIT, val)

/**************************************************************/
/****                EDLOOP                               *****/
/**************************************************************/

static int edloop_expand_fds(MyEDLoop * this, nfds_t nfds)
{
	if (nfds > this->poll_max_nfds)
	{
		if (this->poll_fds == NULL)
		{
			if ((this->poll_fds = malloc(sizeof(struct pollfd) * nfds)) == NULL)
				return -1;
		}
		else
		{
			if ((this->poll_fds = realloc(this->poll_fds, sizeof(struct pollfd) * nfds)) == NULL)
				return -2;
		}

		this->poll_max_nfds = nfds;
	}
	
	return 0;
}

static int edloop_setup_new_fds(MyEDLoop * this, struct pollfd pfd)
{
	nfds_t num = this->poll_cur_nfds;

	if (edloop_expand_fds(this, num + 1) < 0)
		return -1;

	this->poll_cur_nfds = num + 1;
	this->poll_fds[num] = pfd;
	return 0;
}

/**************************************************************/
/****                EDLOOP_SUPPORT_SYSSIG                *****/
/**************************************************************/
#ifdef EDLOOP_SUPPORT_SYSSIG
#include <signal.h>
#include <sys/signalfd.h>

static EDEvent * syssig_find_event(LinkedList * events, int sig)
{
	Enumerator * enumerator;
	EDEvent    * event = NULL;

	enumerator = events->CreateEnumerator(events);
	while ((event = enumerator->Enumerate(enumerator)))
	{
		if (event->type == EDEVT_TYPE_SYSSIG && event->signal_id == sig)
			break;
	}
	enumerator->Destroy(enumerator);

	return event;
}

static int syssig_remove_event(LinkedList * events, int sig)
{
	Enumerator * enumerator;
	EDEvent    * event = NULL;

	enumerator = events->CreateEnumerator(events);
	while ((event = enumerator->Enumerate(enumerator)))
	{
		if (event->type == EDEVT_TYPE_SYSSIG && event->signal_id == sig)
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

static int syssig_bind_fds(MyEDLoop * this)
{
	Enumerator *  enumerator;
	EDEvent    *  event = NULL;
	sigset_t      mask;
	int           found = 0;
	struct pollfd pfd;

	sigemptyset (&mask);

	/* Check signal event has been registered */
	enumerator = this->events->CreateEnumerator(this->events);
	while ((event = enumerator->Enumerate(enumerator)))
	{
		if (event->type == EDEVT_TYPE_SYSSIG)
		{
			sigaddset(&mask, event->signal_id);
			found = 1;
		}
	}
	enumerator->Destroy(enumerator);

	if (found == 0)
		return 0;

	/* Block the signals that we handle using signalfd */
	if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0)
		return -1;

	/* Recreate signalfd */
	if (this->syssig_fd > 0)
		close(this->syssig_fd);

	if ((this->syssig_fd = signalfd(-1, &mask, 0)) < 0)
		return -2;

	/* Bind into current fds */
	pfd.fd      = this->syssig_fd;
	pfd.events  = POLLIN;
	pfd.revents = 0;
	if (edloop_setup_new_fds(this, pfd) < 0)
		return -3;

	return 0;
}

static int syssig_handle_pfd(MyEDLoop * this, struct pollfd * pfd)
{
	EDEvent * event = NULL;
	struct signalfd_siginfo si;
	ssize_t size;

	if (pfd->fd != this->syssig_fd)
		return 1;

	if (pfd->revents & POLLERR)
		return -1;

	if (!(pfd->revents & POLLIN))
		return 0;

	if ((size = read(pfd->fd, &si, sizeof(si))) != sizeof(si))
		return -2;

	if ((event = syssig_find_event(this->events, si.ssi_signo)) == NULL)
		return 0;

	event->signal_cb(&this->self, event->signal_id);
	return 0;
}

static int syssig_init(MyEDLoop * this)
{
	this->syssig_fd = -1;
	return 0;
}

static void syssig_destroy(MyEDLoop * this)
{
	if (this->syssig_fd > 0)
		close(this->syssig_fd);
}

#endif

/**************************************************************/

#ifdef EDLOOP_SUPPORT_SYSSIG
static int M_RegSysSignal(EDLoop * self, int sig, EDLoopSignalCB cb)
{
	MyEDLoop * this  = (MyEDLoop *) self;
	EDEvent  * event = NULL;

	if (sig < 1 || sig > SIGRTMAX)
		return -1;

	/* Check this event has been regiseteed */
	if ((event = syssig_find_event(this->events, sig)) != NULL)
	{
		if (cb == NULL)
		{
			/* Remove this signal when cb was NULL */
			syssig_remove_event(this->events, sig);
		}
		else
		{
			/* Change this signal callback only */
			event->signal_cb = cb;
		}

		return 0;
	}

	if (cb == NULL)
		return -2;

	/* Create new event for this setting */
	if ((event = malloc(sizeof(EDEvent))) == NULL)
		return -3;

	memset(event, 0, sizeof(EDEvent));
	event->type = EDEVT_TYPE_SYSSIG; 
	event->signal_id = sig;
	event->signal_cb = cb;

	this->events->InsertLast(this->events, event);
	return 0;
}
#endif

#ifdef EDLOOP_SUPPORT_CUSSIG
static int M_RegCusSignal(EDLoop * self, int sig, EDLoopSignalCB cb)
{
	return -1;
}
#endif

#ifdef EDLOOP_SUPPORT_IOEVT
static int M_RegIOEvt(EDLoop * self, int fd,  int events, EDLoopIOEvtCB cb)
{
	return -1;
}
#endif
	
#ifdef EDLOOP_SUPPORT_DBUS
static int M_RegDBusMethod(EDLoop * self, char * ifname, char * method, EDLoopDBusCB cb)
{
	return -1;
}

static int M_RegDBusSignal(EDLoop * self, char * ifname, char * signal, EDLoopDBusCB cb)
{
	return -1;
}
#endif

static int M_Loop (EDLoop * self)
{
	MyEDLoop * this = (MyEDLoop *) self;
	int error_code = 0;
	int update_fds = 0;
	int timeout    = -1;
	int count, ret;
	nfds_t num;

	if (IS_RUNNING(this))
		return -1;
	SET_RUNNING(this, 1);

	/* Start */
	do {

		update_fds = 0;

		/* Initial and Setup poll fds */
		this->poll_cur_nfds = 0;

#ifdef EDLOOP_SUPPORT_SYSSIG
		if (syssig_bind_fds(this) < 0)
		{
			error_code = -10;
			goto stop;
		}
#endif

		/* Check poll fds number */
		if (this->poll_cur_nfds == 0)
		{
			error_code = -11;
			goto stop;
		}

		/* Poll loop */
		while (!IS_NEEDSTOP(this))
		{
			LOG_D(TAG, "Start Poll, nfds[%d]", this->poll_cur_nfds);

			if ((count = poll(this->poll_fds, this->poll_cur_nfds, timeout)) < 0)
			{
				error_code = -12;
				goto stop;
			}

			if (count == 0)
				continue;

			LOG_D(TAG, "Poll event counnt[%d]", count);

			for (num = 0 ; num < this->poll_cur_nfds ; num++)
			{
				if (this->poll_fds[num].revents > 0)
				{
#ifdef EDLOOP_SUPPORT_SYSSIG
					if ((ret = syssig_handle_pfd(this, &this->poll_fds[num])) < 0)
					{
						LOG_E(TAG, "syssig_handle_pfd error! ret[%d]", ret);
						error_code = -21;
						goto stop;
					}
					else if (ret == 0) continue;
#endif
				}
			}
		}

	} while (update_fds);

	error_code = 0;

stop:

	SET_RUNNING(this,  0);
	SET_NEEDSTOP(this, 0);

	return error_code;
}

static void M_Stop (EDLoop * self)
{
	MyEDLoop * this = (MyEDLoop *) self;

	if (IS_RUNNING(this))
	{
		SET_NEEDSTOP(this, 1);
	}
}

static void M_Destroy (EDLoop * self)
{
	MyEDLoop * this = (MyEDLoop *) self;

	if (this == NULL)
		return;

#ifdef EDLOOP_SUPPORT_SYSSIG
	syssig_destroy(this);
#endif

	if (this->poll_fds)
		free(this->poll_fds);

	free(this);
}

/**************************************************************/

EDLoop * EDLoopCreate()
{
	MyEDLoop * this = NULL;
	EDLoop     self = (EDLoop) {
		.Loop    = M_Loop,
		.Stop    = M_Stop,
		.Destroy = M_Destroy,
#ifdef EDLOOP_SUPPORT_SYSSIG
		.RegSysSignal = M_RegSysSignal,
#endif
#ifdef EDLOOP_SUPPORT_CUSSIG
		.RegCusSignal = M_RegCusSignal,
#endif
#ifdef EDLOOP_SUPPORT_IOEVT
		.RegIOEvt     = M_RegIOEvt,
#endif
#ifdef EDLOOP_SUPPORT_DBUS
		.RegDBusMethod = M_RegDBusMethod,
		.RegDBusSignal = M_RegDBusSignal,
#endif
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

#ifdef EDLOOP_SUPPORT_SYSSIG
	if (syssig_init(this) < 0)
	{
		M_Destroy(&this->self);
		return NULL;
	}
#endif

	this->poll_fds      = NULL;
	this->poll_cur_nfds = 0;
	this->poll_max_nfds = 0;

	return &this->self;
}
