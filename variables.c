#include <stdio.h>
#include <stdlib.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "mpc.h"

/**
 * Forward declarations
 */

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;


/**
 * Lispc value types
 */

/* Enumeration of all possible type of lval types */
enum { LVAL_NUM, LVAL_SYM, LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR, LVAL_ERR };

/* Declare function type */
typedef lval*(*lbuiltin)(lenv*, lval*);

/* Declare lisp value (lval) struct */
struct lval {
    /* The type of the value, the possible values are listed in enum above */
    int   type;

    long  num;
    char* err;
    char* sym;
    lbuiltin fun;

    /* Count and pointer to list of `lval*` */
    int    count;
    lval** cell;
};

struct lenv {
    int    count;
    char** syms;
    lval** vals;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Constructors and destructor
 */

/* Constructor for number-typed lval */
lval* lval_num(long x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num  = x;

    return v;
}

/* Constructor for error-typed lval */
lval* lval_err(char* m) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err  = malloc(strlen(m) + 1);
    strcpy(v->err, m);

    return v;
}

/* Constructor for symbol-typed lval */
lval* lval_sym(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym  = malloc(sizeof(s) + 1);
    strcpy(v->sym, s);

    return v;
}

/* Constructor for function-typed lval */
lval* lval_fun(lbuiltin func) {
    lval* v  = malloc(sizeof(lval));
    v->type  = LVAL_FUN;
    v->fun   = func;

    return v;
}

/* Constructor for s-expression-typed lval */
lval* lval_sexpr(void) {
    lval* v  = malloc(sizeof(lval));
    v->type  = LVAL_SEXPR;
    v->count = 0;
    v->cell  = NULL;

    return v;
}

/* Constructor for q-expression-typed lval */
lval* lval_qexpr(void) {
    lval* v  = malloc(sizeof(lval));
    v->type  = LVAL_QEXPR;
    v->count = 0;
    v->cell  = NULL;

    return v;
}

/* Destructor for lval */
void lval_del(lval* v) {
    /* We need to free all the malloc-ed variables inside the lval first */
    switch (v->type) {
        /* Num-typed lval doesn't allocate any memory, so `break` */
        case LVAL_NUM: break;

        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;
        case LVAL_FUN: break;

        /* For sexpr-typed and qexpr-typed lval, we need to recursively delete its contents */
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            for (int i = 0; i < v->count; ++i) {
                lval_del(v->cell[i]);
            }
            /* Don't forget to free the memory allocated to store pointers */
            free(v->cell);
            break;
    }

    /* Finally, we can safely free the lval itself */
    free(v);
}

/* Constructor for environment */
lenv* lenv_new(void) {
    lenv* e  = malloc(sizeof(lenv));
    e->count = 0;
    e->syms  = NULL;
    e->vals  = NULL;

    return e;
}

