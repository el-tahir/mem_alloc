CC      := gcc
CFLAGS  := -std=gnu11 -Wall -Wextra -g
SANFLAGS := -fsanitize=address,undefined

COMMON  := mm.c memlib.c

.PHONY: all clean test test-san

all: test_trace test_random

test_trace: test_trace.c $(COMMON) mm.h memlib.h
	$(CC) $(CFLAGS) -o $@ test_trace.c $(COMMON)

test_random: test_random.c $(COMMON) mm.h memlib.h
	$(CC) $(CFLAGS) -o $@ test_random.c $(COMMON)

# sanitizer builds
test_trace-san: test_trace.c $(COMMON) mm.h memlib.h
	$(CC) $(CFLAGS) $(SANFLAGS) -o $@ test_trace.c $(COMMON)

test_random-san: test_random.c $(COMMON) mm.h memlib.h
	$(CC) $(CFLAGS) $(SANFLAGS) -o $@ test_random.c $(COMMON)

# run the smoke test and a few test_random seeds
test: test_trace test_random
	./test_trace
	@for s in 0 1 2 42 1337; do ./test_random $$s; done

# same under ASan/UBSan
test-san: test_trace-san test_random-san
	./test_trace-san
	@for s in 0 1 2 42 1337; do ./test_random-san $$s; done

clean:
	rm -f test_trace test_random test_trace-san test_random-san
