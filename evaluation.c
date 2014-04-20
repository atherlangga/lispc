#include <stdio.h>
#include <stdlib.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "mpc.h"

/* Evaluate two number based on the operator */
long eval_op(long x, char* op, long y) {
    if (strcmp(op, "+") == 0) {
        return x + y;
    }
    if (strcmp(op, "-") == 0) {
        return x - y;
    }
    if (strcmp(op, "*") == 0) {
        return x * y;
    }
    if (strcmp(op, "/") == 0) {
        return x / y;
    }

    return 0;
}

/* Evaluate the AST */
long eval(mpc_ast_t* ast) {

    /* If `ast` is a "number", return directly. Otherwise, we consider it as an operator. */
    if (strstr(ast->tag, "number")) {
        return atoi(ast->contents);
    }
    
    /* Get the operator */
    char* op = ast->children[1]->contents;

    /* Evaluate the third child */
    long x = eval(ast->children[2]);

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
            long result = eval(parse_result.output);
            printf("%li\n", result);

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
