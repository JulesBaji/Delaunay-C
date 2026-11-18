CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -g
GLAD    = glad

all: tests affichage

# Tests dans le terminal : la triangulation seule, sans OpenGL.
tests: delaunay.c tests.c delaunay.h
	$(CC) $(CFLAGS) delaunay.c tests.c -o $@

# Affichage OpenGL (GLFW + glad).
affichage: delaunay.c affichage.c delaunay.h
	$(CC) $(CFLAGS) -I$(GLAD)/include delaunay.c affichage.c $(GLAD)/src/gl.c \
		-o $@ $$(pkg-config --cflags --libs glfw3) -ldl -lm

valgrind: tests
	valgrind --leak-check=full ./tests

clean:
	rm -f tests affichage

.PHONY: all valgrind clean
