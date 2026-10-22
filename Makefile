CC ?= gcc
CFLAGS ?= -Wall -Wextra

.PHONY: test clean

test_cachedb: test_cachedb.c sample-database.c
	$(CC) $(CFLAGS) -o test_cachedb test_cachedb.c

test: test_cachedb
	./test_cachedb

clean:
	rm -f test_cachedb
