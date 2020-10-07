/* SMP1: Simple Shell */

/* LIBRARY SECTION */
#include <ctype.h>              /* Character types                       */
#include <stdio.h>              /* Standard buffered input/output        */
#include <stdlib.h>             /* Standard library functions            */
#include <string.h>             /* String operations                     */
#include <sys/types.h>          /* Data types                            */
#include <sys/wait.h>           /* Declarations for waiting              */
#include <unistd.h>             /* Standard symbolic constants and types */

#include "smp1_tests.h"         /* Built-in test system                  */

/* DEFINE SECTION */
#define SHELL_BUFFER_SIZE 256   /* Size of the Shell input buffer        */
#define SHELL_MAX_ARGS 8        /* Maximum number of arguments parsed    */
#define SHELL_MAX_PREVIOUS 10   /* Maximum number of commands to be re-run or re-executed */

/* VARIABLE SECTION */
char previous_command[10][SHELL_BUFFER_SIZE];
int previous_command_count = 0;

enum { STATE_SPACE, STATE_NON_SPACE };  /* Parser states */

int counter = 1;

void update_previous_command(char *p)
{
        int i = 0;
        if (previous_command_count == SHELL_MAX_PREVIOUS)
        {
                for (i = 0; i < SHELL_MAX_PREVIOUS - 1; i++)
                {
                        memset(previous_command[i], 0, SHELL_BUFFER_SIZE);
                        strcpy(previous_command[i], previous_command[i + 1]);
                }
                strcpy(previous_command[i], p);
                previous_command_count++;
        }
        else
        {
                strcpy(previous_command[previous_command_count], p);
                previous_command_count++;
        }
}

int imthechild(const char *path_to_exec, char *const args[])
{
        /* TO-DO P5.1: Modified the "execv" for "execvp" so the program can run with only 'ls' only
        instead of using the whole path of '/bin/ls' */
        return execvp(path_to_exec, args) ? -1 : 0;
}

void imtheparent(pid_t child_pid, int run_in_background)
{
        int child_return_val, child_error_code;

        /* fork returned a positive pid so we are the parent */
        fprintf(stderr, "Parent says 'child process has been forked with pid=%d'\n", child_pid);

        if (run_in_background)
        {
                fprintf(stderr, "  Parent says 'run_in_background=1 ... so we're not waiting for the child'\n");
                return;
        }

        // TO-DO P5.4
        waitpid(child_pid, &child_return_val, 0);

        /* Use the WEXITSTATUS to extract the status code from the return value */
        child_error_code = WEXITSTATUS(child_return_val);
        fprintf(stderr, "Parent says 'wait() returned so the child with pid=%d is finished.'\n", child_pid);

        if (child_error_code != 0)
        {
                /* Error: Child process failed. Most likely a failed exec */
                fprintf(stderr, "Parent says 'Child process %d failed with code %d'\n", child_pid, child_error_code);
        }
        else
        {
                counter = counter + 1;
        }
}

