PROJECT = ..
$(info $(PROJECT))
INCLUDES = $(PROJECT)/include
SRC = $(PROJECT)

SRC := sprintf stringlib
SRC := $(addsuffix .o, $(SRC))

CFLAGS := -O0 -g -I$(INCLUDES) -DTEST_STRING

$(info SRC = $(SRC))

all: test

%.o: ../%.c
	@echo Making $@ from $^
	gcc -c $(CFLAGS) -o $@ $^

test.o: test.c
	@echo Making $@ from $^
	gcc -c $(CFLAGS) -o $@ $^

test: $(SRC) test.o
	gcc $^ -o test $(CFLAGS)

.PHONY: test_debug
test_debug:
	gdb -ex 'b main' -ex 'r' ./test
