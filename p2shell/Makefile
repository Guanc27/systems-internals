CC ?= gcc
CFLAGS ?= -Wall -Werror -std=c11
LDFLAGS ?=

SRC = myshell.c sim_device.c trace.c allocator.c ../p3malloc/smalloc.c

myshell: $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

sanitizers: CFLAGS += -fsanitize=address,undefined -g
sanitizers: LDFLAGS += -fsanitize=address,undefined
sanitizers: clean myshell

test: myshell
	./tests/run_tests.sh

clean:
	rm -f myshell *~ traces/*.jsonl
