#ifndef APP_CTX_H_
#define APP_CTX_H_

#include "obtype.h"
#include "EDLoop.h"

/******************************************************************************/

typedef struct AppCtx AppCtx;
typedef struct Server Server;

/******************************************************************************/

struct AppCtx {
	EDLoop   * loop;
	EDEvt    * syssig;
	Server   * server;
};

/******************************************************************************/

struct Server {
	int  (* const Init)    (Server *);
	int  (* const Listen)  (Server *);
	void (* const Destroy) (Server *);
};

Server * ServerCreate(AppCtx *);

/******************************************************************************/

#endif
