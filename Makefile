CC ?= clang
CFLAGS=-c -Wall
LDFLAGS=
SOURCES=ast.c parse.c gmcc.c gen.c var.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=gmcc

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE) blob.o program
