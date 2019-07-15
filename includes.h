#ifndef INCLUDES_H
#define INCLUDES_H

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define BUFFER_SIZE    1024
#define DEFAULT_PATH   "/bin:."
#define MYPATH         "MYPATH"

// Holds data about the input entered into the shell
struct command_info
{
  char* command;
  char** arguments;
  int arg_count;
  int ampersand;
  int pipe_index;
  int pipe_count;
};

struct command_info* new_command_info();
char* make_buffer();
void free_char_pointers(char** array, int size);
int get_all_paths(char* path, char*** all_paths);
void remove_excess_whitespace(const char* string, char* modified_string);
void parse_input(struct command_info* cmd, char* input, char* modified_input);
int pipe_found(struct command_info* cmd);
int equal_strings(const char* str1, const char* str2);
void free_cmd_struct(struct command_info* cmd);
void modify_args_array(char** args, char* file_path, char** buffer, int arg_count);
void clear_command_data(struct command_info* cmd);
void extract_read_args(char** args, char* file_path, char** buffer, int start, int end);

#endif
