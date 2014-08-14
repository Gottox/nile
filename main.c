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
	char opt;
	enum Options options = 0;
	int fdn = 0, rv = EXIT_FAILURE;
	FILE *f[3] = { NULL, NULL, stdout };
	int (*action)(FILE*,FILE*,FILE*,enum Options) = NULL;

	while ((opt = getopt(argc, argv, "DPMdr")) != -1) {
		switch (opt) {
		case 'D':
			if(action) goto usage;
			action = nileDiff;
			break;
		case 'P':
			if(action) goto usage;
			action = nilePatch;
			break;
		case 'M':
			if(action) goto usage;
			action = nileMerge;
			break;
		case 'd':
			options |= NILE_DECOMPRESS;
			break;
		case 'r':
			options |= NILE_RECOMPRESS;
			break;
		default: /* '?' */
usage:
			fprintf(stderr, 
			                "Usage: %s -D [-dr] <oldfile> <newfile> [patch]\n"
			                "       %s -P [-r] <oldfile> <patch> [newfile]\n"
			                "       %s -M <patch1 patch2> [newpatch]\n",
			                argv[0], argv[0], argv[0]);
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

	rv = action(f[0], f[1], f[2], options);

cleanup:
	for(fdn--; fdn>=0; fdn--)
		if(fclose(f[fdn])) {
			perror(argv[optind+fdn]);
			rv = EXIT_FAILURE;
		}
	return rv;
}
