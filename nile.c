#include "nile.h"
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <archive.h>
#include <openssl/md5.h>

#define MIN(x,y) (((x)<(y)) ? (x) : (y))
#define IN_MEMORY_THRESHOLD 1024*1024*20
#define HEADER_PADDING 8
#define LCS_BLOCKSIZE 512

const char MAGIC[] = "NILE1.0 ";

static off_t offtin(char *buf)
{
	off_t y;

	y=buf[7]&0x7F;
	y=y*256;y+=buf[6];
	y=y*256;y+=buf[5];
	y=y*256;y+=buf[4];
	y=y*256;y+=buf[3];
	y=y*256;y+=buf[2];
	y=y*256;y+=buf[1];
	y=y*256;y+=buf[0];

	if(buf[7]&0x80) y=-y;

	return y;
}

static void offtout(off_t x, char *buf)
{
	off_t y;

	if(x<0) y=-x; else y=x;

	buf[0]=y%256;y-=buf[0];
	y=y/256;buf[1]=y%256;y-=buf[1];
	y=y/256;buf[2]=y%256;y-=buf[2];
	y=y/256;buf[3]=y%256;y-=buf[3];
	y=y/256;buf[4]=y%256;y-=buf[4];
	y=y/256;buf[5]=y%256;y-=buf[5];
	y=y/256;buf[6]=y%256;y-=buf[6];
	y=y/256;buf[7]=y%256;

	if(x<0) buf[7]|=0x80;
}

static int
buildhash(char *md5sum, FILE *f) {
	char buf[BUFSIZ];
	int size = 0;
	MD5_CTX md5;

	MD5_Init(&md5);
	while((size = fread(buf, 1, sizeof(buf), f)) > 0) {
		MD5_Update(&md5, buf, size);
	}
	MD5_Final((unsigned char *)md5sum, &md5);
	rewind(f);
	return 0;
}

