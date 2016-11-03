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

#ifdef EDLOOP_SUPPORT_DBUS
	DBusConnection * dbus_conn;
	LinkedList *     dbus_watchs;
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

static int syssig_register_event(LinkedList * events, int sig, EDLoopSignalCB cb)
{
	EDEvent * event = NULL;

	if (sig < 1 || sig > SIGRTMAX)
		return -1;

	/* Check this event has been regiseteed */
	if ((event = syssig_find_event(events, sig)) != NULL)
	{
		if (cb == NULL)
		{
			/* Remove this signal when cb was NULL */
			syssig_remove_event(events, sig);
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

	if (events->InsertLast(events, event) < 0)
	{
		free(event);
		return -4;
	}

	return 0;
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
/****                EDLOOP_SUPPORT_DBUS                  *****/
/**************************************************************/
#ifdef EDLOOP_SUPPORT_DBUS

static EDEvent * eddbus_find_event(LinkedList * events, uint32_t type, char * ifname, char * mtname)
{
	Enumerator * enumerator;
	EDEvent    * event = NULL;

	enumerator = events->CreateEnumerator(events);
	while ((event = enumerator->Enumerate(enumerator)))
	{
		if (	event->type == type && 
				strcmp(event->dbus_ifname, ifname) == 0 &&
				strcmp(event->dbus_mtname, mtname) == 0 )
			break;
	}
	enumerator->Destroy(enumerator);

	return event;
}

static int eddbus_remove_event(LinkedList * events, uint32_t type, char * ifname, char * mtname)
{
	Enumerator * enumerator;
	EDEvent    * event = NULL;

	enumerator = events->CreateEnumerator(events);
	while ((event = enumerator->Enumerate(enumerator)))
	{
		if (	event->type == type && 
				strcmp(event->dbus_ifname, ifname) == 0 &&
				strcmp(event->dbus_mtname, mtname) == 0 )
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

static int eddbus_register_event(LinkedList * events, uint32_t type, char * ifname, char * mtname, EDLoopDBusCB cb)
{
	EDEvent * event = NULL;

	if (type != EDEVT_TYPE_DBUS_METHOD && type != EDEVT_TYPE_DBUS_SIGNAL)
		return -1;
	if (ifname == NULL || ifname[0] == '\0' || strlen(ifname) >= sizeof(event->dbus_ifname))
		return -1;
	if (mtname == NULL || mtname[0] == '\0' || strlen(mtname) >= sizeof(event->dbus_mtname))
		return -1;
	
	/* Check this event has been regiseteed */
	if ((event = eddbus_find_event(events, type, ifname, mtname)) != NULL)
	{
		if (cb == NULL)
		{
			/* Remove this signal when cb was NULL */
			eddbus_remove_event(events, type, ifname, mtname);
		}
		else
		{
			/* Change this signal callback only */
			event->dbus_cb = cb;
		}

		return 0;
	}

	/* Check callback function before register new event */
	if (cb == NULL)
		return -2;

	/* Create new event for this setting */
	if ((event = malloc(sizeof(EDEvent))) == NULL)
		return -3;

	memset(event, 0, sizeof(EDEvent));
	event->type		 = type; 
	event->dbus_cb   = cb;
	strcpy(event->dbus_ifname, ifname);
	strcpy(event->dbus_mtname, mtname);

	if (events->InsertLast(events, event) < 0)
	{
		free(event);
		return -4;
	}

	return 0;
}

static dbus_bool_t eddbus_watch_add(DBusWatch * watch, void * data)
{
	MyEDLoop * this = data;

	LOG_D(TAG, "Add DBusWatch");

	if (this->dbus_watchs->InsertLast(this->dbus_watchs, watch) < 0)
		return FALSE;

	return TRUE;
}

static void eddbus_watch_remove(DBusWatch * watch, void * data)
{
	MyEDLoop   * this = data;
	Enumerator * enumerator;
	DBusWatch  * watch2;
	int found = 0;

	LOG_D(TAG, "Remove DBusWatch");

	enumerator = this->dbus_watchs->CreateEnumerator(this->dbus_watchs);
	while ((watch2 = enumerator->Enumerate(enumerator)))
	{
		if (watch == watch2)
		{
			this->dbus_watchs->RemoveAt(this->dbus_watchs, enumerator);
			enumerator->Destroy(enumerator);
			found = 1;
			break;
		}
	}
	enumerator->Destroy(enumerator);

	if (found == 0)
	{
		LOG_D(TAG, "Cannot found the dbus watch in list, Ignore.");
		return;
	}
}

static void eddbus_watch_toggle(DBusWatch * watch, void * data) 
{
	if (dbus_watch_get_enabled(watch))
	{
		LOG_D(TAG, "DBus Watch Toggle to Enabled.");
		eddbus_watch_add(watch, data);
	}
	else
	{
		LOG_D(TAG, "DBus Watch Toggle to Disabled.");
		eddbus_watch_remove(watch, data);
	}
}

static DBusHandlerResult eddbus_filter (DBusConnection * conn, DBusMessage *message, void * pdata)
{
	MyEDLoop * this = pdata;
	Enumerator *  enumerator;
	EDEvent    *  event = NULL;

	/* Check signal event has been registered */
	enumerator = this->events->CreateEnumerator(this->events);
	while ((event = enumerator->Enumerate(enumerator)))
	{
		if (event->type == EDEVT_TYPE_DBUS_METHOD)
		{
			if (dbus_message_is_method_call(
						message, 
						event->dbus_ifname,
						event->dbus_mtname))
			{
				event->dbus_cb(&this->self, message);
				enumerator->Destroy(enumerator);
				return DBUS_HANDLER_RESULT_HANDLED;
			}

			continue;
		}

		if (event->type == EDEVT_TYPE_DBUS_SIGNAL)
		{
			if (dbus_message_is_signal(
						message, 
						event->dbus_ifname,
						event->dbus_mtname))
			{
				event->dbus_cb(&this->self, message);
				enumerator->Destroy(enumerator);
				return DBUS_HANDLER_RESULT_HANDLED;
			}

			continue;
		}
	}
	enumerator->Destroy(enumerator);

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	(void) conn; /* unused */
}

static int eddbus_bind_fds(MyEDLoop * this)
{
	Enumerator *  enumerator;
	EDEvent    *  event = NULL;
	DBusWatch  *  watch = NULL;
	uint8_t       found = 0;
	uint32_t      flags;
	struct pollfd pfd;

	if (this->dbus_watchs->GetCount(this->dbus_watchs) == 0)
		return 0;

	/* Check signal event has been registered */
	enumerator = this->events->CreateEnumerator(this->events);
	while ((event = enumerator->Enumerate(enumerator)))
	{
		if (event->type == EDEVT_TYPE_DBUS_METHOD || event->type == EDEVT_TYPE_DBUS_SIGNAL)
		{
			found = 1;
			break;
		}
	}
	enumerator->Destroy(enumerator);

	if (found == 0)
		return 0;

	if (dbus_connection_add_filter(this->dbus_conn, eddbus_filter, this, NULL) == FALSE)
		return -1;

	/* Bind all watch into current fds */
	enumerator = this->dbus_watchs->CreateEnumerator(this->dbus_watchs);
	while ((watch = enumerator->Enumerate(enumerator)))
	{
		if (!dbus_watch_get_enabled(watch))
			continue;

		pfd.fd      = dbus_watch_get_unix_fd(watch);
		pfd.events  = 0;
		pfd.revents = 0;

		flags = dbus_watch_get_flags(watch);
		if (flags & DBUS_WATCH_READABLE)
			pfd.events |= POLLIN;
		if (flags & DBUS_WATCH_WRITABLE)
			pfd.events |= POLLOUT;

		if (edloop_setup_new_fds(this, pfd) < 0)
		{
			enumerator->Destroy(enumerator);
			return -1;
		}
	}
	enumerator->Destroy(enumerator);

	return 0;
}

static int eddbus_handle_pfd(MyEDLoop * this, struct pollfd * pfd)
{
	Enumerator * enumerator;
	DBusWatch  * watch = NULL;
	uint8_t      found = 0;
	uint32_t     flags = 0;

	/* Check FD was DBus Watch */
	enumerator = this->dbus_watchs->CreateEnumerator(this->dbus_watchs);
	while ((watch = enumerator->Enumerate(enumerator)))
	{
		if (dbus_watch_get_unix_fd(watch) == pfd->fd)
		{
			found = 1;
			break;
		}
	}
	enumerator->Destroy(enumerator);

	if (found == 0)
		return 1;

	if (pfd->revents & POLLERR)
		flags |= DBUS_WATCH_ERROR;
	if (pfd->revents & POLLHUP)
		flags |= DBUS_WATCH_HANGUP;
	if (pfd->revents & POLLIN)
		flags |= DBUS_WATCH_READABLE;
	if (pfd->revents & POLLOUT)
		flags |= DBUS_WATCH_WRITABLE;

	LOG_D(TAG, "DBus watch event: fd[%d], events[%d]", pfd->fd, pfd->revents);
	if (!dbus_watch_handle(watch, flags))
		return -1;

	LOG_D(TAG, "DBus handle dispatching.");
	while (dbus_connection_get_dispatch_status(this->dbus_conn) == DBUS_DISPATCH_DATA_REMAINS)
		dbus_connection_dispatch(this->dbus_conn);

	return 0;
}

static int eddbus_init(MyEDLoop * this)
{
	this->dbus_conn = NULL;

	if ((this->dbus_watchs = LinkedListCreate()) == NULL)
		return -1;

	return 0;
}

static void eddbus_destroy(MyEDLoop * this)
{
	DBusWatch * watch = NULL;

	if (this->dbus_watchs)
	{
		while ((watch = this->dbus_watchs->RemoveLast(this->dbus_watchs)))
			dbus_watch_set_data(watch, NULL, NULL);
	}
}

#endif

/**************************************************************/
/****                EDLOOP Polling Loop                  *****/
/**************************************************************/

static int edloop_update_fds(MyEDLoop * this)
{
	/* ReInitial and Setup poll fds */
	this->poll_cur_nfds = 0;

#ifdef EDLOOP_SUPPORT_SYSSIG
	if (syssig_bind_fds(this) < 0)
		return -1;
#endif

#ifdef EDLOOP_SUPPORT_DBUS
	if (eddbus_bind_fds(this) < 0)
		return -2;
#endif

	return 0;
}

static int edloop_handle_fds(MyEDLoop * this)
{
	int ret;
	nfds_t num;

	for (num = 0 ; num < this->poll_cur_nfds ; num++)
	{
		if (this->poll_fds[num].revents > 0)
		{
#ifdef EDLOOP_SUPPORT_SYSSIG
			if ((ret = syssig_handle_pfd(this, &this->poll_fds[num])) < 0)
			{
				LOG_E(TAG, "syssig_handle_pfd error! ret[%d]", ret);
				return ret;
			}
			else if (ret == 0) continue;
#endif
#ifdef EDLOOP_SUPPORT_DBUS
			if ((ret = eddbus_handle_pfd(this, &this->poll_fds[num])) < 0)
			{
				LOG_E(TAG, "eddbus_handle_pfd error! ret[%d]", ret);
				return ret;
			}
			else if (ret == 0) continue;
#endif
		}
	}

	return 0;
}

static int edloop_loop(MyEDLoop * this, int timeout)
{
	int error_code = 0;
	int count, ret;

	SET_NEED_UPDATE(this, 1);

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

		/* Check poll fds number */
		if (this->poll_cur_nfds == 0)
		{
			error_code = -2;
			goto error_return;
		}
	}

	/* Poll loop */
	while (!IS_NEED_STOP(this))
	{
		LOG_D(TAG, "Start Poll, nfds[%d]", this->poll_cur_nfds);

		SET_POLLING(this, 1);
		count = poll(this->poll_fds, this->poll_cur_nfds, timeout);
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
	}
	
	return 0;

error_return:

	LOG_E(TAG, "Exit loop because something failed! error_code[%d]!", error_code);
	return error_code;
}

/**************************************************************/
/****                EDLOOP Public Method                 *****/
/**************************************************************/
#ifdef EDLOOP_SUPPORT_SYSSIG
static int M_RegSysSignal(EDLoop * self, int sig, EDLoopSignalCB cb)
{
	MyEDLoop * this  = (MyEDLoop *) self;
	return syssig_register_event(this->events, sig, cb);
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
static int M_RegDBusConn(EDLoop * self, DBusConnection * pConn)
{
	MyEDLoop * this = (MyEDLoop *) self;

	if (this->dbus_conn)
		return -1;

	/* Setup dbus watch */
	if (!dbus_connection_set_watch_functions(
				pConn, 
				eddbus_watch_add, 
				eddbus_watch_remove, 
				eddbus_watch_toggle, 
				this, NULL))
	{
		LOG_E(TAG, "dbus_connection_set_watch_functions failed!");
		return -2;
	}

	dbus_bus_add_match(pConn, "type='signal'", NULL);
	dbus_bus_add_match(pConn, "type='method_call'", NULL);

	this->dbus_conn = pConn;
	return 0;
}

static int M_RegDBusMethod(EDLoop * self, char * ifname, char * method, EDLoopDBusCB cb)
{
	MyEDLoop * this = (MyEDLoop *) self;
	return eddbus_register_event(this->events, EDEVT_TYPE_DBUS_METHOD, ifname, method, cb);
}

static int M_RegDBusSignal(EDLoop * self, char * ifname, char * signal, EDLoopDBusCB cb)
{
	MyEDLoop * this = (MyEDLoop *) self;
	return eddbus_register_event(this->events, EDEVT_TYPE_DBUS_SIGNAL, ifname, signal, cb);
}
#endif

static int M_Loop (EDLoop * self)
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

static void M_Stop (EDLoop * self)
{
	MyEDLoop * this = (MyEDLoop *) self;

	if (IS_RUNNING(this))
	{
		SET_NEED_STOP(this, 1);
	}
}

static void M_Destroy (EDLoop * self)
{
	MyEDLoop * this = (MyEDLoop *) self;

	if (this == NULL)
		return;

	if (this->poll_fds)
		free(this->poll_fds);

#ifdef EDLOOP_SUPPORT_SYSSIG
	syssig_destroy(this);
#endif

#ifdef EDLOOP_SUPPORT_DBUS
	eddbus_destroy(this);
#endif

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
		.RegDBusConn   = M_RegDBusConn,
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

	this->poll_fds      = NULL;
	this->poll_cur_nfds = 0;
	this->poll_max_nfds = 0;

#ifdef EDLOOP_SUPPORT_SYSSIG
	if (syssig_init(this) < 0)
	{
		M_Destroy(&this->self);
		return NULL;
	}
#endif

#ifdef EDLOOP_SUPPORT_DBUS
	if (eddbus_init(this) < 0)
	{
		M_Destroy(&this->self);
		return NULL;
	}
#endif

	return &this->self;
}
