#include <stdio.h>

int factorial(int n) {
	int f;
	f = 1;
	while (n > 1) {
		f = f * n;
		n--;
	}
	return f;
}

int main() {
	printf("Hello, world!\n");
	printf("9 + 10 = %d\n", 9 + 10);
	printf("factorial(5) = %d\n", factorial(5));
	return 0;
}
