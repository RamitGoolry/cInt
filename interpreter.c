#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>

#define int long long

int token;
char *src, *old_src;
int poolsize;
int line;

void next() {
	token = *src++;
}

void program() {
	next();

	while (token > 0) {
		printf("token is %c\n", token);
		next();
	}
}

int eval() {
	return 0;
}

int main(int argc, char **argv) {
	int i, fd;

	argc--;
	argv++;

	poolsize = 1024 * 1024; // arbirary size
	line = 1;

	if((fd = open(*argv, 0)) < 0) {
		printf("could not open(%s)\n", *argv);
		return 1;
	}

	if(!(src = old_src = malloc(poolsize))) {
		printf("could not malloc(%d) for source area\n", poolsize);
		return 1;
	}

	if((i = read(fd, src, poolsize - 1)) <= 0) {
		printf("read() returned %d\n", i);
		return 1;
	}

	src[i] = 0;
	close(fd);

	program();
	return eval();
}
