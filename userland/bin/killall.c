/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2000 Peter Wemm <peter@FreeBSD.org>
 * Copyright (c) 2000 Paul Saab <ps@FreeBSD.org>
 * All rights reserved.
 *
 * Ported to SzpontOS from FreeBSD usr.bin/killall/
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <signal.h>
#include <regex.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>

#ifndef MAXCOMLEN
#define MAXCOMLEN 63
#endif

#ifndef SZOMB
#define SZOMB 1
#endif

static void __dead2
usage(void)
{
	fprintf(stderr, "usage: killall [-delmqsvz] [-help] [-I]\n");
	fprintf(stderr, "               [-u user] [-c cmd] [-SIGNAL] [cmd]...\n");
	fprintf(stderr, "At least one option or argument to specify processes must be given.\n");
	exit(1);
}

static void
printsig(FILE *fp)
{
	const char *const *p;
	int cnt;
	int offset = 0;

	for (cnt = NSIG, p = sys_signame + 1; --cnt; ++p) {
		if (*p == NULL || **p == '\0')
			continue;
		offset += fprintf(fp, "%s ", *p);
		if (offset >= 75 && cnt > 1) {
			offset = 0;
			fprintf(fp, "\n");
		}
	}
	fprintf(fp, "\n");
}

static void
nosig(const char *name)
{
	warnx("unknown signal %s; valid signals:", name);
	printsig(stderr);
	exit(1);
}

