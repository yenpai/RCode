#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/signalfd.h>
#include "LinkedList.h"
#include "EDLoop.h"
#include "EDEvtDBus.h"

typedef struct MyEDEvt       MyEDEvt;
typedef struct EDEvtDBusInfo MyEvent;

struct MyEDEvt {
	EDEvt            self;
	DBusConnection * pConn;
	LinkedList     * watchs; /* DBusWatch */
	LinkedList     * events; /* EDEvtDBusInfo */
};

#ifdef EDLOOP_SUPPORT_LOGGER
#include "CLogger.h"
#define TAG "EvtDBus"
#else
#define LOG_V(...) do {} while(0)
#define LOG_I(...) do {} while(0)
#define LOG_W(...) do {} while(0)
#define LOG_D(...) do {} while(0)
#define LOG_E(...) do {} while(0)
#endif

/**************************************************************/
/****                DBus Private Method                  *****/
/**************************************************************/

static MyEvent * eddbus_find_event(LinkedList * events, EDEVT_DBUS_TYPE type, char * ifname, char * mtname)
{
	Enumerator * enumerator;
	MyEvent    * event = NULL;

	enumerator = events->CreateEnumerator(events);
	while ((event = enumerator->Enumerate(enumerator)))
	{
		if (	event->type == type && 
				strcmp(event->ifname, ifname) == 0 &&
				strcmp(event->mtname, mtname) == 0 )
			break;
	}
	enumerator->Destroy(enumerator);

	return event;
}

