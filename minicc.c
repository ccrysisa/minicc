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
int token_val;                 // value of current token (mainly for number)
int *current_id,               // current parsed ID
    *symbols;                  // symbol table
int *idmain;                   // the 'main' function
int base_type;                 // the type of a declaration
int expr_type;                 // the type of an expression

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

// tokens and classes (operators last and in precedence order)
enum {
    Num = 128,
    Fun,
    Sys,
    Glo,
    Loc,
    Id,
    Char,
    Else,
    Enum,
    If,
    Int,
    Return,
    Sizeof,
    While,
    Assign,
    Cond,
    Lor,
    Lan,
    Or,
    Xor,
    And,
    Eq,
    Ne,
    Lt,
    Gt,
    Le,
    Ge,
    Shl,
    Shr,
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Inc,
    Dec,
    Brak,
};

// fields of identifier
enum {
    Token,
    Hash,
    Name,
    Type,
    Class,
    Value,
    BType,
    BClass,
    BValue,
    IdSize,
};

// types of variable and function
enum {
    CHAR,
    INT,
    PTR,
};

void next()
{
    char *last_pos;
    int hash;

    while ((token = *src)) {
        ++src;
        // parse token here
        if (token == '\n') {
            ++line;
        } else if (token == '#') {
            // skip macro, becausewe will not support it
            while (*src != 0 && *src != '\n') {
                ++src;
            }
        } else if ((token >= 'a' && token <= 'z') ||
                   (token >= 'A' && token <= 'Z') || (token == '_')) {
            // parse identifier
            last_pos = src - 1;
            hash = token;

            while ((*src >= 'a' && *src <= 'z') ||
                   (*src >= 'A' && *src <= 'Z') ||
                   (*src >= '0' && *src <= '9') || (*src == '_')) {
                hash = hash * 147 + *src;
                src++;
            }

            // look for existing identifier, liner search
            current_id = symbols;
            while (current_id[Token]) {
                if (current_id[Hash] == hash &&
                    !memcmp((char *) current_id[Name], last_pos,
                            src - last_pos)) {
                    // found one, return
                    token = current_id[Token];
                    return;
                }
                current_id += IdSize;
            }

            // store new ID
            current_id[Name] = (int) last_pos;
            current_id[Hash] = hash;
            token = current_id[Token] = Id;
            return;
        } else if (token >= '0' && token <= '9') {
            // parse number, three kinds: dec(123), hex(0x123), oct(017)
            token_val = token - '0';
            if (token_val > 0) {
                // dec, starts with [1-9]
                while (*src >= '0' && *src <= '9') {
                    token_val = token_val * 10 + *src++ - '0';
                }
            } else {
                // starts with number 0
                if (*src == 'x' || *src == 'X') {
                    // hex
                    token = *++src;
                    while ((token >= '0' && token <= '9') ||
                           (token >= 'a' && token <= 'z') ||
                           (token >= 'A' && token <= 'Z')) {
                        token_val = token_val * 16 + (token & 15) +
                                    (token >= 'A' ? 9 : 0);
                        token = *++src;
                    }
                } else {
                    // oct
                    while (*src >= '0' && *src <= '7') {
                        token_val = token_val * 8 + *src++ - '0';
                    }
                }
            }
            token = Num;
            return;
        } else if (token == '"' || token == '\'') {
            // parse string literal, currently, the only supported escape
            // character is '\n', store the string literal into data.
            last_pos = data;
            while (*src != 0 && *src != token) {
                token_val = *src++;
                if (token_val == '\\') {
                    // escape character
                    token_val = *src++;
                    if (token_val == 'n') {
                        token_val = '\n';
                    }
                }
                if (token == '"') {
                    *data++ = token_val;
                }
            }

            src++;
            // if it is a single character, return Num token
            if (token == '"') {
                token_val = (int) last_pos;
            } else {
                token = Num;
            }
            return;
        } else if (token == '/') {
            if (*src == '/') {
                // skip comments
                while (*src != 0 && *src != '\n') {
                    ++src;
                }
            } else {
                // divide operator
                token = Div;
                return;
            }
        } else if (token == '=') {
            // parse '==' and '='
            if (*src == '=') {
                ++src;
                token = Eq;
            } else {
                token = Assign;
            }
            return;
        } else if (token == '+') {
            // parse '+' and '++'
            if (*src == '+') {
                ++src;
                token = Inc;
            } else {
                token = Add;
            }
            return;
        } else if (token == '-') {
            // parse '-' amd '--'
            if (*src == '-') {
                ++src;
                token = Dec;
            } else {
                token = Sub;
            }
            return;
        } else if (token == '!') {
            // parse '!='
            if (*src == '=') {
                ++src;
                token = Ne;
            }
            return;
        } else if (token == '<') {
            // parse '<=', '<<' and '<'
            if (*src == '=') {
                ++src;
                token = Le;
            } else if (*src == '<') {
                ++src;
                token = Shl;
            } else {
                token = Lt;
            }
            return;
        } else if (token == '>') {
            // parse '>=', '>>' and '>'
            if (*src == '=') {
                ++src;
                token = Ge;
            } else if (*src == '>') {
                ++src;
                token = Shr;
            } else {
                token = Gt;
            }
            return;
        } else if (token == '|') {
            // parse '|' and '||'
            if (*src == '|') {
                ++src;
                token = Lor;
            } else {
                token = Or;
            }
            return;
        } else if (token == '&') {
            // parse '&' and '&&'
            if (*src == '&') {
                ++src;
                token = Lan;
            } else {
                token = And;
            }
            return;
        } else if (token == '^') {
            token = Xor;
            return;
        } else if (token == '%') {
            token = Mod;
            return;
        } else if (token == '*') {
            token = Mul;
            return;
        } else if (token == '[') {
            token = Brak;
            return;
        } else if (token == '?') {
            token = Cond;
            return;
        } else if (token == '~' || token == ';' || token == '{' ||
                   token == '}' || token == '(' || token == ')' ||
                   token == ']' || token == ',' || token == ':') {
            // directly return the character as token
            return;
        }
    }
    return;
}

