#include <stdio.h>

int try() {
	 int i;
	 i = 0;

	 while (i < 10) {
		 i = i + 1;
	 }

	 return i;
}

int main() {
	printf("Hello, world!\n");
	printf("9 + 10 = %d\n", 9 + 10);
	printf("try() = %d\n", try());
	return 0;
}
