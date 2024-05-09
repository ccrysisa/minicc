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
int index_of_bp;               // index of bp pointer on stack

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
                current_id = current_id + IdSize;
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

void match(int tk)
{
    if (token != tk) {
        printf("%d expected token: %d\n", line, tk);
        exit(-1);
    }
    next();
}

void expression(int level)
{
    // expressions have various format.
    // but majorly can be divided into two parts: unit and operator
    // for example `(char) *a[10] = (int *) func(b > 0 ? 10 : 20);
    // `a[10]` is an unit while `*` is an operator.
    // `func(...)` in total is an unit.
    // so we should first parse those unit and unary operators
    // and then the binary ones
    //
    // also the expression can be in the following types:
    //
    // 1. unit_unary ::= unit | unit unary_op | unary_op unit
    // 2. expr ::= unit_unary (bin_op unit_unary ...)

    int *id;
    int tmp;
    int *addr;

    // unit_unary()
    {
        if (token == Num) {
            match(Num);

            // emit code
            *++text = IMM;
            *++text = token_val;
            expr_type = INT;
        } else if (token == '"') {  // string
            // emit code;
            *++text = IMM;
            *++text = token_val;

            match('"');
            // store the rest strings
            while (token == '"') {
                match('"');
            }

            // append the end of string character '\0', all the data are default
            // to 0, so just move data one position forward.
            data = (char *) (((int) data + sizeof(int)) & (-sizeof(int)));
            expr_type = PTR;
        } else if (Sizeof) {
            // sizeof is actually an unary operator
            // now only `sizeof(int)`, `sizeof(char)` and `sizeof(int/char
            // *...)` are supported.

            match(Sizeof);
            match('(');
            expr_type = INT;

            if (token == Int) {
                match(Int);
            } else if (token == Char) {
                expr_type = CHAR;
                match(Char);
            }

            while (token == Mul) {
                expr_type = expr_type + PTR;
                match(Mul);
            }

            match(')');

            // emit code
            *++text = IMM;
            *++text = (expr_type == CHAR) ? sizeof(char) : sizeof(int);

            expr_type = INT;
        } else if (token == Id) {
            // there are several type when occurs to Id
            // but this is unit, so it can only be
            // 1. function call
            // 2. Enum variable
            // 3. global/local variable

            match(Id);
            id = current_id;

            if (token == '(') {  // function call
                match('(');

                // pass in arguments
                tmp = 0;  // number of arguments
                while (token != ')') {
                    expression(Assign);
                    *++text = PUSH;
                    ++tmp;

                    if (token == ',') {
                        match(',');
                    }
                }
                match(')');

                // emit code
                if (id[Class] == Sys) {  // system functions
                    *++text = id[Value];
                } else if (id[Class] == Fun) {  // function call
                    *++text = CALL;
                    *++text = id[Value];
                } else {
                    printf("%d: bad function call\n", line);
                    exit(-1);
                }

                // clean the stack for arguments
                if (tmp > 0) {
                    *++text = ADJ;
                    *++text = tmp;
                }

                expr_type = id[Type];
            } else if (id[Class] == Num) {  // enum
                *++text = IMM;
                *++text = id[Value];
                expr_type = INT;
            } else {  // variable
                if (id[Class] == Loc) {
                    *++text = LEA;
                    *++text = index_of_bp - id[Value];
                } else if (id[Class] == Glo) {
                    *++text = IMM;
                    *++text = id[Value];
                } else {
                    printf("%d: undefined variable\n", line);
                    exit(-1);
                }
                // emit code
                // default behaviour is to load the value of the address which
                // is stored in `ax`
                expr_type = id[Type];
                *++text = (expr_type == Char) ? LC : LI;
            }
        } else if (token == '(') {  // cast or parenthesis
            match('(');

            if (token == Int || token == Char) {
                tmp = (token == Char) ? CHAR : INT;  // cast type
                match(token);

                while (token == Mul) {
                    match(Mul);
                    tmp = tmp + PTR;
                }

                match(')');
                expression(Inc);  // cast has precedence as Inc(++)
                expr_type = tmp;
            } else {  // normal parenthesis
                expression(Assign);
                match(')');
            }
        } else if (token == Mul) {  // dereference *<addr>
            match(Mul);
            expression(Inc);  // dereference has the same precedence as Inc(++)

            if (expr_type >= PTR) {
                expr_type = expr_type - PTR;
            } else {
                printf("%d: bad dereference\n", line);
                exit(-1);
            }

            *++text = (expr_type == CHAR) ? LC : LI;
        } else if (token == And) {  // get the address of
            match(And);
            expression(Inc);

            if (*text == LC || *text == LI) {
                text--;
            } else {
                printf("%d: bad address\n", line);
                exit(-1);
            }

            expr_type = expr_type + PTR;
        } else if (token == '!') {  // not
            match('!');
            expression(Inc);

            // emit code, use <expr> == 0
            *++text = PUSH;
            *++text = IMM;
            *++text = 0;
            *++text = EQ;

            expr_type = INT;
        } else if (token == '~') {  // bitwise not
            match('~');
            expression(Inc);

            // emit code, use <expr> ^ 0xFFFF(-1)
            *++text = PUSH;
            *++text = IMM;
            *++text = -1;
            *++text = XOR;

            expr_type = INT;
        } else if (token == Add) {  // +var, do nothing
            match(Add);
            expression(Inc);

            expr_type = INT;
        } else if (token == Sub) {  // -var
            match(Sub);

            if (token == Num) {
                *++text = IMM;
                *++text = -token_val;
                match(Num);
            } else {  // -x == -1 * x
                expression(Inc);
                *++text = PUSH;
                *++text = IMM;
                *++text = -1;
                *++text = MUL;
            }

            expr_type = INT;
        } else if (token == Inc || token == Dec) {
            tmp = token;
            match(token);
            expression(Inc);

            if (*text == LC) {
                *text = PUSH;
                *++text = LC;
            } else if (*text == LI) {
                *text = PUSH;
                *++text = LI;
            } else {
                printf("%d: bad lvalue of pre-increment\n", line);
                exit(-1);
            }

            *++text = PUSH;
            *++text = IMM;
            *++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
            *++text = (tmp == Inc) ? ADD : SUB;
            *++text = (expr_type == CHAR) ? SC : SI;
        }
    }

    // binary operator and postfix operators.
    {
        while (token >= level) {
            // handle according to current operator's precedence
            tmp = expr_type;

            if (token == Assign) {
                // var = expr;
                match(Assign);
                if (*text == LC || *text == LI) {
                    *text = PUSH;  // save the lvalue's address
                } else {
                    printf("%d: bad lvalue in assignment\n", line);
                    exit(-1);
                }
                expression(Assign);

                expr_type = tmp;
                *text = (expr_type == CHAR) ? SC : SI;
            } else if (token == Cond) {
                // expr ? a : b;
                match(Cond);
                *++text = JZ;
                addr = ++text;
                expression(Assign);

                if (token == ':') {
                    match(':');
                } else {
                    printf("%d: missing colon in conditional\n", line);
                    exit(-1);
                }

                *addr = (int) (text + 3);
                *++text = JMP;
                addr = ++text;
                expression(Assign);
                *addr = (int) (text + 1);
            } else if (token == Lor) {
                // logic or
                match(Lor);
                *++text = JNZ;
                addr = ++text;
                expression(Lan);
                *addr = (int) (text + 1);
                expr_type = INT;
            } else if (token == Lan) {
                // logic and
                match(Lan);
                *++text = JZ;
                addr = ++text;
                expression(Or);
                *addr = (int) (text + 1);
                expr_type = INT;
            } else if (token == Xor) {
                // bitwise xor
                match(Xor);
                *++text = PUSH;
                expression(And);
                *++text = XOR;
                expr_type = INT;
            } else if (token == And) {
                // bitwise and
                match(And);
                *++text = PUSH;
                expression(Eq);
                *++text = AND;
                expr_type = INT;
            } else if (token == Eq) {
                // equal ==
                match(Eq);
                *++text = PUSH;
                expression(Ne);
                *++text = EQ;
                expr_type = INT;
            } else if (token == Ne) {
                // not equal !=
                match(Ne);
                *++text = PUSH;
                expression(Lt);
                *++text = Ne;
                expr_type = INT;
            } else if (token == Lt) {
                // less than <
                match(Lt);
                *++text = PUSH;
                expression(Shl);
                *++text = LT;
                expr_type = INT;
            } else if (token == Gt) {
                // greater than >
                match(Gt);
                *++text = PUSH;
                expression(Shl);
                *++text = GT;
                expr_type = INT;
            } else if (token == Le) {
                // less than or equal to <=
                match(Le);
                *++text = PUSH;
                expression(Shl);
                *++text = LE;
                expr_type = INT;
            } else if (token == Ge) {
                // greater than or equal to >=
                match(Ge);
                *++text = PUSH;
                expression(Shl);
                *++text = GE;
                expr_type = INT;
            } else if (token == Shl) {
                // shift left
                match(Shl);
                *++text = PUSH;
                expression(Add);
                *++text = SHL;
                expr_type = INT;
            } else if (token == Shr) {
                // shift right
                match(Shr);
                *++text = PUSH;
                expression(Add);
                *++text = SHR;
                expr_type = INT;
            } else if (token == Add) {
                // add
                match(Add);
                *++text = PUSH;
                expression(Mul);

                expr_type = tmp;
                if (expr_type > PTR) {
                    // pointer type, and not `char *`
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = MUL;
                }
                *++text = ADD;
            } else if (token == Sub) {
                // sub
                match(Sub);
                *++text = PUSH;
                expression(Mul);

                if (tmp > PTR && tmp == expr_type) {
                    // pointers subtraction
                    *++text = SUB;
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = DIV;
                    expr_type = INT;
                } else if (tmp > PTR) {
                    // pointer movement
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = MUL;
                    *++text = SUB;
                    expr_type = tmp;
                } else {
                    // numeral substraction
                    *++text = SUB;
                    expr_type = tmp;
                }
            } else if (token == Mul) {
                // multiply
                match(Mul);
                *++text = PUSH;
                expression(Inc);
                *++text = MUL;
                expr_type = tmp;
            } else if (token == Div) {
                // division
                match(Div);
                *++text = PUSH;
                expression(Inc);
                *++text = DIV;
                expr_type = tmp;
            } else if (token == Mod) {
                // modulo
                match(Mod);
                *++text = PUSH;
                expression(Inc);
                *++text = MOD;
                expr_type = tmp;
            } else if (token == Inc || token == Dec) {
                // postfix inc(++) and dec(--)
                // we will increase the value to the variable and decrease it
                // on `ax` to get its original value.
                if (*text == LC) {
                    *text = PUSH;
                    *++text = LC;
                } else if (*text == LI) {
                    *text = PUSH;
                    *++text = LI;
                } else {
                    printf("%d: bad value in increment\n", line);
                    exit(-1);
                }

                *++text = PUSH;
                *++text = IMM;
                *++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
                *++text = (token == Inc) ? ADD : SUB;
                *++text = (expr_type == CHAR) ? SC : SI;
                *++text = PUSH;
                *++text = IMM;
                *++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
                *++text = (token == Inc) ? SUB : ADD;
                match(token);
            } else if (token == Brak) {
                // array access var[xx]
                match(Brak);
                *++text = PUSH;
                expression(Assign);
                match(']');

                if (tmp > PTR) {
                    // pointer, not `char *`
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = MUL;
                } else if (tmp < PTR) {
                    printf("%d: pointer type expected\n", line);
                    exit(-1);
                }
                expr_type = tmp - PTR;
                *++text = ADD;
                *++text = (expr_type == CHAR) ? LC : LI;
            } else {
                printf("%d: compiler error, token = %d\n", line, token);
                exit(-1);
            }
        }
    }
}

