#include <stdio.h>
#include <stdbool.h>

#define MAX (36000000)
#define MAX_SQRT (6000)

static bool FLAGS[MAX];

int main() {
    for(int i=2; i<MAX; i++) {
        FLAGS[i] = true;
    }

    for(int i=2; i<MAX_SQRT; i++) {
        if(FLAGS[i]) {
            for(int k=i*2; k<MAX; k += i) {
                FLAGS[k] = false;
            }
        }
    }

    for(int i=2; i<MAX; i++) {
        if(FLAGS[i]) {
            printf("%d, ", i);
        }
    }
    printf("\n");

    return 0;
}

