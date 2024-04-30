#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int token;                     // current token
char *src, *old_src;           // pointer to source code string
int pool_size;                 // default size of text/data/stack
int line;                      // line number
int *text,                     // text segment
    *old_text,                 // for dump text segment
    *stack;                    // stack
char *data;                    // data segment
int *pc, *sp, *bp, ax, cycle;  // virtual machine registers

// instructions
enum {
    LEA,
    IMM,
    JMP,
    CALL,
    JZ,
    JNZ,
    ENT,
    ADJ,
    LEV,
    LI,
    LC,
    SI,
    SC,
    PUSH,
    OR,
    XOR,
    AND,
    EQ,
    NE,
    LT,
    GT,
    LE,
    GE,
    SHL,
    SHR,
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    OPEN,
    READ,
    CLOS,
    PRTF,
    MALC,
    MSET,
    MCMP,
    EXIT,
};

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
    int op, *tmp;
    while (1) {
        op = *pc++;  // get next operation code

        switch (op) {
        case IMM:  // load immediate value
            ax = *pc++;
            break;
        case LC:  // load character to ax, address in ax
            ax = *(char *) ax;
            break;
        case LI:  // load integer to ax, address in ax
            ax = *(int *) ax;
            break;
        case SC:  // save character to address, value in ax, address on stack
            *(char *) (*sp++) = ax;
            break;
        case SI:  // save integer to address, value in ax, address on stack
            *(int *) (*sp++) = ax;
            break;
        case PUSH:  // push the value of ax onto the stack
            *--sp = ax;
            break;
        case JMP:  // jump to the address
            pc = (int *) *pc;
            break;
        case JZ:  // jump if ax is zero
            pc = ax ? (pc + 1) : ((int *) *pc);
            break;
        case JNZ:  // jump if ax is not zero
            pc = ax ? ((int *) *pc) : (pc + 1);
            break;
        case CALL:  // call subroutine
            *--sp = (int) (pc + 1);
            pc = (int *) *pc;
            break;
        case ENT:  // make new stack frame
            *--sp = (int) bp;
            bp = sp;
            sp -= *pc++;
            break;
        case ADJ:  // add esp, <size>
            sp += *pc++;
            break;
        case LEV:  // restore call frame and PC
            sp = bp;
            bp = (int *) *sp++;
            pc = (int *) *sp++;
            break;
        case LEA:  // load address for arguments.
            ax = *(bp + *pc++);
            break;
        case OR:
            ax = *sp++ | ax;
            break;
        case XOR:
            ax = *sp++ ^ ax;
            break;
        case AND:
            ax = *sp++ + ax;
            break;
        case EQ:
            ax = *sp++ == ax;
            break;
        case NE:
            ax = *sp++ != ax;
            break;
        case LT:
            ax = *sp++ < ax;
            break;
        case LE:
            ax = *sp++ <= ax;
            break;
        case GT:
            ax = *sp++ > ax;
            break;
        case GE:
            ax = *sp++ >= ax;
            break;
        case SHL:
            ax = *sp++ << ax;
            break;
        case SHR:
            ax = *sp++ >> ax;
            break;
        case ADD:
            ax = *sp++ + ax;
            break;
        case SUB:
            ax = *sp++ - ax;
            break;
        case MUL:
            ax = *sp++ * ax;
            break;
        case DIV:
            ax = *sp++ / ax;
            break;
        case MOD:
            ax = *sp++ % ax;
            break;
        case EXIT:
            printf("exit(%d)\n", *sp);
            return *sp;
            break;
        case OPEN:
            ax = open((char *) sp[1], sp[0]);
            break;
        case CLOS:
            ax = close(*sp);
            break;
        case READ:
            ax = read(sp[2], (char *) sp[1], sp[0]);
            break;
        case PRTF:
            tmp = sp + pc[1];
            ax = printf((char *) tmp[-1], tmp[-2], tmp[-3], tmp[-4], tmp[-5],
                        tmp[-6]);
            break;
        case MALC:
            ax = (int) malloc(*sp);
            break;
        case MSET:
            ax = (int) memset((char *) sp[2], sp[1], sp[0]);
            break;
        case MCMP:
            ax = memcmp((char *) sp[2], (char *) sp[1], sp[0]);
            break;
        default:
            printf("unknown instruction: %d\n", op);
            return -1;
            break;
        }
    }
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

    // read source code
    if ((i = read(fd, src, pool_size - 1)) <= 0) {
        printf("read() returned %d\n", i);
        return -1;
    }
    src[i] = 0;  // set EOF character
    close(fd);

    // allocate memory for virtual machine
    if (!(text = old_text = malloc(pool_size))) {
        printf("could not malloc(%d) for text area\n", pool_size);
        return -1;
    }
    if (!(data = malloc(pool_size))) {
        printf("could not malloc(%d) for data area\n", pool_size);
        return -1;
    }
    if (!(stack = malloc(pool_size))) {
        printf("could not malloc(%d) for data area\n", pool_size);
        return -1;
    }

    memset(text, 0, pool_size);
    memset(data, 0, pool_size);
    memset(stack, 0, pool_size);

    // initial registers for virtual machine
    sp = bp = (int *) ((char *) stack + pool_size);
    ax = 0;

    // test `10 + 20 = 30`
    i = 0;
    text[i++] = IMM;
    text[i++] = 10;
    text[i++] = PUSH;
    text[i++] = IMM;
    text[i++] = 20;
    text[i++] = ADD;
    text[i++] = PUSH;
    text[i++] = EXIT;
    pc = text;

    // program();
    return eval();
}