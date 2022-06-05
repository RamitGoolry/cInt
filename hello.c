#include <stdio.h>

int try() {
	 int i;
	 i = 7;

	 while (i < 10) {
		 i = i + 1;
	 }

	 return i;
}

int main() {
	printf("try() = %d\n", try());
	return 0;
}
