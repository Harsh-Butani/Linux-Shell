#include<stdio.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<wait.h>
#include<unistd.h>
#include<ctype.h>
#include<stdlib.h>
#include<string.h>
#include "shell.h"

int shell_num_builtins(){
    return sizeof(builtin_str) / sizeof(char*);
}

int shell_cd(char** args){
    if(args[1] == NULL){
        fprintf(stderr, "Expected argument to \"cd\"\n");
    }
    else if(chdir(args[1]) != 0){
        perror("chdir");
    }
    return 1;
}

int shell_help(char** args){
    printf("Custom made Shell\n");
    printf("Type program names and arguments, and hit enter\n");
    printf("The following are built in:\n");
    int num_builtins = shell_num_builtins();
    for(int i=0;i<num_builtins;i++){
        printf("  %s\n", builtin_str[i]);
    }
    printf("Use the man command for information on other programs.\n");
    return 1;
}

int shell_exit(char** args){
    return 0;
}

void close_all_pipes(int pipe_fd[][2], int count){
    for(int i=0;i<count;i++){
        close(pipe_fd[i][0]);
        close(pipe_fd[i][1]);
    }
}

bool is_string_number(char* str){
    int len = strlen(str);
    for(int i=0;i<len;i++){
        if(!isdigit(str[i])){
            return false;
        }
    }
    return true;
}

