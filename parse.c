#include "shell.h"
#include "utils.h"
#include "parse.h"
#include "cd.h"
#include "ls.h"
#include "pwd.h"
#include "echo.h"
#include "pinfo.h"
#include "external.h"
#include "history.h"
#include "nightswatch.h"
#include "exit.h"
#include "env.h"
#include "jobs.h"
#include "kjob.h"
#include "overkill.h"
#include "fg.h"
#include "bg.h"
#include "redirection.h"
#include "pipe.h"
#include "recall.h"
#include "cronjob.h"

// commands are broken first by semi colons into separate commands,
// then by pipes, then an individual command and its input and output redirections are handled

void handle_tilda(int argc, char** argv) {
    // expands the tilda in front of every argument
    // replaces the string in case a tilda exists, old pointer never freed: source of memory leaks
    for (int i = 0; i < argc; i++) {
        argv[i] = tilda_expand(argv[i]);
    }
}

int handle_amp(char* command, int argc, char** argv) {
    // checks for ampersand appearing with or without a space before it
    // ignores the part of the command after the ampersand
    // returns updated size of argv
    // incase of ampersand in between, may overwrite some ponters in argv with pointers to newly
    // allocated strings: source of memory leaks

    // argv has enough space for 1 more argument

    // general idea: look for ampersands, either standalone, or at the end of arguments
    // if you find one, separate it into its own argument, place at the next index in argv
    // and return that as the number of arguments, ignoring everything after the ampersand

    size_t c_len = strlen(command);

    if (command[c_len-1] == '&') {
        command[c_len - 1] = '\0';
        argv[0] = (char*)malloc(sizeof(char) * 2);
        strcpy(argv[0], "&");
        return 1;
    }

    for (int i = 0; i < argc; i++) {
        size_t len = strlen(argv[i]);
        if (len == 0) {
            continue; // safety check, although this can't happen
        }
        if (argv[i][len-1] != '&') {
            continue;
        }
        // Now surely an ampersand

        if (len == 1) {
            // only ampersand
            return i + 1;
        } else {
            // more than ampersand
            argv[i][len - 1] = '\0';
            argv[i + 1] = (char*)malloc(sizeof(char) * 2);
            strcpy(argv[i + 1], "&");
            return i + 2;
        }
    }
    return argc;
}

