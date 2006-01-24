#include "bench/Benchmark.h"
#include <dballe/dba_init.h>

#include <sys/times.h>
#include <unistd.h>

#define timing(fmt, ...) do { \
	struct tms curtms; \
	if (times(&curtms) == -1) \
		return dba_error_system("reading timing informations"); \
	fprintf(stderr, fmt, __VA_ARGS__); \
	fprintf(stderr, ": %.3f user, %.3f system, %.3f total\n", \
			(curtms.tms_utime - lasttms.tms_utime)/tps,\
			(curtms.tms_stime - lasttms.tms_stime)/tps,\
			((curtms.tms_utime - lasttms.tms_utime) + (curtms.tms_stime - lasttms.tms_stime))/tps); \
	lasttms = curtms; \
} while (0)

static void print_dba_error()
{
	const char* details = dba_error_get_details();
	fprintf(stderr, "Error %d (%s) while %s",
			dba_error_get_code(),
			dba_error_get_message(),
			dba_error_get_context());
	if (details == NULL)
		fputc('\n', stderr);
	else
		fprintf(stderr, ".  Details:\n%s\n", details);
}

int main(int argc, const char* argv[])
{
	dba_err err = DBA_OK;

	// We want predictable results
	srand(1);

	DBA_RUN_OR_GOTO(fail, dba_init());

	if (argc == 1)
		DBA_RUN_OR_GOTO(fail, Benchmark::root()->run());
	else
	{
		for (int i = 1; i < argc; i++)
			if (argv[i][0] == '/')
				DBA_RUN_OR_GOTO(fail, Benchmark::root()->run(argv[i]+1));
			else
				DBA_RUN_OR_GOTO(fail, Benchmark::root()->run(argv[i]));
	}

	dba_shutdown();

	return 0;

fail:
	print_dba_error();
	return 1;
}

/* vim:set ts=4 sw=4: */
