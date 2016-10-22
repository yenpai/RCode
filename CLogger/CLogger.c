#include <stdarg.h>
#include "CLogger.h"

/****************************************
 * Private Interface Definition
 ****************************************/

/****************************************
 * Private Function Definition
 ****************************************/

static void clogger_default_output(CLoggerInfo info, const char *fmt, ...)
{
	va_list args;

	if (gLogger.stream && (info.level & gLogger.levels))
	{

		fprintf(gLogger.stream, "[%s][%s][%s:%u] ", 
				info.tag, info.file, info.func, info.line);

		va_start(args, fmt);
		vfprintf(gLogger.stream, fmt, args);
		va_end(args);

		fprintf(gLogger.stream, "\n");
		fflush(gLogger.stream);
	}
}

/****************************************
 * Public Method Definition
 ****************************************/
 
/****************************************
 * Constructor Definition
 ****************************************/

CLogger gLogger = { 
	.stream = NULL,
	.levels = CLOGGER_ALL,
	.output = clogger_default_output,
};

