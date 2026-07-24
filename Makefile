CC      := gcc
CFLAGS  := -std=gnu11 -Wall -Wextra -g
SANFLAGS := -fsanitize=address,undefined

COMMON  := mm.c memlib.c

.PHONY: all clean test test-san

all: driver mtest

driver: driver.c $(COMMON) mm.h memlib.h
	$(CC) $(CFLAGS) -o $@ driver.c $(COMMON)

mtest: mtest.c $(COMMON) mm.h memlib.h
	$(CC) $(CFLAGS) -o $@ mtest.c $(COMMON)

# sanitizer builds
driver-san: driver.c $(COMMON) mm.h memlib.h
	$(CC) $(CFLAGS) $(SANFLAGS) -o $@ driver.c $(COMMON)

mtest-san: mtest.c $(COMMON) mm.h memlib.h
	$(CC) $(CFLAGS) $(SANFLAGS) -o $@ mtest.c $(COMMON)

# run the smoke test and a few mtest seeds
test: driver mtest
	./driver
	@for s in 0 1 2 42 1337; do ./mtest $$s; done

# same under ASan/UBSan
test-san: driver-san mtest-san
	./driver-san
	@for s in 0 1 2 42 1337; do ./mtest-san $$s; done

clean:
	rm -f driver mtest driver-san mtest-san
