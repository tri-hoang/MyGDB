#include <stdio.h>

void printt();
int main(int argc, char const *argv[])
{
	int test = 100;
	printf("TEST1\n");
	printf("TEST2\n");
	printf("TEST3\n");
	printt();
	return 0;
}

void printt() {
	printf("TEST4\n");
	printf("TEST5\n");
	printf("TEST6\n");
}