void lenv_del(lenv* e) {
    for (int i = 0; i < e->count; ++i) {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }

    free(e->syms);
    free(e->vals);
    free(e);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

lval* lval_copy(lval* v) {
    
    /* Create the new lval */
    lval* x = malloc(sizeof(lval));
    x->type = v->type;

    switch (v->type) {

        /* Numbers and functions are copied directly */
        case LVAL_NUM: x->num = v->num; break;
        case LVAL_FUN: x->fun = v->fun; break;

        /* String-backed type are copied using strcpy */
        case LVAL_SYM: x->sym = malloc(strlen(v->sym) + 1); strcpy(x->sym, v->sym); break;
        case LVAL_ERR: x->err = malloc(strlen(v->err) + 1); strcpy(x->err, v->err); break;

        /* Copy iist-type value by copying each sub-expressions */
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            x->cell  = malloc(sizeof(lval*) * x->count);
            for (int i = 0; i < v->count; ++i) {
                x->cell[i] = lval_copy(v->cell[i]);
            }
            break;
    }

    return x;
}

/* Add lval to a s-expression */
lval* lval_add(lval* v, lval* x) {

    /* Increase the count of contained elements */
    v->count++;
    /* Re-allocate memory to fit the needs */
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    /* Finally, really add the new lval */
    v->cell[v->count - 1] = x;

    return v;
}

/* Read and parse number */
lval* lval_read_num(mpc_ast_t* t) {

    /* Reset the error number */
    errno = 0;

    long x = strtol(t->contents, NULL, 10);

    return errno != ERANGE ? lval_num(x) : lval_err("Invalid number");
}

/* Read and parse the AST */
lval* lval_read(mpc_ast_t* t) {
    /* If it's a Number and Symbol, return its counterpart representation */
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

    /* If it's a root, or an s-expression, or an q-expression, then create a new empty list */
    lval* x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
    if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
    if (strstr(t->tag, "qexpr"))  { x = lval_qexpr(); }

    /* Fill the emptly list created above with any valid expression contained within */
    for (int i = 0; i < t->children_num; ++i) {

        /* Drop all the "meaningless" AST before further processing */
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
        if (strcmp(t->children[i]->tag, "regex") == 0)  { continue; }

        /* Do the real processing of children */
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

void lval_print(lval* v);

/* Print the whole expression */
void lval_expr_print(lval* v, char open, char close) {
    putchar(open);

    for (int i = 0; i < v->count; ++i) {
        /* Print the current cell */
        lval_print(v->cell[i]);

        /* If current cell is *NOT* the last element, put trailing space */
        if (i != (v->count - 1)) {
            putchar(' ');
        }
    }

    putchar(close);
}

/* Print an lval */
void lval_print(lval* v) {
    switch (v->type) {
        case LVAL_NUM:   printf("%li", v->num);        break;
        case LVAL_ERR:   printf("Error: %s", v->err);  break;
        case LVAL_SYM:   printf("%s", v->sym);         break;
        case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
        case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
    }
}

/* Print an lval, followed by a newline */
void lval_println(lval* v) {
    lval_print(v);
    putchar('\n');
}

lval* lval_eval(lval* v);
lval* lval_pop(lval* v, int i);
lval* lval_take(lval* v, int i);

lval* builtin_op(lval* a, char* op) {

    /* Ensure all arguments are numbers */
    for (int i = 0; i < a->count; ++i) {
        if (a->cell[i]->type != LVAL_NUM) {
            lval_del(a);
            return lval_err("Cannot operate on non-number");
        }
    }

    /* We need to check the first element, so let's pop the first element */
    lval* x = lval_pop(a, 0);

    /* Perform unary negation */
    if ((strcmp(op, "-") == 0) && a->count == 0) { x->num = -x->num; }

    /* Process the remaining elements */
    while (a->count > 0) {

        /* Pop the next element */
        lval* y = lval_pop(a, 0);

        /* Perform the operation */
        if (strcmp(op, "+") == 0) { x->num += y->num; }
        if (strcmp(op, "-") == 0) { x->num -= y->num; }
        if (strcmp(op, "*") == 0) { x->num *= y->num; }
        if (strcmp(op, "/") == 0) {
            if (y->num == 0) {
                lval_del(x);
                lval_del(y);

                x = lval_err("Division by zero");
                break;
            }

            x->num /= y->num;
        }

        /* Delete the already-processed_element */
        lval_del(y);
    }

    /* Now that the expression has been processed, delete it */
    lval_del(a);

    return x;
}

/* Merge lval `y` into lval `x` */
lval* lval_join(lval* x, lval* y) {

    /* One-by-one pop element in `y` and insert it to `x` */
    while (y->count) { 
        x = lval_add(x, lval_pop(y, 0));
    }

    /* Delete the now empty `y` */
    lval_del(y);

    return x;
}

#define LASSERT(args, cond, err) if (!(cond)) { lval_del(args); return lval_err(err); }

/* Get the first element of an lval */
lval* builtin_head(lval* a) {
    /* Series of check for errors */
    LASSERT(a, (a->count == 1), "Function 'head' passed too many arguments");
    LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'head' passed incorrect types");
    LASSERT(a, (a->cell[0]->count != 0), "Function 'head' passed {}");

    /* Take the first argument */
    lval* v = lval_take(a, 0);

    /* Delete all "tail" elements */
    while (v->count > 1) { lval_del(lval_pop(v, 1)); }

    /* Return the "head" */
    return v;
}

/* Get all the elements of an lval except the first element */
lval* builtin_tail(lval* a) {
    /* Series of check for errors */
    LASSERT(a, (a->count == 1), "Function 'tail' passed too many arguments");
    LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'tail' passed incorrect types");
    LASSERT(a, (a->cell[0]->count != 0), "Function 'tail' passed {}");

    /* Take the first argument */
    lval* v = lval_take(a, 0);

    /* Delete the "head" */
    lval_del(lval_pop(v, 0));

    /* Return the "tail" */
    return v;
}

/* Convert an lval to a list. In other word, make an s-expression to be q-expression */
lval* builtin_list(lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

/* Eval an lval. In other word, make an q-expression to be s-expression */
lval* builtin_eval(lval* a) {
    LASSERT(a, (a->count == 1), "Function 'eval' passed too many arguments");
    LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'eval' passed incorrect type");

    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(x);
}

lval* builtin_join(lval* a) {
    for (int i = 0; i < a->count; ++i) {
        LASSERT(a, (a->cell[i]->type == LVAL_SEXPR), "Function join passed incorrect tyupe");
    }

    lval* x = lval_pop(a, 0);

    while (a->count) {
        x = lval_join(x, lval_pop(a, 0));
    }

    lval_del(a);
    return x;
}


lval* builtin(lval* a, char* func) {
    if (strcmp("list", func) == 0) { return builtin_list(a); }
    if (strcmp("head", func) == 0) { return builtin_head(a); }
    if (strcmp("tail", func) == 0) { return builtin_tail(a); }
    if (strcmp("join", func) == 0) { return builtin_join(a); }
    if (strcmp("eval", func) == 0) { return builtin_eval(a); }
    if (strstr("+-*/", func)) { return builtin_op(a, func); }

    lval_del(a);
    return lval_err("Unknown function");
}

/* Evaluate an s-expression */
lval* lval_eval_sexpr(lval* v) {

    /* Evaluate all children */
    for (int i = 0; i < v->count; ++i) {
        v->cell[i] = lval_eval(v->cell[i]);
    }

    /* Check for errors in all children */
    for (int i = 0; i < v->count; ++i) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }

    /* Return all empty expressions */
    if (v->count == 0) { return v; }

    /* Evaluate and return single expressions */
    if (v->count == 1) { return lval_take(v, 0); }

    /* Last check: ensure that the first element is a Symbol */
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_SYM) {
        lval_del(f);
        lval_del(v);

        return lval_err("S-expression does not start with the symbol");
    }

    /* Finally, get the result of expression */
    lval* result = builtin(v, f->sym);
    lval_del(f);

    return result;
}

/* Evaluate an `lval` */
lval* lval_eval(lval* v) {
    /* Check whether v is an s-expression. If it is so, eval it */
    if (v->type == LVAL_SEXPR) {
        return lval_eval_sexpr(v);
    }

    /* Other expression won't be touched */
    return v;
}

/* Pop an element inside lval v with index i */
lval* lval_pop(lval* v, int i) {
    lval* x = v->cell[i];

    /* Shift the memory */
    memmove(&v->cell[i], &v->cell[i + 1], sizeof(lval*) * (v->count - i - 1));

    /* Reduce the count of elements in the `v` */
    v->count--;

    /* Reallocate the needed memory to store the elements of `v` */
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);

    return x;
}

lval* lval_take(lval* v, int i) {
    lval* x = lval_pop(v, i);
    lval_del(v);

    return x;
}


int main(int argc, char** argv) {

    /* Create parsers */
    mpc_parser_t* Number    = mpc_new("number");
    mpc_parser_t* Symbol    = mpc_new("symbol");
    mpc_parser_t* Sexpr     = mpc_new("sexpr");
    mpc_parser_t* Qexpr     = mpc_new("qexpr");
    mpc_parser_t* Expr      = mpc_new("expr");
    mpc_parser_t* Lispc     = mpc_new("lispc");

    /* Define the language */
    mpca_lang(MPC_LANG_DEFAULT,
            "                                                       \
             number     : /-?[0-9]+/  ;                             \
             symbol     : /[a-aA-Z0-9_+\\-*\\/\\\\=<>!&]+/  ;       \
             sexpr      : '(' <expr>* ')'  ;                        \
             qexpr      : '{' <expr>* '}'  ;                        \
             expr       : <number> | <symbol> | <sexpr> | <qexpr> ; \
             lispc      : /^/ <expr>* /$/  ;                        \
            ",
            Number, Symbol, Sexpr, Qexpr, Expr, Lispc);

    puts("Lispc version 0.0.1");
    puts("Press Ctrl+C to exit\n");

    /* Do the main loop for REPL */
    while (1) {
        char* input = readline("lispc > ");
        add_history(input);

        /* Process the input */
        mpc_result_t res;
        if (mpc_parse("<stdin>", input, Lispc, &res)) {

            /* Read and parse the result */
            lval* x = lval_eval(lval_read(res.output));
            lval_println(x);
            lval_del(x);

            mpc_ast_delete(res.output);
        }
        else {
            mpc_err_print(res.error);
            mpc_err_delete(res.error);
        }

        free(input);
    }

    /* Clean up the parsers */
    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispc);

    return 0;
}
