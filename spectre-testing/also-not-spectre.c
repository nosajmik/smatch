#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <x86intrin.h>

#define ARRAY_SIZE 10
uint8_t array1[ARRAY_SIZE];
uint8_t array2[ARRAY_SIZE * 4096];

// Victim function with bounds check
uint8_t victim_function(size_t index) {
    volatile uint8_t value = array2[array1[index] * 4096];
    return value;
}

int main() {
    // Initialize array1 with some data
    for (int i = 0; i < ARRAY_SIZE; ++i) {
        array1[i] = rand() % 10;
    }

    // Prime the cache
    for (int i = 0; i < ARRAY_SIZE * 4096; ++i) {
        _mm_clflush(&array2[i]);
    }

    // Perform the attack
    size_t index = 5; // Index outside the bounds of array1
    victim_function(index);

    return 0;
}
