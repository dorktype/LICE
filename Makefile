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
	rm -f $(OBJECTS) $(EXECUTABLE) blob.o program
