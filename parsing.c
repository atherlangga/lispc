#include <stdio.h>
#include <stdlib.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "mpc.h"

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

    puts("Silang version 0.0.1");
    puts("Press Ctrl+C to exit\n");

    /* Do the main loop for REPL */
    while (1) {
        char* input = readline("lispc > ");
        add_history(input);

        /* Process the input */
        mpc_result_t result;
        if (mpc_parse("<stdin>", input, Lispc, &result)) {
            mpc_ast_print(result.output);
            mpc_ast_delete(result.output);
        }
        else {
            mpc_err_print(result.error);
            mpc_err_delete(result.error);
        }

        free(input);
    }

    /* Clean up the parsers */
    mpc_cleanup(4, Number, Operator, Expr, Lispy);

    return 0;
}
