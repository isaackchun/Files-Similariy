compare: compare.c
	gcc -g -std=c99 -Wvla -fsanitize=address -pthread -Wall -o compare compare.c -lm



