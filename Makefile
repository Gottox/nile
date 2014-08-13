all: nile
nile: nile.c

install:
	${INSTALL_PROGRAM} bsdiff bspatch ${PREFIX}/bin
	${INSTALL_MAN} bsdiff.1 bspatch.1 ${PREFIX}/man/man1