void expression(int level)
{
    // unimplement
}

void match(int tk)
{
    if (token != tk) {
        printf("%d expected token: %d\n", line, tk);
        exit(-1);
    }
    next();
}

void enum_declaration()
{
    // parse enum [id] { a = 1, b = 3, ... }
    int i;
    i = 0;

    while (token != '}') {
        if (token != Id) {
            printf("%d: bad enum identifier %d\n", line, token);
            exit(-1);
        }
        match(Id);

        if (token == Assign) {  // e.g. { a = 10 }
            match(Assign);
            if (token != Num) {
                printf("%d: enum initializer\n", line);
                exit(-1);
            }
            i = token_val;
            match(Num);
        }

        current_id[Class] = Num;
        current_id[Type] = INT;
        current_id[Value] = i++;

        match(',');
    }
}

void global_declaration()
{
    // global_declaration ::= enum_decl | variable_decl | function_decl
    //
    // enum_decl ::= 'enum' [id] '{' id ['=' 'num'] {',' id ['=' 'num'} '}'
    //
    // variable_decl ::= type {'*'} id { ',' {'*'} id } ';'
    //
    // function_decl ::= type {'*'} id '(' parameter_decl ')' '{' body_decl '}'

    int type;  // tmp, actual type for variable

    base_type = INT;

    // parse enum, this should be treated alone.
    if (token == Enum) {
        // enum [id] {1 = 10, b = 26, ... }
        match(Enum);
        if (token != '{') {  // skip te [id] part
            match(Id);
        }
        if (token == '{') {  // parse the assign part
            match('{');
            enum_declaration();
            match('}');
        }
        match(';');
        return;
    }

    // parse type information
    if (token == Int) {
        match(Int);
    } else if (token == Char) {
        match(Char);
        base_type = CHAR;
    }

    // parse the comma seperated variable declaration
    while (token != ';' && token != '}') {
        type = base_type;
        // parse pointer type, note that there may exist `int ***x`
        while (token == Mul) {
            match(Mul);
            type = type + PTR;
        }

        if (token != Id) {  // identifier declaration
            printf("%d: bad global declaration\n", line);
            exit(-1);
        }
        if (current_id[Class]) {  // identifier exists
            printf("%d: duplicate global declaration\n", line);
            exit(-1);
        }
        match(Id);
        current_id[Type] = type;

        if (token == '(') {  // function declaration
            current_id[Class] = Fun;
            // the memory address of function
            current_id[Value] = (int) (text + 1);
            // function_declaration();
        } else {  // variable declaration
            current_id[Class] = Glo;
            current_id[Value] = (int) data;
            data = data + sizeof(int);
        }

        match(',');
    }
    next();
}

void program()
{
    // get next token
    next();
    while (token > 0) {
        global_declaration();
    }
}

