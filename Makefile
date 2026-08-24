CC = gcc
CFLAGS = -O3 -I.

all: build/server build/client

# Compiles src/server.c -> build/server
# NOTE -lm at the end to specify to the linker use math.h library
build/server: src/server.c src/utils.c src/rta.c src/task.c -lm
	@mkdir -p build
	$(CC) $(CFLAGS) $^ -o $@

# Compiles src/client.c -> build/client
build/client: src/client.c src/utils.c src/rta.c src/task.c -lm
	@mkdir -p build
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -rf build