static int eddbus_remove_event(LinkedList * events, EDEVT_DBUS_TYPE type, char * ifname, char * mtname)
{
	Enumerator * enumerator;
	MyEvent    * event = NULL;

	enumerator = events->CreateEnumerator(events);
	while ((event = enumerator->Enumerate(enumerator)))
	{
		if (	event->type == type && 
				strcmp(event->ifname, ifname) == 0 &&
				strcmp(event->mtname, mtname) == 0 )
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

static int eddbus_register_event(LinkedList * events, MyEvent * info)
{
	MyEvent * event = NULL;

	if (info->type != EDEVT_DBUS_METHOD && info->type != EDEVT_DBUS_SIGNAL)
		return -1;
	if (info->ifname[0] == '\0')
		return -1;
	if (info->mtname[0] == '\0')
		return -1;
	
	/* Check this event has been regiseteed */
	if ((event = eddbus_find_event(events, info->type, info->ifname, info->mtname)) != NULL)
	{
		if (info->cb == NULL)
		{
			/* Remove this signal when cb was NULL */
			eddbus_remove_event(events, info->type, info->ifname, info->mtname);
		}
		else
		{
			/* Change this signal callback only */
			event->cb = info->cb;
		}

		return 0;
	}

	/* Check callback function before register new event */
	if (info->cb == NULL)
		return -2;

	/* Create new event for this setting */
	if ((event = malloc(sizeof(MyEvent))) == NULL)
		return -3;
	memcpy(event, info, sizeof(MyEvent));

	if (events->InsertLast(events, event) < 0)
	{
		free(event);
		return -4;
	}

	return 0;
}

static dbus_bool_t eddbus_watch_add(DBusWatch * watch, void * pData)
{
	MyEDEvt * this = pData;

	LOG_D(TAG, "Add DBusWatch");

	if (this->watchs->InsertLast(this->watchs, watch) < 0)
		return FALSE;

	return TRUE;
}

static void eddbus_watch_remove(DBusWatch * watch, void * pData)
{
	MyEDEvt * this = pData;
	Enumerator * enumerator;
	DBusWatch  * watch2;
	int found = 0;

	LOG_D(TAG, "Remove DBusWatch");

	enumerator = this->watchs->CreateEnumerator(this->watchs);
	while ((watch2 = enumerator->Enumerate(enumerator)))
	{
		if (watch == watch2)
		{
			this->watchs->RemoveAt(this->watchs, enumerator);
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

static void eddbus_watch_toggle(DBusWatch * watch, void * pData) 
{
	if (dbus_watch_get_enabled(watch))
	{
		LOG_D(TAG, "DBus Watch Toggle to Enabled.");
		eddbus_watch_add(watch, pData);
	}
	else
	{
		LOG_D(TAG, "DBus Watch Toggle to Disabled.");
		eddbus_watch_remove(watch, pData);
	}
}

static DBusHandlerResult eddbus_filter_handle(DBusConnection * conn, DBusMessage *message, void * pData)
{
	MyEDEvt * this = pData;
	Enumerator * enumerator;
	MyEvent    * event = NULL;

	LOG_V(TAG, "DBus Message Filter [%d][%s -> %s][%s][%s][%s]%s",
           dbus_message_get_type(message),
           dbus_message_get_sender(message),
           dbus_message_get_destination(message),
           dbus_message_get_path(message),
           dbus_message_get_interface(message),
           dbus_message_get_member(message),
           dbus_message_get_type(message) == DBUS_MESSAGE_TYPE_ERROR ?
           dbus_message_get_error_name(message) : "");

	/* Check signal event has been registered */
	enumerator = this->events->CreateEnumerator(this->events);
	while ((event = enumerator->Enumerate(enumerator)))
	{
		if (event->type == EDEVT_DBUS_METHOD)
		{
			if (dbus_message_is_method_call(
						message, event->ifname, event->mtname))
			{

				event->cb(&this->self, event, message);
				enumerator->Destroy(enumerator);
				return DBUS_HANDLER_RESULT_HANDLED;
			}

			continue;
		}

		if (event->type == EDEVT_DBUS_SIGNAL)
		{
			if (dbus_message_is_signal(
						message, event->ifname, event->mtname))
			{
				event->cb(&this->self, event,  message);
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

static int eddbus_match_add(DBusConnection * conn, EDEvtDBusInfo * info)
{
	DBusError     error;
	char          match[256];
	const char *  match_str = "type='%s',interface='%s',member='%s'";

	dbus_error_init(&error);
	snprintf(match, sizeof(match), match_str, 
			(info->type == EDEVT_DBUS_SIGNAL) ? "signal" : "method_call", 
			info->ifname, info->mtname);
	dbus_bus_add_match(conn, match, &error);
	dbus_connection_flush(conn);

	if (dbus_error_is_set(&error))
	{
		LOG_E(TAG, "dbus_bus_add_match failed! match_str[%s], err[%s]",
				match, error.message);
		dbus_error_free(&error);
		return -1;
	}

	dbus_connection_read_write_dispatch(conn, 0);
	return 0;
}

static int eddbus_match_remove(DBusConnection * conn, EDEvtDBusInfo * info)
{
	DBusError     error;
	char          match[256];
	const char *  match_str = "type='%s',interface='%s',member='%s'";

	dbus_error_init(&error);
	snprintf(match, sizeof(match), match_str, 
			(info->type == EDEVT_DBUS_SIGNAL) ? "signal" : "method_call", 
			info->ifname, info->mtname);
	dbus_bus_remove_match(conn, match, &error);
	dbus_connection_flush(conn);

	if (dbus_error_is_set(&error))
	{
		LOG_E(TAG, "dbus_bus_remove_match failed! match_str[%s], err[%s]",
				match, error.message);
		dbus_error_free(&error);
		return -1;
	}

	dbus_connection_read_write_dispatch(conn, 0);
	return 0;
}

/**************************************************************/
/****                EvtDBus Private Method               *****/
/**************************************************************/

static EDRtn M_Subscribe(EDEvt * self, EDEvtInfo * pInfo)
{
	EDEvtDBusInfo * info = (EDEvtDBusInfo *) pInfo;
	MyEDEvt * this  = (MyEDEvt *) self;
	MyEvent * event = NULL;

	if (pInfo->magic != EDEvtDBusInfoMagic)
		return EDRTN_ERROR;

	/* Register new event if not exist */
	if ((event = eddbus_find_event(
					this->events, info->type, 
					info->ifname, info->mtname)) == NULL)
	{
		if (info->cb == NULL)
			return EDRTN_ERROR;

		if (eddbus_match_add(this->pConn, info) < 0)
			return EDRTN_ERROR;

		if (eddbus_register_event(this->events, info) < 0)
			return EDRTN_ERROR;

		return EDRTN_SUCCESS;
	}

	/* Remove old event if callback not setting */
	if (info->cb == NULL)
	{
		if (eddbus_remove_event(
					this->events, info->type, 
					info->ifname, info->mtname) < 0)
			return EDRTN_ERROR;

		if (eddbus_match_remove(this->pConn, info) < 0)
		{
			// Debug Only
		}

		return EDRTN_SUCCESS;
	}

	/* Update callback only */
	event->cb = info->cb;
	return EDRTN_SUCCESS;
}

static EDRtn M_Bind(EDEvt * self, struct pollfd * pfd, uint32_t * idx)
{
	MyEDEvt * this  = (MyEDEvt *) self;

	Enumerator *  enumerator;
	DBusWatch  *  watch = NULL;
	uint32_t      flags;
	EDRtn         rtn = EDRTN_EVT_BIND_IGNORE;

	uint32_t num = 0;

	/* Check current events */
	if (this->events->GetCount(this->events) == 0)
		return rtn;

	/* Check current watchs */
	if (*idx >= this->watchs->GetCount(this->watchs))
		return rtn;

	/* Got watch[idx] and check need to bind pfdxs */
	enumerator = this->watchs->CreateEnumerator(this->watchs);
	for (num = 0; (watch = enumerator->Enumerate(enumerator)); num++)
	{
		if (num < *idx)
			continue;

		if (!dbus_watch_get_enabled(watch))
			continue;

		pfd->fd = dbus_watch_get_unix_fd(watch);
		pfd->events  = 0;
		pfd->revents = 0;

		flags = dbus_watch_get_flags(watch);
		if (flags & DBUS_WATCH_READABLE)
			pfd->events |= POLLIN;
		if (flags & DBUS_WATCH_WRITABLE)
			pfd->events |= POLLOUT;
		if (pfd->events != 0)
		{
			rtn = EDRTN_EVT_BIND_NEXT;
			break;
		}
	}
	enumerator->Destroy(enumerator);

	/* Change Next to Success if last watch */
	if (rtn == EDRTN_EVT_BIND_NEXT)
	{
		*idx = num;
		if (num >= this->events->GetCount(this->events))
			rtn = EDRTN_SUCCESS;
	}

	return rtn;
}

static EDRtn M_Handle(EDEvt * self, struct pollfd * pfd)
{
	MyEDEvt    * this  = (MyEDEvt *) self;
	Enumerator * enumerator;
	DBusWatch  * watch = NULL;
	uint8_t      found = 0;
	uint32_t     flags = 0;

	/* Check FD was DBus Watch */
	enumerator = this->watchs->CreateEnumerator(this->watchs);
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
	do {
		dbus_connection_read_write_dispatch(this->pConn, 0);
	} while (DBUS_DISPATCH_DATA_REMAINS == dbus_connection_get_dispatch_status(this->pConn));

	return 0;
}

static void M_Destroy(EDEvt * self)
{
	MyEDEvt * this = (MyEDEvt *) self;

	if (this == NULL)
		return;

	if (this->watchs)
	{
		//this->events->RemoveAll(this->watchs, free);
		this->events->Destroy(this->watchs);
	}

	if (this->events)
	{
		this->events->RemoveAll(this->events, free);
		this->events->Destroy(this->events);
	}

	free(this);
}

EDEvt * EDEvtDBusCreate(DBusConnection * pConn)
{
	MyEDEvt * this = NULL;
	EDEvt     self = {
		.Subscribe = M_Subscribe,
		.Bind      = M_Bind,
		.Handle    = M_Handle,
		.Destroy   = M_Destroy,
	};

	if (pConn == NULL)
		return NULL;

	if ((this = malloc(sizeof(MyEDEvt))) == NULL)
		return NULL;
	memset(this, 0,     sizeof(MyEDEvt));
	memcpy(this, &self, sizeof(EDEvt));

	if ((this->watchs = LinkedListCreate()) == NULL)
	{
		M_Destroy(&this->self);
		return NULL;
	}

	if ((this->events = LinkedListCreate()) == NULL)
	{
		M_Destroy(&this->self);
		return NULL;
	}

	this->pConn = pConn;

	/* Setup dbus watch */
	if (!dbus_connection_set_watch_functions(
				pConn, 
				eddbus_watch_add, 
				eddbus_watch_remove, 
				eddbus_watch_toggle, 
				this, NULL))
	{
		M_Destroy(&this->self);
		return NULL;
	}

	/* Setup dbus filter */
	if (dbus_connection_add_filter(
				pConn, 
				eddbus_filter_handle, 
				this, NULL) == FALSE)
	{
		M_Destroy(&this->self);
		return NULL;
	}

	return &this->self;
}

