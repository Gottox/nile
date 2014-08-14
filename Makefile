LDFLAGS+= -larchive -lcrypto
CFLAGS+= -Wall -O0 -g

all: nile
clean:
	rm nile
nile: main.c nile.c
.PHONY: clean all
