catan: main.c move.c position.c search.c util.c Makefile
	clang -Wall -Wextra -pedantic -std=c99 -O2 -flto=full $< -o catan -lm
