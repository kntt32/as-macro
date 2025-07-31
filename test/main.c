#include <stdio.h>
#include <stdbool.h>

static int numbers[100000];
static int numbers_len = 0;

int main() {
    for(int i=2; i<100000; i++) {
        bool prime_flag = true;
        for(int k=0; i<numbers_len; k++) {
            if(i % numbers[k] == 0) {
                prime_flag = false;
                break;
            }
        }
        if(prime_flag) {
            numbers[numbers_len] = i;
            numbers_len ++;
        }
    }

    for(int i=0; i<numbers_len; i++) {
        printf("%d, ", numbers[i]);
    }
    printf("\n");

    return 0;
}

