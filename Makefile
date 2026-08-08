CC = gcc
CFLAGS = -O3 -I.

# List only the entry-point names (e.g., server and client)
PROGRAMS = server client
BINS = $(addprefix build/, $(PROGRAMS))

all: $(BINS)

# Compile a specific executable with all supporting src/ files
build/%: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) $< src/*.c -o $@   # adjust helper path if any

clean:
	rm -rf build

.PHONY: all clean