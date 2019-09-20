#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

int my_random_int( int min, int max ) {
    float tmp = (float) ( (float) rand() / (float) RAND_MAX );
    return (int) (tmp * ( max - min + 1 ) + min);
}

int main () {
	int losow = 100;
	while (losow--) {
		printf( "%d\n", my_random_int(1,10));
	}
	return 0;
}