void statement()
{
    // there are 6 kinds of statements here:
    // 1. if (...) <statement> [else <statement>]
    // 2. while (...) <statement>
    // 3. { <statement> }
    // 4. return xxx;
    // 5. <empty statement>;
    // 6. expression; (expression end with semicolon)

    int *a, *b;  // bess for branch control

    if (token == If) {
        // if (...) <statement> [else <statement>]
        //
        //   if (<cond>)                   <cond>
        //                                 JZ a
        //     <true_statement>   ===>     <true_statement>
        //   else:                         JMP b
        // a:                           a:
        //     <false_statement>           <false_statement>
        // b:                           b:

        match(If);
        match('(');
        expression(Assign);  // parse condition
        match(')');


        // emit code for JZ a
        *++text = JZ;
        b = ++text;

        statement();  // parse true statement

        if (token == Else) {
            match(Else);

            // emit code for JMP b
            *++text = JMP;
            *b = (int) (text + 2);
            b = ++text;

            statement();  // parse false statement
        }

        *b = (int) (text + 1);
    } else if (token == While) {
        // a:                       a:
        //    while (<cond>)          <cond>
        //                            JZ b
        //     <statement>    ===>    <statement>
        //                            JMP a
        // b:                       b:

        match(While);
        a = text + 1;
        match('(');
        expression(Assign);
        match(')');

        // emit code for JZ b
        *++text = JZ;
        b = ++text;

        statement();  // parse statement

        // emit code for JMP a
        *++text = JMP;
        *++text = (int) a;
        *b = (int) (text + 1);
    } else if (token == Return) {
        // return [expression];
        match(Return);

        if (token != ';') {
            expression(Assign);
        }
        match(';');

        // emit code for return
        *++text = LEV;
    } else if (token == '{') {
        // { <statement> ... }
        match('{');
        while (token != '}') {
            statement();
        }
        match('}');
    } else if (token == ';') {
        // empty statement
        match(';');
    } else {
        // `a= b;` or `function_call()`
        expression(Assign);
        match(';');
    }
}