void parse_command(char* str) {
    // handles an individual command, its input and output redirections
    // and whitespace separated arguments

    if (str == NULL)
        return;

    size_t len = strlen(str);

    // get the main command
    char* next = strtok(str, space_delim);
    // do not use str outside of strtok again now
    // next is not malloced memory, it is just a part of str
    // do not free next

    if (next == NULL)
        return;
    char* command = (char*)malloc(sizeof(char) * (strlen(next) + 1));
    strcpy(command, next);

    // initialise redirection
    redirect r;
    r.append = false;
    r.input = false;
    r.output = false;
    r.input_file[0] = '\0';
    r.output_file[0] = '\0';
    bool in_file_now = false;
    bool out_file_now = false;

    // separate out arguments and handle redirection symbols at once
    int argc = 0;
    char** argv = (char**)malloc(sizeof(char*) * (len+1)); // lots of space

    while((next = strtok(NULL, space_delim)) != NULL) {

        // after '<', '>', or '>>', expect a file
        if (strcmp(next, "<") == 0) {

            if (out_file_now || in_file_now) {
                fprintf(stderr, "Could not parse command: Parse error near '<'.\n");
                return; // TODO: FREE POINTERS
            }

            r.input = true;
            in_file_now = true;
            continue;

        } else if (strcmp(next, ">") == 0) {
            
            if (out_file_now || in_file_now) {
                fprintf(stderr, "Could not parse command: Parse error near '>'.\n");
                return;  // TODO: FREE POINTERS
            }

            r.output = true;
            out_file_now = true;
            r.append = false;
            continue;

        } else if (strcmp(next, ">>") == 0) {
            
            if (out_file_now || in_file_now) {
                fprintf(stderr, "Could not parse command: Parse error near '>>'.\n");
                return;  // TODO: FREE POINTERS
            }

            r.output = true;
            out_file_now = true;
            r.append = true;
            continue;
        }

        if (in_file_now) {
            strcpy(r.input_file, next);
            in_file_now = false;
            continue;
        } else if (out_file_now) {
            strcpy(r.output_file, next);
            out_file_now = false;
            continue;
        }

        argv[argc] = (char*)malloc(sizeof(char) * (strlen(next) + 1));
        strcpy(argv[argc], next);
        argc++;
    }

    // file not provided after '>', '>>' or '<'
    if (in_file_now || out_file_now) {
        fprintf(stderr, "Could not parse command: Invalid redirection syntax.\n");
        return; // TODO: FREE POINTERS
    }

    // changes argc, so all pointers in argv not freed in case
    // ampersand in between: source of memory leaks
    argc = handle_amp(command, argc, argv);
    handle_tilda(argc, argv);

    redirect_meta m = redirection_begin(r);
    if ((r.input && m.in_file == -1) || (r.output && m.out_file == -1)) {
        return; // ERROR PRINTED BY redirection_begin
    }

    if (strcmp(command, "echo") == 0) {
        handle_echo(argc, argv);
    } else if (strcmp(command, "ls") == 0) {
        handle_ls(argc, argv);
    } else if (strcmp(command, "pwd") == 0) {
        handle_pwd(argc, argv);
    } else if (strcmp(command, "cd") == 0) {
        handle_cd(argc, argv);
    } else if (strcmp(command, "pinfo") == 0) {
        handle_pinfo(argc, argv);
    } else if (strcmp(command, "history") == 0) {
        handle_history(argc, argv);
    } else if (strcmp(command, "nightswatch") == 0) {
        handle_nightswatch(argc, argv);
    } else if (strcmp(command, "quit") == 0) {
        handle_exit(argc, argv);
    } else if (strcmp(command, "setenv") == 0) {
        handle_setenv(argc, argv);
    } else if (strcmp(command, "unsetenv") == 0) {
        handle_unsetenv(argc, argv);
    } else if (strcmp(command, "jobs") == 0) {
        handle_jobs(argc, argv);
    } else if (strcmp(command, "kjob") == 0) {
        handle_kjob(argc, argv);
    } else if (strcmp(command, "overkill") == 0) {
        handle_overkill(argc, argv);
    } else if (strcmp(command, "fg") == 0) {
        handle_fg(argc, argv);
    } else if (strcmp(command, "bg") == 0) {
        handle_bg(argc, argv);
    } else if (strcmp(command, "cronjob") == 0) {
        handle_cronjob(argc, argv);
    } else if (is_recall(command)) {
        handle_recall(command, argc, argv);
    } else {
        handle_external(command, argc, argv);
    }

    redirection_end(r, m);

    free(command);
    for (int i = 0; i < argc; i++) {
        free(argv[i]);
    }
    free(argv);
}

void parse(char* input) {
    // handles separation of commands by semi-colon then passes onto pipe handler
    
    size_t len_array = count_chars(input, ';') + 1;
    //last one may not end with ';', so +1 for safety
    char** commands = (char**)malloc(sizeof(char*) * len_array);

    int i = 0;
    char* next = strtok(input, ";");
    while (next != NULL) {
        commands[i] = (char*)malloc(sizeof(char) * (strlen(next) + 1));
        strcpy(commands[i], next);
        i++;
        next = strtok(NULL, ";");
    }
    int num_commands = i; // actual number of commands
    
    // strtok does not malloc any memory. It returns parts of input string.
    // so I'm malloc-ing only as many times as the actual number of commands
    // so free only those many times
    
    for (i = 0; i < num_commands; i++) {
        if (!is_recall(commands[i])) {
            // not saved if it starts with up arrow
            store_history(commands[i]);
        }
        pied_piper(commands[i]);
        free(commands[i]);
    }
    free(commands);
}