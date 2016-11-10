#ifndef CLOGGER_H_
#define CLOGGER_H_

#include <stdio.h>
#include <string.h>
#include <syslog.h>

/****************************************
 * Interface Definition
 ****************************************/

typedef struct CLogger     CLogger;
typedef struct CLoggerInfo CLoggerInfo;

typedef void (*CLoggerOutput) (CLoggerInfo, const char *, ...);

struct CLogger {
	FILE        * stream;
	CLoggerOutput output;
};

struct CLoggerInfo {
	const int level;
	const char * tag;
	const char * file;
	const char * func;
	unsigned int line;
};

/****************************************
 * Constructor Declaration
 ****************************************/

/****************************************
 * Function Declaration
 ****************************************/
extern CLogger gLogger;

#define _CLOGGER_FILENAME (strrchr(__FILE__,'/') ? strrchr(__FILE__,'/')+1 : __FILE__ )
#define _CLOGGER_OUTPUT(level, tag, fmt, ...) \
	gLogger.output(\
			(CLoggerInfo){level, tag, _CLOGGER_FILENAME, __FUNCTION__, __LINE__}, \
		   	fmt, ## __VA_ARGS__)

#define LOG_I(tag, fmt, ...) _CLOGGER_OUTPUT(LOG_INFO,    tag, fmt, ## __VA_ARGS__)
#define LOG_E(tag, fmt, ...) _CLOGGER_OUTPUT(LOG_ERR,     tag, fmt, ## __VA_ARGS__)
#define LOG_W(tag, fmt, ...) _CLOGGER_OUTPUT(LOG_WARNING, tag, fmt, ## __VA_ARGS__)
#define LOG_D(tag, fmt, ...) _CLOGGER_OUTPUT(LOG_DEBUG,   tag, fmt, ## __VA_ARGS__)
#define LOG_V(tag, fmt, ...) _CLOGGER_OUTPUT(LOG_NOTICE,  tag, fmt, ## __VA_ARGS__)

#endif