static int
mapfile(FILE *f, char **map) {
	struct stat stat;
	int fd = fileno(f);

	if(fstat(fd, &stat) == -1) {
		perror("fstat");
		return -1;
	}

	*map = mmap(NULL, stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if(*map == MAP_FAILED) {
		perror("mmap");
		return -1;
	}
	return stat.st_size;
}

FILE *
uncompress(FILE *f, FILE* out, enum Options options) {
	int fdtmp = -1, i, size;
	struct archive *a = NULL;
	struct archive_entry *entry;
	FILE *rv = f, *ftmp;
	char buf[BUFSIZ], tmpfile[] = "/tmp/nile.XXXXXX", nfilter = 0;

	if(!(options & NILE_DECOMPRESS))
		goto uncompress_cleanup;

	a = archive_read_new();
	archive_read_support_filter_all(a);
	archive_read_support_format_raw(a);
	if(archive_read_open_FILE(a, f) != ARCHIVE_OK ||
	   archive_read_next_header(a, &entry) != ARCHIVE_OK) {
		fprintf(stderr, "Warning: decompression failed while open\n");
		goto uncompress_cleanup;
	}
	if((nfilter = archive_filter_count(a)) == 0) {
		goto uncompress_cleanup;
	}
	if((fdtmp = mkstemp(tmpfile)) == -1) {
		perror("mkstemp");
		goto uncompress_cleanup;
	}

	while((size = archive_read_data(a, buf, sizeof(buf))) > 0) {
		write(fdtmp, buf, size);
	}
	if (size < 0) {
		fprintf(stderr, "Warning: decompression failed read\n");
		goto uncompress_cleanup;
	}
	if((ftmp = fdopen(fdtmp, "r")) == NULL) {
		perror("fdopen");
		goto uncompress_cleanup;
	}
	rv = ftmp;

uncompress_cleanup:
	rewind(f);
	if(out != NULL) {
		for(i = 0; i < nfilter; i++) {
			fputc(archive_filter_code(a, i), out);
		}
		for(i = i % HEADER_PADDING; i < HEADER_PADDING; i++) {
			fputc(0, out);
		}
	}
	if(fdtmp > 0)
		close(fdtmp);
	if(a != NULL) {
		archive_read_free(a);
	}
	return rv;
}

static int
lcs(const char *olddata, off_t *oldoff, size_t oldsize, const char *newdata, off_t *newoff, size_t newsize, off_t *lastSkip, FILE *output) {
	off_t oi, ni, bufi, lastni, lastoi, no = *newoff, oo = *oldoff, skip, baseSkip = *lastSkip, end = 0;
	int map[LCS_BLOCKSIZE][LCS_BLOCKSIZE] = { { 0 } };
	char buf[LCS_BLOCKSIZE * 17], *p = NULL;

	// Build LCS Map
	for(oi = 0; oi < oldsize - oo && oi < LCS_BLOCKSIZE; oi++) {
		for(ni = 0; ni < newsize - no && ni < LCS_BLOCKSIZE; ni++) {
			if(olddata[oo + oi] == newdata[no + ni]) {
				map[oi][ni] = oi == 0 || ni == 0 ? 1 : map[oi-1][ni-1] + 1;
			}
			else if(map[oi][ni-1] < map[oi-1][ni]) {
				map[oi][ni] = oi == 0 ? 0 : map[oi-1][ni];
			}
			else {
				map[oi][ni] = ni == 0 ? 0 : map[oi][ni-1];
			}
		}
	}

	// Check if we hit boundarys
	end = oi == oldsize - oo || ni == newsize - no;

	// Tell caller about the new maximal offsets;
	*oldoff = oo + oi;
	*newoff = no + ni;

	for(skip = 0, lastni = --ni, lastoi = --oi, bufi = sizeof(buf);oi >= 0 && ni >= 0;) {
		// LEFT
		if(ni != 0 && map[oi][ni-1] == map[oi][ni]) {
			buf[--bufi] = newdata[no + ni];
			ni--;
		}
		// UP
		else if(oi != 0 && map[oi-1][ni] == map[oi][ni]) {
			oi--;
		}
		// DIAGONAL
		else {
			// Set Offset to last common sequence
			if(oldoff != NULL) {
				*oldoff = oo + oi;
				*newoff = no + ni;
				newoff = oldoff = NULL;
			}
			// This is the first common character after
			// differences, so set new diff header
			if(skip == 0 && (lastni - ni != 0 || lastoi - oi != 0)) {
				bufi-=8;
				offtout(lastni - ni, &buf[bufi]);
				bufi-=8;
				offtout(lastoi - oi, &buf[bufi]);
				bufi-=8;
				p=&buf[bufi];
			}
			ni--;oi--;
			skip++;
			lastni = ni;
			lastoi = oi;
			continue;
		}

		if(p != NULL) {
			offtout(skip, p);
			p = NULL;
		}
		// tell caller how much data we skipped
		if(lastSkip) {
			*lastSkip += skip;
			lastSkip = NULL;
		}
		skip = 0;
	}
	// Set skip for first header in this round
	if(p != NULL)
		offtout(skip+baseSkip, p);
	// tell caller how much data we skipped if we haven't done
	// that already.
	if(lastSkip)
		*lastSkip += skip;
	fwrite(&buf[bufi], sizeof(buf) - bufi, 1, output);
	return end;
}

int
nileDiff(FILE *oldfile, FILE *newfile, FILE *output, enum Options options) {
	int rv = 1;
	off_t oldoff = 0, newoff = 0, lastSkip = 0;
	size_t oldsize, newsize;
	char *olddata = NULL, *newdata = NULL, md5sum[MD5_DIGEST_LENGTH] = { 0 }, header[8];

	if((oldsize = mapfile(oldfile, &olddata)) == -1)
		rv = 1;
	else if((newsize = mapfile(newfile, &newdata)) == -1)
		rv = 1;

	fputs(MAGIC, output);

	// newfile: uncompress and build md5sum
	if((newfile = uncompress(newfile, output, options)) == NULL)
		goto diff_cleanup;
	buildhash(md5sum, newfile);
	fwrite(md5sum, sizeof(md5sum), 1, output);

	// oldfile: uncompress and build md5sum
	if((oldfile = uncompress(oldfile, NULL, options)) == NULL)
		goto diff_cleanup;
	buildhash(md5sum, oldfile);
	fwrite(md5sum, sizeof(md5sum), 1, output);

	// Building Diff
	for(oldoff = newoff = 0; oldoff < oldsize && newoff < newsize;) {
		if(lcs(olddata, &oldoff, oldsize, newdata, &newoff, newsize, &lastSkip, output)) break;
	}
	// write trailing diff
	offtout(lastSkip, header);
	fwrite(header, sizeof(header), 1, output);
	offtout(oldsize - oldoff, header);
	fwrite(header, sizeof(header), 1, output);
	offtout(newsize - newoff, header);
	fwrite(header, sizeof(header), 1, output);
	fwrite(&newdata[newoff], newsize - newoff, 1, output);

diff_cleanup:
	if(oldsize >= 0)
		munmap(olddata, oldsize);
	if(newsize >= 0)
		munmap(newdata, newsize);
	return rv;
}

int
nilePatch(FILE *oldfile, FILE *patch, FILE *output, enum Options options) {
	int rv = 0;
	char difference[3*8], header[8], buf[BUFSIZ];
	char oldmd5[MD5_DIGEST_LENGTH] = { 0 }, newmd5[MD5_DIGEST_LENGTH] = { 0 };
	int size;

	off_t skip, oldsize, newsize;

	if(fread(header, sizeof(header), 1, patch) != 1 ||
			memcmp(header, MAGIC, sizeof(header)) != 0) {
		fprintf(stderr, "patchfile: invalid magic\n");
		goto patch_cleanup;
	}

	if(fread(header, sizeof(header), 1, patch) != 1) {
		fprintf(stderr, "patchfile: invalid header\n");
		goto patch_cleanup;
	}

	if(fread(oldmd5, sizeof(oldmd5), 1, patch) != 1) {
		fprintf(stderr, "patchfile: invalid header\n");
		goto patch_cleanup;
	}
	if(fread(newmd5, sizeof(newmd5), 1, patch) != 1) {
		fprintf(stderr, "patchfile: invalid header\n");
		goto patch_cleanup;
	}

	while((size = fread(difference, sizeof(difference), 1, patch)) == 1) {
		skip = offtin(difference);
		oldsize = offtin(difference+8);
		newsize = offtin(difference+16);
		fprintf(stderr, "skip %li oldsize: %li newsize: %li\n", skip, oldsize,
				newsize);

		while((size = fread(buf, 1, MIN(sizeof(buf), skip), oldfile)) > 0) {
			fwrite(buf, size, 1, output);
			skip -= size;
		}

		fseek(oldfile, oldsize, SEEK_CUR);

		while((size = fread(buf, 1, MIN(sizeof(buf), newsize), patch)) > 0) {
			fwrite(buf, size, 1, output);
			newsize -= size;
		}
	}
	if(size < 0) {
		perror("patchfile");
		rv = 1;
	}
	else if(size != 0) {
		fprintf(stderr, "patchfile: premature end of file %i\n", size);
		rv = 1;
	}
patch_cleanup:
	return rv;
}


int
nileMerge(FILE *oldpatch, FILE *newpatch, FILE *output, enum Options options) {
	return 1;
}
