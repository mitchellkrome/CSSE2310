CC=gcc
CFLAGS=-Wall -Wextra -pedantic -std=gnu99 -g -O0
LDFLAGS=-lm

HUBSRCS=clubhub.c utils.c
HUBOBJS=$(patsubst %.c, %.o, $(HUBSRCS))

CLUBSRCS=clubber.c utils.c
CLUBOBJS=$(patsubst %.c, %.o, $(CLUBSRCS))

all: clubhub clubber

clubhub: $(HUBOBJS)
	$(CC) -o clubhub $(CFLAGS) $(HUBOBJS) $(LDFLAGS)

clubber: $(CLUBOBJS)
	$(CC) -o clubber $(CFLAGS) $(CLUBOBJS) $(LDFLAGS)

clean:
	rm *.o clubber clubhub
