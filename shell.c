/*
	Based on code from http://dumbified.wordpress.com/2010/04/25/how-to-write-a-shell-in-c-using-fork-and-execv/
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdbool.h>
#include <sys/utsname.h>

#define BUFFSIZE 512
#define DEBUG false
#define DELIMITERS " \t"
#define SUCCESS 0
#define FAILURE 1
#define UNIVERSAL_ZERO { 0 }

//struct for built-in shell functions
struct builtin {
    const char* label;
    void (*op)(char**);  //function pointer to built-in's operation function
};

int countArgs(char* buffer) {

    int word_count = 0;
    bool inword = false;
    const char* args = buffer;

    do switch(*args) {
        case '\0':
        case ' ': case '\t':
           if(inword) { inword = false; word_count++; }
           break;
        default: inword = true;
    } while(*args++);

#if DEBUG
    printf("countArgs: Buffer contents\n");
    puts(buffer);
    printf("\ncountArgs: %d arguments\n", word_count);
#endif
    return word_count;
}

void parse(char* buffer, char** arguments) {

    char* parsed = strtok(buffer, DELIMITERS);  //Break buffer into words
    char* ch;
    int i = 0;  //Index for char** arguments

    while (parsed != NULL) {
        ch = strrchr(parsed,'\n');  //Find last occurance of newline.
         if(ch) {
            *ch = 0;  //Remove newline character
        }
        arguments[i] = parsed;

#if DEBUG
    printf("parse: parsed pointer: *%s*\n", parsed);
    printf("parse: Contents of arguments at index %d\n",i);
    puts(arguments[i]);
#endif
        i++;
        parsed = strtok(NULL, DELIMITERS);  //Increment to next word
    }
}

bool check_builtins(struct builtin* bfunc, char* buffer, int bfunc_size) {
    //returns true if buffer contains a built-in function, false otherwise.
    int arg_num = countArgs(buffer);
    char* arg[arg_num + 1];
    char input[BUFFSIZE] = UNIVERSAL_ZERO;
    int i;

    strcpy(input, buffer);
    arg[arg_num] = NULL;
    parse(input, arg);  //Note: parse tokenizes buffer passed to it.

    for(i = 0; i < bfunc_size; i++) {
        if(strlen(arg[0])== strlen((bfunc[i]).label)) {
            if (strcmp((bfunc[i]).label, arg[0]) == 0){
                (bfunc[i]).op(arg);
                return true;
            }
        }
    }
    return false;
}

void close_shell() {
    exit(SUCCESS);
}

void cd(char** arg) {
    chdir(arg[1]);   //will always be index 1 as 0 contains "cd".
}

bool valid_file(char* filename) {
    FILE* fptr = fopen(filename, "r");
    if (fptr != NULL)
    {
        fclose(fptr);
        return true;
    }
    return false;
}

void check_redirection(char** arguments) {
    char** arg = arguments;

    while(*arg) {
        //check for redirection of stdout with append feature.
        if(strcmp(*arg, ">>") == 0 ) {
            *arg = NULL;    arg++;
            if(*arg) {
                freopen(*arg, "a", stdout);
            }
            else {
                fprintf(stderr, "Error: No file to redirect to.\n");
                exit(FAILURE);
            }
        }

        //check for redirection of stdout.
        if(strcmp(*arg, ">") == 0) {
            *arg = NULL;    arg++;
            if(*arg) {
                freopen(*arg, "w", stdout);
            }
            else {
                fprintf(stderr, "Error: No file to redirect to.\n");
                exit(FAILURE); //kill child proccess
            }
       }

        //check for redirection of stdin.
        if(strcmp(*arg, "<") == 0) {
            *arg = NULL;    arg++;
            if(*arg) {
                if(valid_file(*arg)) {
                   freopen(*arg, "r", stdin);
                }
                else {
                    fprintf(stderr, "Error: %s does not exist.\n", *arg);
                    exit(FAILURE);
                }
            }
            else {
                fprintf(stderr, "Error: No file to redirect to.\n");
                exit(FAILURE);
            }
        }
        arg++;
    }
}

int main(int argc, char** argv) {

    struct builtin bfunc[] = {
        {.label = "exit", .op = &close_shell},
        {.label = "cd", .op = &cd}
    };
    int bfunc_size = (int) (sizeof(bfunc)/sizeof(bfunc[0]));

    //buffer is to hold user commands
    struct utsname ubuffer;
    char buffer[BUFFSIZE] = UNIVERSAL_ZERO;  //zero every elerment of the buffer
    char cwd[BUFFSIZE] = UNIVERSAL_ZERO;
    char username[BUFFSIZE] = UNIVERSAL_ZERO;
    char* path[] = {"/bin/", "/usr/bin/", 0};

    uname(&ubuffer);

    while(1)
    {
        //print the prompt
        if(getlogin_r(username, BUFFSIZE) == 0 &&
           getcwd(cwd, BUFFSIZE) != NULL) {
            printf("%s@%s:~%s$ ", username, ubuffer.nodename, cwd);
        }
        else {
            printf("myShell&gt: ");
        }

        fgets(buffer, BUFFSIZE, stdin);
        if(!check_builtins(bfunc, buffer, bfunc_size)) {
            int pid = fork();

            if(pid < 0) {
                fprintf(stderr, "Unable to fork new process.\n");
            }
            if(pid > 0) {
                //Parent code
                wait(NULL);
            }
            if(pid == 0) {
                //Child code
                int num_of_args = countArgs(buffer);
                //arguments to be passed to execv
                char* arguments[num_of_args+1];
                parse(buffer, arguments);

                //Requirement of execv
                arguments[num_of_args] = NULL;
                check_redirection(arguments);

                char prog[BUFFSIZE];
                char** path_p = path;

                while(*path_p) {
                    strcpy(prog, *path_p);

                    //Concancate the program name to path
                    strcat(prog, arguments[0]);
                    execv(prog, arguments);

                    path_p++;   //program not found. Try another path
                }

                //Following will only run if execv fails
                fprintf(stderr, "%s: Command not found.\n",arguments[0]);
                return FAILURE;
            }
        }
    }
    return SUCCESS;
}
