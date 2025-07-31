#include <stdio.h>
#include <stdbool.h>

int main() {
    long long sum = 0;
    for(long long i=0; i<1000000000; i++) {
        sum += i;
    }
    printf("%lld\n", sum);

    return 0;
}

