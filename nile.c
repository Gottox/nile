/*
 * nile.c
 * Copyright (C) 2014 tox <tox@rootkit>
 *
 * Distributed under terms of the MIT license.
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "nile.h"

int main(int argc, char *argv[]) {
	int fdn = 0, rv = EXIT_FAILURE;
	FILE *f[3] = { NULL, NULL, stdout };
	int (*action)(FILE*,FILE*,FILE*) = NULL;
	while (getopt(argc, argv, "dDpm") != -1) {
		switch (optopt) {
		case 'd':
			if(action != 0) goto usage;
			action = nileDiff;
			break;
		case 'D':
			if(action != 0) goto usage;
			action = nileDeepDiff;
			break;
		case 'p':
			if(action != 0) goto usage;
			action = nilePatch;
			break;
		case 'm':
			if(action != 0) goto usage;
			action = nileMerge;
			break;
		default: /* '?' */
usage:
			fprintf(stderr, 
			                "Usage: %s -d <oldfile> <newfile> [patch]\n"
			                "       %s -D <oldfile> <newfile> [patch]\n"
			                "       %s -p <oldfile> <patch> [newfile]\n"
			                "       %s -m <patch1 patch2> [newpatch]\n",
			                argv[0], argv[0], argv[0], argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	if(action == NULL || argc - optind < 2 || argc - optind > 3)
		goto usage;

	// Open files, first two arguments are for reading, third is for writing
	for(; optind+fdn < argc; fdn++) {
		if((f[fdn]=fopen(argv[optind+fdn], fdn != 2 ? "r" : "w")) == NULL) {
			perror(argv[optind+fdn]);
			goto cleanup;
		}
	}

	rv = action(f[0], f[1], f[2]) ? EXIT_SUCCESS : EXIT_FAILURE;

cleanup:
	for(fdn--; fdn>=0; fdn--) {
		fclose(f[fdn]);
	}
	return rv;
}
