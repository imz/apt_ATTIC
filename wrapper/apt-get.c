/*
 * APT-RPM wrapper for apt-get to wait for all its processes to finish
 *
 * Copyright (c) 2022 Vitaly Chikunov <vt@altlinux.org>
 * SPDX-License-Identifier: GPL-2.0-only
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern const char *__progname;

static void set_signals(const sighandler_t handler)
{
	signal(SIGHUP,  handler);
	signal(SIGINT,  handler);
	signal(SIGPIPE, handler);
	signal(SIGQUIT, handler);
	signal(SIGTTIN, handler);
	signal(SIGTTOU, handler);
}

int main(int argc, char **argv)
{
	if (!argc)
		errx(1, "argv is empty");

	/*
	 * Provide post-update command API.
	 * Script specified in POST_UPDATE_SCRIPT will be run
	 * after everything apt-get specific is finished. Thus,
	 * children can add commands there.
	 */
	char cmd_filename[] = "/tmp/post_update.XXXXXX";
	int cmd_fd = mkostemp(cmd_filename, O_CLOEXEC);
	if (cmd_fd != -1) {
		char pid_text[sizeof(intmax_t) * 3];
		if ((size_t)snprintf(pid_text, sizeof(pid_text), "%jd", (intmax_t)getpid()) < sizeof(pid_text)) {
			/*
			 * PID may be useful but not necessary for child processes
			 * to check availability of post-update launcher.
			 */
			setenv("POST_UPDATE_PID", pid_text, 1);
		}
		/* Child should write bash commands there. */
		setenv("POST_UPDATE_SCRIPT", cmd_filename, 1);
	} else {
		warn("mkostemp %s", cmd_filename);
	}

	/*
	 * Do not create pgrp nor tpgrp, taking what bash is created for us.
	 * In that case signals will go naturally to our children too, and we
	 * need just to ignore several signals to continue job of waiting for
	 * their completion.
	 */
	set_signals(SIG_IGN);

#ifndef WRAP
	/* Generic wrapper. */
	argc--;
	argv++;
	if (!argc)
		errx(1, "need arguments");
#else
	/* Wrap a particular binary. */
	argv[0] = WRAP;
#endif
	pid_t pid = fork();
	if (pid == -1)
		err(1, "fork");
	if (!pid) {
		set_signals(SIG_DFL);
		execvp(argv[0], argv);
		err(127, "exec: %s", argv[0]); /* command not found */
	}

	/* Reap direct child (such as apt-get) and simulate bash exit code. */
	int ret = 1;
	int wstatus = 0;
	pid_t wpid;
	while ((wpid = waitpid(pid, &wstatus, 0)) == -1 && errno == EINTR)
		;
	if (wpid == -1)
		warn("waitpid");
	else if (WIFEXITED(wstatus))
		ret = WEXITSTATUS(wstatus);
	else if (WIFSIGNALED(wstatus))
		ret = 128 + WTERMSIG(wstatus);

	/* Run post-update script if available. */
	if (cmd_fd != -1) {
		struct stat st;
		if (!fstat(cmd_fd, &st) && st.st_size > 0) {
			pid = fork();
			if (pid == -1) {
				warn("fork");
			} else {
				if (!pid) {
					char *av[3] = {"bash", cmd_filename, NULL};
					set_signals(SIG_DFL);
#ifdef DEBUG
					fprintf(stderr, "%s: Run: %s %s\n", __progname, av[0], av[1]);
#endif
					execvp(av[0], av);
					err(1, "exec: %s", av[0]);
				}
				while ((wpid = waitpid(pid, NULL, 0)) == -1 && errno == EINTR)
					;
				if (wpid == -1)
					warn("waitpid");
			}
		}
		unlink(cmd_filename);
	}

	return ret;
}