/* MAIN PROCEDURE SECTION */
int main(int argc, char **argv)
{
        pid_t shell_pid, pid_from_fork;
        int n_read, i, exec_argc, parser_state, run_in_background;

        /* buffer: The Shell's input buffer. */
        char buffer[SHELL_BUFFER_SIZE];

        /* exec_argv: Arguments passed to exec call including NULL terminator. */
        char *exec_argv[SHELL_MAX_ARGS + 1];

        /* Variable for P5.2 */
        int command_counter = 1;

        /* Variable for P5.3 */
        char previous_load_buffer[9][SHELL_BUFFER_SIZE];

        /* Variable for P5.6 */
        int sub_shell = 0;

        /* Entrypoint for the testrunner program */
        if (argc > 1 && !strcmp(argv[1], "-test"))
        {
                return run_smp1_tests(argc - 1, argv + 1);
        }

        /* Allow the Shell prompt to display the pid of this process */
        shell_pid = getpid();

        while (1)
        {       /* The Shell runs in an infinite loop, processing input. */

                /* TO-DO P5.2: Shell modified with a counter that will be updating itself each time a command
                is being execute */
                fprintf(stdout, "Shell(pid=%d)%d> ", shell_pid, command_counter);
                fflush(stdout);

                /* Read a line of input. */
                if (fgets(buffer, SHELL_BUFFER_SIZE, stdin) == NULL)
                {
                        return EXIT_SUCCESS;
                }

                n_read = strlen(buffer);

                /* TO-DO P5.3: Modify the MP so it will be re-executing the n'th command entered.
                Only 9 values can be entered and can't be going far from that. */
                if (buffer[0] == '!')
                {
                        if (strlen(buffer) > 2 && strlen(buffer) < 4)
                        {
                                /* Variables that tracks the amount of commands that will be re-executed */
                                char previous_command_number = buffer[1];
                                int num = 0;
/*    */
                                if (previous_command_number > 48 && previous_command_number < 58)
                                {
                                        num = previous_command_number - 48;

                                        if (num > previous_command_number)
                                        {
                                                fprintf(stderr, "Incorrect\n");
                                                continue;
                                        }
                                        else
                                        {
                                                char optional[SHELL_BUFFER_SIZE] = "";
                                                strcpy(optional, previous_command[num - 1]);
                                                memset(buffer, 0, sizeof(buffer));
                                                strcpy(buffer, optional);
                                                n_read = strlen(buffer);
                                        }
                                }
                                else
                                {
                                        fprintf(stderr, "Incorrect\n");
                                        continue;
                                }
                        }
                        else
                        {
                                fprintf(stderr, "Incorrect\n");
                                continue;
                        }
 }
                else
                {
                        update_previous_command(buffer);
                }

                run_in_background = n_read > 2 && buffer[n_read - 2] == '&';
                buffer[n_read - run_in_background - 1] = '\n';

                if (buffer[0] == '!')
                {
                        int last_command = buffer[1] - '0';
                        if ((1 <= last_command) && (last_command <= 9) && (last_command < command_counter))
                        {
                                strncpy(buffer, previous_load_buffer[last_command - 1], SHELL_BUFFER_SIZE);
                        }
                        else
                        {
                                fprintf(stderr, "Not valid\n");
                                continue;
                        }
                }

                if (buffer[0] != '\n')
                {
                        if (command_counter <= 9)
                        {
                                strncpy(previous_load_buffer[command_counter - 1], buffer, SHELL_BUFFER_SIZE);
                        }
                        command_counter++;
                }

/* Parse the arguments: the first argument is the file or command *
                 * we want to run.                                                */

                parser_state = STATE_SPACE;
                for (exec_argc = 0, i = 0;
                     (buffer[i] != '\n') && (exec_argc < SHELL_MAX_ARGS); i++) {

                        if (!isspace(buffer[i])) {
                                if (parser_state == STATE_SPACE)
                                        exec_argv[exec_argc++] = &buffer[i];
                                parser_state = STATE_NON_SPACE;
                        } else {
                                buffer[i] = '\0';
                                parser_state = STATE_SPACE;
                        }
                }

/* run_in_background is 1 if the input line's last character *
                 * is an ampersand (indicating background execution).        */


                buffer[i] = '\0';       /* Terminate input, overwriting the '&' if it exists */

                /* If no command was given (empty line) the Shell just prints the prompt again */
                if (!exec_argc)
                        continue;
                /* Terminate the list of exec parameters with NULL */
                exec_argv[exec_argc] = NULL;

                /* If Shell runs 'exit' it exits the program. */
                if (!strcmp(exec_argv[0], "exit")) {
                        printf("Exiting process %d\n", shell_pid);
                        return EXIT_SUCCESS;    /* End Shell program */

                } else if (!strcmp(exec_argv[0], "cd") && exec_argc > 1) {
                /* Running 'cd' changes the Shell's working directory. */
                        /* Alternative: try chdir inside a forked child: if(fork() == 0) { */
                        if (chdir(exec_argv[1]))
                                /* Error: change directory failed */
                                fprintf(stderr, "cd: failed to chdir %s\n", exec_argv[1]);
                        /* End alternative: exit(EXIT_SUCCESS);} */

                } else {
 /* Execute Commands */
                        /* Try replacing 'fork()' with '0'.  What happens? */
                        pid_from_fork = fork();

                        if (pid_from_fork < 0)
                        {
                                /* Error: fork() failed.  Unlikely, but possible (e.g. OS *
                                 * kernel runs out of memory or process descriptors).     */
                                fprintf(stderr, "fork failed\n");
                                continue;
                        }
                        if (pid_from_fork == 0)
                        {
                                // TO-DO P5.6
                                if (!strcmp(exec_argv[0], "sub"))
                                {
                                        command_counter = 1;
                                        shell_pid = getpid();
                                        sub_shell++;
                                        if (sub_shell >= 3)
                                        {
                                                fprintf(stderr, "Too deep!\n");
                                                return 0;
                                        }
                                }
                                else
                                {
                                        return imthechild(exec_argv[0], &exec_argv[0]);
                                }
                                /* Exit from main. */
                        }
                        else
                        {
                                imtheparent(pid_from_fork, run_in_background);
                                /* Parent will continue around the loop. */
                        }
                } /* end if */
        } /* end while loop */

        return EXIT_SUCCESS;
} /* end main() */
