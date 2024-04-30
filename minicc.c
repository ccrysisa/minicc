#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int token;            // current token
char *src, *old_src;  // pointer to source code string
int pool_size;        // default size of text/data/stack
int line;             // line number

void next()
{
    token = *src++;
    return;
}

void expression(int level)
{
    // unimplement
}

void program()
{
    next();
    while (token > 0) {
        printf("token is %c\n", token);
        next();
    }
}

int eval()
{
    // unimplement
    return 0;
}

int main(int argc, char **argv)
{
    int i, fd;

    argc--;
    argv++;

    pool_size = 256 * 1024;  // arbitrary size
    line = 1;

    if ((fd = open(*argv, 0)) < 0) {
        printf("could not open(%s)\n", *argv);
        return -1;
    }

    if (!(src = old_src = malloc(pool_size))) {
        printf("could not malloc(%d) for source area\n", pool_size);
        return -1;
    }

    if ((i = read(fd, src, pool_size - 1)) <= 0) {
        printf("read() returned %d\n", i);
        return -1;
    }
    src[i] = 0;  // set EOF character
    close(fd);

    program();
    return eval();
}