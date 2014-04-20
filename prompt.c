#include <stdio.h>
#include <stdlib.h>

#include <readline/readline.h>
#include <readline/history.h>

static char input[2048];

int main(int argc, char** argv) {
    puts("Lispc version 0.0.1");
    puts("Press Ctrl+C to exit\n");

    /* Do the main loop for REPL */
    while (1) {
        char* input = readline("lispc > ");
        add_history(input);

        printf("No, you're a %s\n", input);
        
        free(input);
    }

    return 0;
}
