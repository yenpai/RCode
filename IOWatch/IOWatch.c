#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include "IOWatch.h"
#include "LinkedList.h"

/*****************************************/
/** Interface Definition                **/
/*****************************************/

typedef struct MyIOWatch MyIOWatch;

/*****************************************/
/** Enum Definition                     **/
/*****************************************/

/*****************************************/
/** Struct Definition                   **/
/*****************************************/

struct MyIOWatch {
	IOWatch         self;
	LinkedList    * evList;

	struct pollfd * poll_fds;
	nfds_t          poll_cur_nfds;
	nfds_t          poll_max_nfds;
};

/*****************************************/
/** Private Method                      **/
/*****************************************/

static IOWatchEv * iowatch_find_ev_by_fd(MyIOWatch * this, int fd)
{
	Enumerator * enumerator = NULL;
	IOWatchEv  * ev = NULL;

	enumerator = this->evList->CreateEnumerator(this->evList);
	while ((ev = enumerator->Enumerate(enumerator)))
	{
		if (ev->fd == fd)
			break;
	}
	enumerator->Destroy(enumerator);

	return ev;
}

static int iowatch_handle_fds(MyIOWatch * this)
{
	nfds_t  num;
	uint8_t events;
	IOWatchEv * ev = NULL;

	for ( num = 0 ; num < this->poll_cur_nfds ; num++ )
	{
		events = 0;
		if (this->poll_fds[num].revents & POLLERR)
			events |= IOWATCH_EXCEPT;
		if (this->poll_fds[num].revents & POLLIN)
			events |= IOWATCH_READ;
		if (this->poll_fds[num].revents & POLLOUT)
			events |= IOWATCH_WRITE;
		if (events == 0)
			continue;
		if ((ev = iowatch_find_ev_by_fd(this, this->poll_fds[num].fd)) == NULL)
			continue;

		ev->revents = events;
		if (ev->handle && ev->handle(ev) == false)
			return -1;
	}

	return 0;
}

static void iowatch_update_fds(MyIOWatch * this)
{
	Enumerator * enumerator = NULL;
	IOWatchEv  * ev = NULL;
	nfds_t nfds = 0;

	enumerator = this->evList->CreateEnumerator(this->evList);
	while ((ev = enumerator->Enumerate(enumerator)))
	{
		this->poll_fds[nfds].fd      = ev->fd;
		this->poll_fds[nfds].events  = 0;
		this->poll_fds[nfds].revents = 0;

		if (ev->events & IOWATCH_READ)
			this->poll_fds[nfds].events |= POLLIN;
		if (ev->events & IOWATCH_WRITE)
			this->poll_fds[nfds].events |= POLLOUT;

		nfds++;
	}
	enumerator->Destroy(enumerator);

	this->poll_cur_nfds = nfds;
}

static int iowatch_expand_fds(MyIOWatch * this, nfds_t nfds)
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

static int iowatch_dispatch(MyIOWatch * this, int timeout)
{
	int count;
	nfds_t nfds;

	if ((nfds = this->evList->GetCount(this->evList)) == 0)
		return -1;

	if (iowatch_expand_fds(this, nfds) < 0)
		return -2;

	iowatch_update_fds(this);

	if ((count = poll(this->poll_fds, this->poll_cur_nfds, timeout)) < 0)
		return -3;
	
	if (count == 0)
		return 0;

	if (iowatch_handle_fds(this) < 0)
		return -4;
	
	return count;
}

/*****************************************/
/** Public Method                       **/
/*****************************************/

static int M_Bind (IOWatch * self, IOWatchEv ev)
{
	MyIOWatch * this   = (MyIOWatch *) self;
	IOWatchEv * new_ev = NULL;

	if (ev.fd < 0 || ev.handle == NULL)
		return -1;
	
	if ((new_ev = malloc(sizeof(IOWatchEv))) == NULL)
		return -2;

	memcpy(new_ev, &ev, sizeof(IOWatchEv));
	this->evList->InsertLast(this->evList, new_ev);
	return 0;
}

static int M_Unbind(IOWatch * self, int fd)
{
	MyIOWatch  * this   = (MyIOWatch *) self;
	Enumerator * enumerator;
	IOWatchEv  * ev;

	if (fd < 0)
		return -1;

	enumerator = this->evList->CreateEnumerator(this->evList);
	while ((ev = enumerator->Enumerate(enumerator)))
	{
		if (ev->fd == fd)
		{
			this->evList->RemoveAt(this->evList, enumerator);
			enumerator->Destroy(enumerator);
			free(ev);
			return 0;
		}
	}
	enumerator->Destroy(enumerator);
	return -2;
}

static int M_Watch(IOWatch * self, int msec)
{
	MyIOWatch  * this   = (MyIOWatch *) self;
	
	return iowatch_dispatch(this, msec);
}

static void M_Reset(IOWatch * self)
{
	MyIOWatch * this = (MyIOWatch *) self;
	
	this->evList->RemoveAll(this->evList, free);
}

static void M_Destroy(IOWatch * self)
{
	MyIOWatch * this = (MyIOWatch *) self;

	if (this->evList)
	{
		this->evList->RemoveAll(this->evList, free);
		this->evList->Destroy(this->evList);
	}

	if (this->poll_fds)
		free(this->poll_fds);

	free(this);
}

/****************************************
 * Constructor Declaration
 ****************************************/

IOWatch * IOWatchCreate()
{
	MyIOWatch * this = NULL;
	IOWatch     self = {
		.Bind    = M_Bind,
		.Unbind  = M_Unbind,
		.Watch   = M_Watch,
		.Reset   = M_Reset,
		.Destroy = M_Destroy,
	};

	if ((this = malloc(sizeof(MyIOWatch))) == NULL)
		return NULL;

	memset(this, 0, sizeof(MyIOWatch));
	memcpy(&this->self, &self, sizeof(IOWatch));

	if ((this->evList = LinkedListCreate()) == NULL)
	{
		M_Destroy(&this->self);
		return NULL;
	}

	this->poll_fds      = NULL;
	this->poll_cur_nfds = 0;
	this->poll_max_nfds = 0;

	return &this->self;
}
