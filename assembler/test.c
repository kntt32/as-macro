#include <stdio.h>
#include <stdint.h>

uint64_t num(uint64_t src);

int main() {
    uint64_t value = num(5);
    printf("%ld\n", value);
    return 0;
}