int
main(int ac, char **av)
{
	char **saved_av;
	struct passwd *pw;
	regex_t rgx;
	regmatch_t pmatch;
	int i, j;
	char first;
	char *user = NULL;
	char *cmd = NULL;
	int qflag = 0;
	int vflag = 0;
	int sflag = 0;
	int dflag = 0;
	int eflag = 0;
	int Iflag = 0;
	int mflag = 0;
	int zflag = 0;
	(void)eflag;
	uid_t uid = 0;
	pid_t mypid;
	char thiscmd[MAXCOMLEN + 1];
	pid_t thispid;
	uid_t thisuid;
	int sig = SIGTERM;
	const char *const *p;
	char *ep;
	int errors = 0;
	int nprocs;
	int matched;
	int killed = 0;

	av++;
	ac--;

	while (ac > 0) {
		if (strcmp(*av, "-l") == 0) {
			printsig(stdout);
			exit(0);
		}
		if (strcmp(*av, "-help") == 0 || strcmp(*av, "--help") == 0)
			usage();
		if (**av == '-') {
			++*av;
			switch (**av) {
			case 'u':
				++*av;
				if (**av == '\0') {
					++av;
					--ac;
				}
				if (*av == NULL)
					errx(1, "must specify user");
				user = *av;
				break;
			case 'c':
				++*av;
				if (**av == '\0') {
					++av;
					--ac;
				}
				if (*av == NULL)
					errx(1, "must specify procname");
				cmd = *av;
				break;
			case 'q':
				qflag++;
				break;
			case 'v':
				vflag++;
				break;
			case 's':
				sflag++;
				break;
			case 'd':
				dflag++;
				break;
			case 'e':
				eflag++;
				break;
			case 'm':
				mflag++;
				break;
			case 'z':
				zflag++;
				break;
			case 'I':
				Iflag++;
				break;
			default:
				saved_av = av;
				if (isalpha((unsigned char)**av)) {
					if (strncasecmp(*av, "SIG", 3) == 0)
						*av += 3;
					for (sig = NSIG, p = sys_signame + 1; --sig; ++p) {
						if (*p != NULL && strcasecmp(*p, *av) == 0) {
							sig = p - sys_signame;
							break;
						}
					}
					if (!sig) {
						if (**saved_av == 'I') {
							av = saved_av;
							Iflag = 1;
							break;
						} else
							nosig(*av);
					}
				} else if (isdigit((unsigned char)**av)) {
					sig = (int)strtol(*av, &ep, 10);
					if (!*av || *ep)
						errx(1, "illegal signal number: %s", *av);
					if (sig <= 0 || sig >= NSIG)
						nosig(*av);
				} else
					nosig(*av);
			}
			++av;
			--ac;
		} else {
			break;
		}
	}

	if (user == NULL && cmd == NULL && ac == 0)
		usage();

	if (user) {
		uid = (uid_t)strtol(user, &ep, 10);
		if (*user == '\0' || *ep != '\0') { /* was it a name? */
			pw = getpwnam(user);
			if (pw == NULL)
				errx(1, "user %s does not exist", user);
			uid = pw->pw_uid;
			if (dflag)
				printf("uid:%d\n", uid);
		}
	} else {
		uid = getuid();
		if (uid != 0) {
			pw = getpwuid(uid);
			if (pw)
				user = pw->pw_name;
			if (dflag)
				printf("uid:%d\n", uid);
		}
	}

	proc_info_t procs[256];
	nprocs = getprocs(procs, 256);
	if (nprocs < 0)
		err(1, "could not retrieve process list");

	if (dflag)
		printf("nprocs %d\n", nprocs);
	mypid = getpid();

	for (i = 0; i < nprocs; i++) {
		if (procs[i].state == SZOMB && !zflag)
			continue;
		thispid = procs[i].pid;
		strlcpy(thiscmd, procs[i].name, sizeof(thiscmd));
		thisuid = procs[i].uid;

		if (thispid == mypid || thispid <= 1)
			continue;

		matched = 1;
		if (user) {
			if (thisuid != uid)
				matched = 0;
		}

		/* Extract basename if command path is stored (e.g. /bin/httpd -> httpd) */
		const char *base = strrchr(thiscmd, '/');
		base = base ? base + 1 : thiscmd;

		if (cmd) {
			if (mflag) {
				if (regcomp(&rgx, cmd, REG_EXTENDED | REG_NOSUB) != 0) {
					mflag = 0;
					warnx("%s: illegal regexp", cmd);
				}
			}
			if (mflag) {
				pmatch.rm_so = 0;
				pmatch.rm_eo = strlen(thiscmd);
				if (regexec(&rgx, thiscmd, 0, &pmatch, 0) != 0 &&
				    regexec(&rgx, base, 0, &pmatch, 0) != 0)
					matched = 0;
				regfree(&rgx);
			} else {
				if (strncmp(thiscmd, cmd, MAXCOMLEN) != 0 &&
				    strncmp(base, cmd, MAXCOMLEN) != 0)
					matched = 0;
			}
		}

		if (matched == 0)
			continue;

		if (ac > 0)
			matched = 0;

		for (j = 0; j < ac; j++) {
			if (mflag) {
				if (regcomp(&rgx, av[j], REG_EXTENDED | REG_NOSUB) != 0) {
					mflag = 0;
					warnx("%s: illegal regexp", av[j]);
				}
			}
			if (mflag) {
				pmatch.rm_so = 0;
				pmatch.rm_eo = strlen(thiscmd);
				if (regexec(&rgx, thiscmd, 0, &pmatch, 0) == 0 ||
				    regexec(&rgx, base, 0, &pmatch, 0) == 0)
					matched = 1;
				regfree(&rgx);
			} else {
				if (strcmp(thiscmd, av[j]) == 0 || strcmp(base, av[j]) == 0)
					matched = 1;
			}
			if (matched)
				break;
		}

		if (matched != 0 && Iflag) {
			printf("Send signal %d to %s (pid %d uid %d)? ",
				sig, thiscmd, thispid, thisuid);
			fflush(stdout);
			int ch = getchar();
			first = (char)ch;
			while (ch != '\n' && ch != EOF)
				ch = getchar();
			if (first != 'y' && first != 'Y')
				matched = 0;
		}

		if (matched == 0)
			continue;

		if (dflag)
			printf("sig:%d, cmd:%s, pid:%d, uid:%d\n",
			    sig, thiscmd, thispid, thisuid);

		if (vflag || sflag)
			printf("kill -%s %d\n", sys_signame[sig] ? sys_signame[sig] : "unknown", thispid);

		killed++;
		if (!dflag && !sflag) {
			if (kill(thispid, sig) < 0) {
				warn("warning: kill -%s %d",
				    sys_signame[sig] ? sys_signame[sig] : "unknown", thispid);
				errors = 1;
			}
		}
	}

	if (killed == 0) {
		if (!qflag)
			fprintf(stderr, "No matching processes %swere found\n",
			    user != NULL ? "belonging to you " : "");
		errors = 1;
	}

	return (errors);
}
