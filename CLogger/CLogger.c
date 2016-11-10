#include <stdarg.h>
#include "CLogger.h"

/****************************************
 * Public Method Definition
 ****************************************/
char * clogger_level_to_string(int level)
{
	switch (level)
	{
		case LOG_NOTICE:
			return "NOTICE";
		case LOG_INFO:
			return "INFO";
		case LOG_DEBUG:
			return "DEBUG";
		case LOG_WARNING:
			return "WARN";
		case LOG_ERR:
			return "ERROR";
	}

	return "";
}

/****************************************
 * Private Function Definition
 ****************************************/

static void clogger_default_output(CLoggerInfo info, const char *fmt, ...)
{
	va_list args;

	if (gLogger.stream)
	{

		fprintf(gLogger.stream, "[%s][%s][%s:%u][%s] ", 
				info.tag, info.file, 
				info.func, info.line,
				clogger_level_to_string(info.level));

		va_start(args, fmt);
		vfprintf(gLogger.stream, fmt, args);
		va_end(args);

		fprintf(gLogger.stream, "\n");
		fflush(gLogger.stream);
	}
}

 
/****************************************
 * Constructor Definition
 ****************************************/

CLogger gLogger = { 
	.stream = NULL,
	.output = clogger_default_output,
};

