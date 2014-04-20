#include <stdio.h>
#include <stdlib.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "mpc.h"

/* Declare lisp value (lval) struct */
typedef struct {
    int  type;
    long num;
    int  error;
} lval;

/* Enumeration of all possible type lval types */
enum { LVAL_NUM, LVAL_ERR };

/* Enumeration of all possible errors */
enum { LERR_ZERO_DIV, LERR_BAD_OP, LERR_BAD_NUM };

/* Factory method to create number-typed lval */
lval lval_num(long x) {
    lval v;
    v.type = LVAL_NUM;
    v.num  = x;

    return v;
}

/* Factory method to create error-typed lval */
lval lval_error(int x) {
    lval v;
    v.type  = LVAL_ERR;
    v.error = x;

    return v;
}

/* Print an lval */
void lval_print(lval v) {
    switch (v.type) {
        case LVAL_NUM:
            printf("%li", v.num);
            break;

        case LVAL_ERR:
            if (v.error == LERR_ZERO_DIV) { printf("Error: Division by zero"); }
            if (v.error == LERR_BAD_OP) { printf("Error: Invalid operator"); }
            if (v.error == LERR_BAD_NUM) { printf("Error: Invalid number"); }
            break;
    }
}

/* Print an lval, followed by a newline */
void lval_println(lval v) {
    lval_print(v);
    putchar('\n');
}

/* Evaluate two lvals based on the operator */
lval eval_op(lval x, char* op, lval y) {
    if (x.type == LVAL_ERR) {
        return x;
    }
    if (y.type == LVAL_ERR) {
        return y;
    }

    long xnum = x.num;
    long ynum = y.num;

    if (strcmp(op, "+") == 0) {
        return lval_num(xnum + ynum);
    }
    if (strcmp(op, "-") == 0) {
        return lval_num(xnum - ynum);
    }
    if (strcmp(op, "*") == 0) {
        return lval_num(xnum * ynum);
    }
    if (strcmp(op, "/") == 0) {
        return ynum == 0 ? lval_error(LERR_ZERO_DIV) : lval_num(xnum / ynum);
    }

    return lval_error(LERR_BAD_OP);
}

/* Evaluate the AST */
lval eval(mpc_ast_t* ast) {

    /* If `ast` is a "number", return directly. Otherwise, it's an operator. */
    if (strstr(ast->tag, "number")) {
        errno = 0;
        long x = strtol(ast->contents, NULL, 10);
        return errno != ERANGE ? lval_num(x) : lval_error(LERR_BAD_NUM);
    }
    
    /* Get the operator */
    char* op = ast->children[1]->contents;

    /* Evaluate the third child */
    lval x = eval(ast->children[2]);

    /* Fold the rest of the child, combining using the `op` */
    int i = 3;
    while (strstr(ast->children[i]->tag, "expr")) {
        /* Update `x` */
        x = eval_op(x, op, eval(ast->children[i]));

        i++;
    }
    
    return x;
}

int main(int argc, char** argv) {

    /* Create parsers */
    mpc_parser_t* Number    = mpc_new("number");
    mpc_parser_t* Operator  = mpc_new("operator");
    mpc_parser_t* Expr      = mpc_new("expr");
    mpc_parser_t* Lispc     = mpc_new("lispc");

    /* Define the language */
    mpca_lang(MPC_LANG_DEFAULT,
            "                                                       \
             number     : /-?[0-9]+/  ;                             \
             operator   : '+' | '-' | '*' | '/'  ;                  \
             expr       : <number> | '(' <operator> <expr>+ ')'  ;  \
             lispc      : /^/ <operator> <expr>+ /$/  ;             \
            ",
            Number, Operator, Expr, Lispc);

    puts("Lispc version 0.0.1");
    puts("Press Ctrl+C to exit\n");

    /* Do the main loop for REPL */
    while (1) {
        char* input = readline("lispc > ");
        add_history(input);

        /* Process the input */
        mpc_result_t parse_result;
        if (mpc_parse("<stdin>", input, Lispc, &parse_result)) {

            /* Evaluate and print the result */
            lval result = eval(parse_result.output);
            lval_println(result);

            mpc_ast_delete(parse_result.output);
        }
        else {
            mpc_err_print(parse_result.error);
            mpc_err_delete(parse_result.error);
        }

        free(input);
    }

    /* Clean up the parsers */
    mpc_cleanup(4, Number, Operator, Expr, Lispc);

    return 0;
}
