CC = gcc
CFLAGS = -O3 -I.

all: build/server build/client

# Compiles src/server.c -> build/server
build/server: src/server.c src/utils.c src/rta.c src/task.c
	@mkdir -p build
	$(CC) $(CFLAGS) $^ -o $@

# Compiles src/client.c -> build/client
build/client: src/client.c src/utils.c src/rta.c src/task.c
	@mkdir -p build
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -rf build