#include <unistd.h>
#include <stdio.h>

int main(void) {
    int sec = 5;

    for (int i = 0; i < sec; i++) { 
        printf("%d sec...\n", i + 1);
        sleep(1);
    }
}
