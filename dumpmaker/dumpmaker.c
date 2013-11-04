#include <stdio.h>
#include <stdlib.h>

#define START      '|'
#define STARTCOUNT  5

int main() {
	int numstarts = 0;
	int started;
	do { 
		int c = getchar();
		if (EOF == c) 
			break;
		if (started) {
			putchar(c);
			continue;
		}

		if (c == START)
			numstarts++;
		else
			numstarts=0;
		if (numstarts == STARTCOUNT)
			started++;
	} while (1);
}
 
