#include <err.h>
#include <libgen.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>

#define TAILCMD "tail -f "
#define HDRFMT "\n==> %s <==\n\n"

static int firstrun = 1;
static pthread_t lastthr;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

static char*
tailcmd(char *fp)
{
	char *c, *cmd;
	size_t tlen, flen;

	tlen = strlen(TAILCMD);
	flen = strlen(fp);

	if (!(cmd = malloc(tlen + flen + 1)))
		err(EXIT_FAILURE, "malloc failed");

	c = stpncpy(cmd, TAILCMD, tlen);
	c = stpncpy(c, fp, flen);
	*c = '\0';

	return cmd;
}

static void*
tail(void *arg)
{
	int ret;
	FILE *pipe;
	size_t llen;
	ssize_t read;
	pthread_t thr;
	char *fp, *fmt, *cmd, *line, *name;

	fp = arg;
	cmd = tailcmd(fp);
	if (!(pipe = popen(cmd, "r")))
		err(EXIT_FAILURE, "popen failed");

	line = NULL;
	llen = 0;

	name = basename(dirname(fp));
	while ((read = getline(&line, &llen, pipe)) != -1) {
		if (pthread_mutex_lock(&mtx))
			err(EXIT_FAILURE, "pthread_mutex_lock failed");

		fmt = HDRFMT;
		if (firstrun) {
			fmt += 1; /* skip newline */
			firstrun = 0;
		}

		thr = pthread_self();
		if (firstrun || !pthread_equal(thr, lastthr))
			printf(fmt, name);
		lastthr = thr;

		printf("%s", line);
		fflush(stdout);

		if (pthread_mutex_unlock(&mtx))
			err(EXIT_FAILURE, "pthread_mutex_unlock failed");
	}

	free(cmd);
	if ((ret = pclose(pipe)) == -1)
		errx(EXIT_FAILURE, "pclose failed");
	else if (ret != EXIT_SUCCESS)
		exit(ret);

	return NULL;
}

int
main(int argc, char **argv)
{
	int i, nthrs;
	pthread_t *thrs;

	if (argc <= 1) {
		fprintf(stderr, "USAGE: %s FILE...\n", argv[0]);
		return EXIT_FAILURE;
	}

	nthrs = argc - 1;
	if (!(thrs = malloc(sizeof(pthread_t*) * nthrs)))
		err(EXIT_FAILURE, "malloc failed");

	for (i = 0; i < nthrs; i++)
		if (pthread_create(&thrs[i], NULL, tail, argv[i + 1]))
			err(EXIT_FAILURE, "pthread_create failed");
	for (i = 0; i < nthrs; i++)
		if (pthread_join(thrs[i], NULL))
			err(EXIT_FAILURE, "pthread_join failed");

	return EXIT_SUCCESS;
}