#include <stdio.h>

int main() {
    int count = 0;

    // The loop will continue as long as count is less than 5
    while (count < 5) {
        printf("Count: %d\n", count);
        count++; // Increment count by 1
    }

    printf("Loop ended.\n");

    return 0;
}
