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

int try() {
	 return 10 * 10;
}

int main() {
	printf("Hello, world!\n");
	printf("9 + 10 = %d\n", 9 + 10);
	printf("factorial(3) = %d\n", factorial(3));
	printf("try() = %d\n", try());
	return 0;
}
