#include <stdio.h>

void printt();
void main(int argc, char const *argv[])
{
	long test = 100;
	fprintf(stdout, "TEST1\n");
	fprintf(stdout, "TEST2\n");
	fprintf(stdout, "TEST3\n");
	printt();

}

void printt() {
	fprintf(stdout, "TEST4\n");
	fprintf(stdout, "TEST5\n");
	fprintf(stdout, "TEST6\n");
}