int eval()
{
    int op, *tmp;
    while (1) {
        op = *pc++;  // get next operation code

        if (op == IMM) {  // load immediate value
            ax = *pc++;
        } else if (op == LC) {  // load character to ax, address in ax
            ax = *(char *) ax;
        } else if (op == LI) {  // load integer to ax, address in ax
            ax = *(int *) ax;
        } else if (op == SC) {  // save character to address, value in ax,
                                // address on stack
            *(char *) (*sp++) = ax;
        } else if (op == SI) {  // save integer to address, value in ax, address
                                // on stack
            *(int *) (*sp++) = ax;
        } else if (op == PUSH) {  // push the value of ax onto the stack
            *--sp = ax;
        } else if (op == JMP) {  // jump to the address
            pc = (int *) *pc;
        } else if (op == JZ) {  // jump if ax is zero
            pc = ax ? (pc + 1) : ((int *) *pc);
        } else if (op == JNZ) {  // jump if ax is not zero
            pc = ax ? ((int *) *pc) : (pc + 1);
        } else if (op == CALL) {  // call subroutine
            *--sp = (int) (pc + 1);
            pc = (int *) *pc;
        } else if (op == ENT) {  // make new stack frame
            *--sp = (int) bp;
            bp = sp;
            sp -= *pc++;
        } else if (op == ADJ) {  // add esp, <size>
            sp += *pc++;
        } else if (op == LEV) {  // restore call frame and PC
            sp = bp;
            bp = (int *) *sp++;
            pc = (int *) *sp++;
        } else if (op == LEA) {  // load address for arguments.
            ax = *(bp + *pc++);
        } else if (op == OR) {
            ax = *sp++ | ax;
        } else if (op == XOR) {
            ax = *sp++ ^ ax;
        } else if (op == AND) {
            ax = *sp++ + ax;
        } else if (op == EQ) {
            ax = *sp++ == ax;
        } else if (op == NE) {
            ax = *sp++ != ax;
        } else if (op == LT) {
            ax = *sp++ < ax;
        } else if (op == LE) {
            ax = *sp++ <= ax;
        } else if (op == GT) {
            ax = *sp++ > ax;
        } else if (op == GE) {
            ax = *sp++ >= ax;
        } else if (op == SHL) {
            ax = *sp++ << ax;
        } else if (op == SHR) {
            ax = *sp++ >> ax;
        } else if (op == ADD) {
            ax = *sp++ + ax;
        } else if (op == SUB) {
            ax = *sp++ - ax;
        } else if (op == MUL) {
            ax = *sp++ * ax;
        } else if (op == DIV) {
            ax = *sp++ / ax;
        } else if (op == MOD) {
            ax = *sp++ % ax;
        } else if (op == EXIT) {
            printf("exit(%d)\n", *sp);
            return *sp;
        } else if (op == OPEN) {
            ax = open((char *) sp[1], sp[0]);
        } else if (op == CLOS) {
            ax = close(*sp);
        } else if (op == READ) {
            ax = read(sp[2], (char *) sp[1], sp[0]);
        } else if (op == PRTF) {
            tmp = sp + pc[1];
            ax = printf((char *) tmp[-1], tmp[-2], tmp[-3], tmp[-4], tmp[-5],
                        tmp[-6]);
        } else if (op == MALC) {
            ax = (int) malloc(*sp);
        } else if (op == MSET) {
            ax = (int) memset((char *) sp[2], sp[1], sp[0]);
        } else if (op == MCMP) {
            ax = memcmp((char *) sp[2], (char *) sp[1], sp[0]);
        } else {
            printf("unknown instruction: %d\n", op);
            return -1;
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
    if (!(symbols = malloc(pool_size))) {
        printf("could not malloc(%d) for symbol table\n", pool_size);
        return -1;
    }

    memset(text, 0, pool_size);
    memset(data, 0, pool_size);
    memset(stack, 0, pool_size);
    memset(symbols, 0, pool_size);

    // initial registers for virtual machine
    sp = bp = (int *) ((char *) stack + pool_size);
    ax = 0;

    src =
        "char else enum if int return sizeof while"
        "open read close printf malloc memset memcmp exit void main";

    // add keywords to symbol table
    i = Char;
    while (i <= While) {
        next();
        current_id[Token] = i++;
    }

    // add library to symbol table
    i = OPEN;
    while (i <= EXIT) {
        next();
        current_id[Class] = Sys;
        current_id[Type] = INT;
        current_id[Value] = i++;
    }

    // handle void type
    next();
    current_id[Token] = Char;
    // keep track of main
    next();
    idmain = current_id;

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

    program();
    return eval();
}