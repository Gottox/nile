/*
 * nile.h
 * Copyright (C) 2014 tox <tox@rootkit>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef NILE_H
#define NILE_H

#include <stdio.h>

enum Options {
	NILE_DECOMPRESS = 1 << 0,
	NILE_RECOMPRESS = 1 << 1,
};

extern int nileDiff(FILE *oldfile, FILE *newfile, FILE *output, enum Options options);
extern int nilePatch(FILE *oldfile, FILE *patch, FILE *output, enum Options options);
extern int nileMerge(FILE *oldpatch, FILE *newpatch, FILE *output, enum Options options);

#endif /* !NILE_H */
