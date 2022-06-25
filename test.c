#include <stdio.h>

int fibb(int n) {
	if (n == 0) {
		return 0;
	} else if (n == 1) {
		return 1;
	} else {
		return fibb(n - 1) + fibb(n - 2);
	}
}

int main() {
	printf("fibonacci(20) = %d\n", fibb(20));

	return 0;
}
