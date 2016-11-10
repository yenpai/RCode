#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "AppCtx.h"
#include "CLogger.h"
#include "EDEvtRawIO.h"

/******************************************************************************/

typedef struct MyServer MyServer;
struct MyServer {
	Server   self;
	EDLoop * loop;
	EDEvt  * sock_evt;
	int      sock_fd;
	uint8_t  sock_rx_buf[2048];
};

/******************************************************************************/
#define TAG "SERVER"
#define SERVER_PORT 11022

/******************************************************************************/

static int socket_setting_reuse(int sfd)
{
	int optval = 1;

	return setsockopt(
			sfd, SOL_SOCKET, SO_REUSEADDR,
			&optval , sizeof(int));
}

static int udp_socket_create()
{
	int sfd;

	if ((sfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		return -1;

	if (socket_setting_reuse(sfd) < 0)
	{
		close(sfd);
		return -2;
	}

	return sfd;
}

static int udp_socket_bind(int sfd)
{
	struct sockaddr_in addr;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port        = htons(SERVER_PORT);

	if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		return -1;

	return 0;
}

/******************************************************************************/

static void OnSocket(EDEvt * UNUSED(evt), EDEvtRawInfo * info, short revents)
{
	MyServer * this = info->pData;
	struct sockaddr_in peer;
	socklen_t peer_len;

	ssize_t rlen;

	if (revents & POLLERR)
	{
		LOG_E(TAG, "Socket Error!");
		return;
	}

	if (!(revents & POLLIN))
	{
		LOG_D(TAG, "Socket Event not POLLIN!");
		return;
	}

	if ((rlen = recvfrom(
					this->sock_fd, 
					this->sock_rx_buf, sizeof(this->sock_rx_buf), 0,
					(struct sockaddr *) &peer, &peer_len)) < 0)
	{
		LOG_E(TAG, "Socket Read Error!");
		return;
	}

	LOG_D("PKT", "Received from %s:%d", inet_ntoa(peer.sin_addr), ntohs(peer.sin_port));
}

/******************************************************************************/

static int M_Init(Server * self)
{
	MyServer * this = (MyServer *) self;
	
	if ((this->sock_fd = udp_socket_create()) < 0)
		return -1;
	
	if (udp_socket_bind(this->sock_fd) < 0)
	{
		close(this->sock_fd);
		this->sock_fd = -1;
		return -2;
	}

	return 0;
}

static int M_Listen(Server * self)
{
	int ret;
	MyServer  * this = (MyServer *) self;
	EDEvtInfo * info = EDEvtRawInfoSet(this->sock_fd, POLLIN, this, OnSocket);

	if ((ret = this->sock_evt->Subscribe(this->sock_evt, info)) < 0)
		return -1;

	if (this->loop->AddEvt(this->loop, this->sock_evt) < 0)
		return -2;

	return 0;
}

static void M_Destroy(Server * self)
{
	MyServer * this = (MyServer *) self;

	if (this == NULL)
		return;

	if (this->sock_evt)
		this->sock_evt->Destroy(this->sock_evt);

	if (this->sock_fd > 0)
		close(this->sock_fd);

	free(this);
}

/******************************************************************************/

Server * ServerCreate(AppCtx * app)
{
	MyServer * this = NULL;
	Server     self = {
		.Init    = M_Init,
		.Listen  = M_Listen,
		.Destroy = M_Destroy,
	};

	if ((this = malloc(sizeof(MyServer))) == NULL)
		return NULL;
	memset(this, 0,     sizeof(MyServer));
	memcpy(this, &self, sizeof(Server));

	this->loop = app->loop;

	if ((this->sock_evt = EDEvtRawCreate()) == NULL)
	{
		M_Destroy(&this->self);
		return NULL;
	}

	this->sock_fd = -1;

	return &this->self;
}
