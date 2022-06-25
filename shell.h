#ifndef SHELL_H
#define SHELL_H

#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>

#define MAX_NUM_OF_CMDS 20
#define MAX_NUM_OF_ARGS 20
#define MAX_ARG_LEN 50
#define BUFFERSIZE 1024
#define GREEN "\033[0;32m"
#define RESET "\033[0;37m"

struct command{
    int argc;
    char* argv[MAX_NUM_OF_ARGS];
    bool input_redirect;
    bool output_redirect;
    bool output_append;
    char input_file[MAX_ARG_LEN];
    char output_file[MAX_ARG_LEN];
    struct command* next;
};

struct command_pipeline{
    struct command* first;
    struct command* last;
    int num_of_cmds;
};

char* input;

char* read_command();
void create_pipeline();
void initialize_cmd(struct command** cmd_ptr);
void parse_cmd(struct command** cmd_ptr, char* input);
void insert_cmd(struct command_pipeline* pipeline, struct command* cmd);
void remove_commands(struct command_pipeline* pipeline);
void execute_commands(struct command_pipeline* pipeline, int* status);
bool is_string_number(char* str);
void close_all_pipes(int pipe_fd[][2], int count);
int shell_exit(char** args);
int shell_help(char** args);
int shell_cd(char** args);
int shell_num_builtins();

char* builtin_str[] = {"cd", "help", "exit"};
int (*builtin_func[]) (char**) = {&shell_cd, &shell_help, &shell_exit};

#endif