void function_parameter()
{
    int type;
    int params;
    params = 0;

    while (token != ')') {
        // e.g. int name, ...
        type = INT;
        if (token == Int) {
            match(Int);
        } else if (token == Char) {
            type = CHAR;
            match(Char);
        }

        // pointer typr, e.g. int ****a
        while (token == Mul) {
            match(Mul);
            type = type + PTR;
        }

        // parameter name
        if (token != Id) {  // invalid declaration
            printf("%d: bad parameter declaration\n", line);
            exit(-1);
        }
        if (current_id[Class] == Loc) {  // identifier exists
            printf("%d: duplicate parameter declaration\n", line);
            exit(-1);
        }
        match(Id);

        // store the parameter
        current_id[BClass] = current_id[Class];
        current_id[Class] = Loc;
        current_id[BType] = current_id[Type];
        current_id[Type] = type;
        current_id[BValue] = current_id[Value];
        current_id[Value] = params++;  // index of current parameter

        if (token == ',') {
            match(',');
        }
    }

    index_of_bp = params + 1;  // set index of bp pointer
}

void function_body()
{
    // type func_name (...) {...}
    //                   -->|   |<--

    // ... {
    // 1. local declarations
    // 2. statements
    // }

    int type;
    int pos_local;  // position of local variables on the stack
    pos_local = index_of_bp;

    while (token == Int || token == Char) {
        // local variable declaration, just like global ones
        base_type = (token == Int) ? INT : CHAR;
        match(token);

        while (token != ';') {
            type = base_type;
            while (token == Mul) {
                match(Mul);
                type = type + PTR;
            }

            if (token != Id) {  // invalid declaration
                printf("%d: bad local declaration\n", line);
                exit(-1);
            }
            if (current_id[Class] == Loc) {  // identifier exists
                printf("%d: duplicate parameter declaration\n", line);
                exit(-1);
            }
            match(Id);

            // store the local variable
            current_id[BClass] = current_id[Class];
            current_id[Class] = Loc;
            current_id[BType] = current_id[Type];
            current_id[Type] = type;
            current_id[BValue] = current_id[Value];
            current_id[Value] = ++pos_local;  // index of current local variable

            if (token == ',') {
                match(',');
            }
        }

        match(';');
    }

    // save the stack size for local variables
    *++text = ENT;
    *++text = pos_local - index_of_bp;

    // statements
    while (token != '}') {
        statement();
    }

    // emit code for leaving the sub function
    *++text = LEV;
}

void function_declaration()
{
    // type func_name (...) {...}
    //               | this part

    match('(');
    function_parameter();
    match(')');
    match('{');
    function_body();
    // match('}');  // remain math('}') to global_declaration()

    // unwind local variable declarations for all local variables
    current_id = symbols;
    while (current_id[Token]) {
        if (current_id[Class] == Loc) {
            current_id[Class] = current_id[BClass];
            current_id[Type] = current_id[BType];
            current_id[Value] = current_id[BValue];
        }
        current_id = current_id + IdSize;
    }
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

        if (token == ',') {
            match(',');
        }
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
            function_declaration();
        } else {  // variable declaration
            current_id[Class] = Glo;
            current_id[Value] = (int) data;
            data = data + sizeof(int);
        }

        if (token == ',') {
            match(',');
        }
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
            sp = sp - *pc++;
        } else if (op == ADJ) {  // add esp, <size>
            sp = sp + *pc++;
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