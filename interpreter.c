#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int token;
char *src, *old_src;
long long poolsize;
int line;

void next() {
	token = *src++;
}

void expression(int level) {
	// do nothing for now
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
		printf("could not malloc(%lld) for source area\n", poolsize);
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
