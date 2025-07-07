#include <stdio.h>
#include <stdint.h>

uint64_t num();

int main() {
    uint64_t value = num();
    printf("%ld\n", value);
    return 0;
}