void execute_commands(struct command_pipeline* pipeline, int* status){
    if(pipeline->num_of_cmds == 0){
        return;
    }
    for(int i=0;i<shell_num_builtins();i++){
        if(!strcmp(pipeline->first->argv[0], builtin_str[i])){
            *status = (*builtin_func[i])(pipeline->first->argv);
            return;
        }
    }
    int count = pipeline->num_of_cmds;
    int pipe_fd[count-1][2];
    for(int i=0;i<count-1;i++){
        if(pipe(pipe_fd[i]) == -1){
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }
    struct command* cmd = pipeline->first;
    for(int i=0;i<count;i++){
        pid_t pid = fork();
        if(pid == -1){
            perror("fork");
            exit(EXIT_FAILURE);
        }
        if(pid == 0){
            if(i){
                if(dup2(pipe_fd[i-1][0], STDIN_FILENO) == -1){
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }
            if(i != count-1){
                if(dup2(pipe_fd[i-1][1], STDOUT_FILENO) == -1){
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }
            if(cmd->input_redirect == true && !is_string_number(cmd->input_file)){
                int fd_input = open(cmd->input_file, O_RDONLY);
                if(fd_input == -1){
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                if(dup2(fd_input, STDIN_FILENO) == -1){
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                if(close(fd_input) == -1){
                    perror("close");
                    exit(EXIT_FAILURE);
                }
            }
            if(cmd->output_redirect && !is_string_number(cmd->output_file)){
                int fd_output;
                if(cmd->output_append){
                    fd_output = open(cmd->output_file, O_APPEND | O_WRONLY | O_CREAT, 0777);
                }
                else{
                    fd_output = open(cmd->output_file, O_TRUNC | O_WRONLY | O_CREAT, 0777);
                }
                if(fd_output == -1){
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                if(dup2(fd_output, STDOUT_FILENO) == -1){
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                if(close(fd_output) == -1){
                    perror("close");
                    exit(EXIT_FAILURE);
                }
            }
            close_all_pipes(pipe_fd, count-1);
            if(execvp(cmd->argv[0], cmd->argv) == -1){
                perror("execvp");
                exit(EXIT_FAILURE);
            }
        }
        else{
            if(i>1){
                close(pipe_fd[i-1][0]);
            }
            close(pipe_fd[i][1]);
            if(wait(NULL) == -1){
                perror("wait");
                exit(EXIT_FAILURE);
            }
        }
        cmd = cmd->next;
    }
    close_all_pipes(pipe_fd, count-1);
}

void remove_commands(struct command_pipeline* pipeline){
    struct command *tmp;
    if(!pipeline){
        return;
    }
    for(tmp=pipeline->first;tmp!=NULL;tmp=tmp->next){
        free(tmp);
    }
    pipeline->first = NULL;
    pipeline->last = NULL;
    pipeline->num_of_cmds = 0;
}

void insert_cmd(struct command_pipeline* pipeline, struct command* cmd){
    if(!(pipeline->first)){
        pipeline->first = cmd;
        pipeline->last = cmd;
        return;
    }
    pipeline->last->next = cmd;
    pipeline->last = cmd;
}

void parse_cmd(struct command** cmd_ptr, char* input){
    int len = strlen(input);
    char* str = (char*) malloc(sizeof(char) * (len+1));
    if(!str){
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    char prev_str[MAX_ARG_LEN];
    int arg_size = 0;
    int num_args = 0;
    strcpy(prev_str, "");
    for(int i=0;i<=len;i++){
        if(i<len && !isspace(input[i])){
            str[arg_size++] = input[i];
        }
        else if(i==len || arg_size >= 1){
            str[arg_size]='\0';
            arg_size = 0;
            if(!strcmp(prev_str, "<")){
                strcpy((*cmd_ptr)->input_file, str);
                strcpy(prev_str, str);
                continue;
            }
            if(!strcmp(prev_str, ">") || !strcmp(prev_str, ">>")){      
                strcpy((*cmd_ptr)->output_file, str);
                strcpy(prev_str, str);
                continue;
            }
            strcpy(prev_str, str);
            if(!strcmp(str, "<")){
                (*cmd_ptr)->input_redirect = true;
                continue;
            }
            if(!strcmp(str, ">")){
                (*cmd_ptr)->output_redirect = true;
                (*cmd_ptr)->output_append = false;
                continue;
            }
            if(!strcmp(str, ">>")){
                (*cmd_ptr)->output_redirect = true;
                (*cmd_ptr)->output_append = true;
                continue;
            }
            (*cmd_ptr)->argv[num_args] = (char*)malloc(sizeof(char)*(arg_size + 5));
            if(!((*cmd_ptr)->argv[num_args])){
                perror("malloc");
                exit(EXIT_FAILURE);
            }
            strcpy((*cmd_ptr)->argv[num_args], str);
            num_args++;
        }
    }
    (*cmd_ptr)->argv[num_args] = NULL;
    (*cmd_ptr)->argc = num_args;
    free(str);
}

void initialize_cmd(struct command** cmd_ptr){
    *cmd_ptr = (struct command*) malloc(sizeof(struct command));
    if(*cmd_ptr == NULL){
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    (*cmd_ptr)->argc = 0;
    strcpy((*cmd_ptr)->input_file, "");
    strcpy((*cmd_ptr)->output_file, "");
    (*cmd_ptr)->input_redirect = 0;
    (*cmd_ptr)->output_redirect = 0;
    (*cmd_ptr)->output_append = 0;
    (*cmd_ptr)->next = NULL;
    for(int i=0;i<MAX_NUM_OF_ARGS;i++){
        (*cmd_ptr)->argv[i] = NULL;
    }
}

void create_pipeline(char* input, struct command_pipeline* pipeline){
    char* token = strtok(input, "|");
    int size = 0;
    while(token != NULL){
        struct command* cmd;
        initialize_cmd(&cmd);
        parse_cmd(&cmd, token);
        size++;
        insert_cmd(pipeline, cmd);
        pipeline->num_of_cmds = size;
        token = strtok(NULL, "|");
    }
}

char* read_command(){
    int buffersize = BUFFERSIZE;
    int position = 0;
    char* buffer = malloc(sizeof(char) * buffersize);
    int c;
    if(!buffer){
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    while(1){
        c = getchar();
        if(c == EOF || c == '\n'){
            buffer[position++] = '\0';
            return buffer;
        }
        else{
            buffer[position++] = c;
        }
        if(position >= buffersize){
            buffersize += BUFFERSIZE;
            buffer = realloc(buffer, sizeof(char) * buffersize);
            if(!buffer){
                perror("malloc");
                exit(EXIT_FAILURE);
            }
        }
    }
}

int main(int argc, char** argv){
    input = NULL;
    struct command_pipeline pipeline;
    pipeline.first = NULL;
    pipeline.last = NULL;
    pipeline.num_of_cmds = 0;
    int status = 1;
    while(status){
        printf(GREEN"Shell"RESET);
        printf(":$ ");
        input = read_command();
        create_pipeline(input, &pipeline);
        execute_commands(&pipeline, &status);
        remove_commands(&pipeline);
        free(input);
        input = NULL;
    }
}
