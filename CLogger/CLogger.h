#ifndef CLOGGER_H_
#define CLOGGER_H_

#include <stdio.h>
#include <string.h>

/****************************************
 * Interface Definition
 ****************************************/

typedef struct CLogger     CLogger;
typedef enum   CLoggerLv   CLoggerLv;
typedef struct CLoggerInfo CLoggerInfo;

typedef void (*CLoggerOutput) (CLoggerInfo, const char *, ...);

#define CLOGGER_SILENT	0x00
#define CLOGGER_ALL		0xFF

enum CLoggerLv {
	CLOGGER_INFO    = 0x01,
	CLOGGER_ERROR   = 0x02,
	CLOGGER_WARN    = 0x04,
	CLOGGER_DEBUG   = 0x08,
	CLOGGER_VERBOSE = 0x10,
};

struct CLogger {
	FILE        * stream;
	unsigned int  levels;
	CLoggerOutput output;
};

struct CLoggerInfo {
	const CLoggerLv level;
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

#define LOG_I(tag, fmt, ...) _CLOGGER_OUTPUT(CLOGGER_INFO,    tag, fmt, ## __VA_ARGS__)
#define LOG_E(tag, fmt, ...) _CLOGGER_OUTPUT(CLOGGER_ERROR,   tag, fmt, ## __VA_ARGS__)
#define LOG_W(tag, fmt, ...) _CLOGGER_OUTPUT(CLOGGER_WARN,    tag, fmt, ## __VA_ARGS__)
#define LOG_D(tag, fmt, ...) _CLOGGER_OUTPUT(CLOGGER_DEBUG,   tag, fmt, ## __VA_ARGS__)
#define LOG_V(tag, fmt, ...) _CLOGGER_OUTPUT(CLOGGER_VERBOSE, tag, fmt, ## __VA_ARGS__)

#endif
