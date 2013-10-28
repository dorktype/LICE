CC ?= clang
CFLAGS=-c -Wall -std=gnu99 -MD
LDFLAGS=
SOURCES=ast.c parse.c lice.c gen.c lexer.c util.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=lice

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE) *.d

test: $(EXECUTABLE)
	@cat tests/expect.c tests/types.c     | ./$(EXECUTABLE) | $(CC) -xassembler - && ./a.out
	@cat tests/expect.c tests/numbers.c   | ./$(EXECUTABLE) | $(CC) -xassembler - && ./a.out
	@cat tests/expect.c tests/call.c      | ./$(EXECUTABLE) | $(CC) -xassembler - && ./a.out
	@cat tests/expect.c tests/list.c      | ./$(EXECUTABLE) | $(CC) -xassembler - && ./a.out
	@cat tests/expect.c tests/control.c   | ./$(EXECUTABLE) | $(CC) -xassembler - && ./a.out
	@cat tests/expect.c tests/operators.c | ./$(EXECUTABLE) | $(CC) -xassembler - && ./a.out
	@cat tests/expect.c tests/array.c     | ./$(EXECUTABLE) | $(CC) -xassembler - && ./a.out
	@cat tests/expect.c tests/forloop.c   | ./$(EXECUTABLE) | $(CC) -xassembler - && ./a.out
	@cat tests/expect.c tests/struct.c    | ./$(EXECUTABLE) | $(CC) -xassembler - && ./a.out
	@cat tests/expect.c tests/union.c     | ./$(EXECUTABLE) | $(CC) -xassembler - && ./a.out
