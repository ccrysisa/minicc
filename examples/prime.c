#include <stdio.h>

int is_prime(int x)
{
    int i;
    i = 3;
    while (i < x) {
        if (x % i == 0) {
            return 0;
        }
        i++;
    }
    return 1;
}

int main()
{
    int count, n, i;
    count = 0;
    n = 10;

    i = 2;
    while (i <= n) {
        count = count + is_prime(i);
        ++i;
    }

    printf("Total primes: %d\n", count);
    return 0;
}