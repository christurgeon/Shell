#include "includes.h"

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

// Allocates all of the data for a struct and returns the memory address of it
struct command_info* new_command_info() {
  struct command_info* new_struct = calloc(1, sizeof(struct command_info));
  if (new_struct == NULL)
    return NULL;
  new_struct->command = calloc(64, sizeof(char));         // The executable name
  new_struct->arguments = calloc(128, sizeof(char *));    // The arguments following
  new_struct->arg_count = 0;                              // The number of arguments
  new_struct->ampersand = 0;                              // Existance of ampersand (Holds number of them)
  new_struct->pipe_index = 0;                             // Holds the first pipe index
  new_struct->pipe_count = 0;                             // The number of pipes
  return new_struct;
};

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

// Allocates 1024 bytes of memory and returns a pointer to it
char* make_buffer() {
  return calloc(BUFFER_SIZE, sizeof(char));
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

// Frees an array of character pointers
void free_char_pointers(char** array, int size) {
  if (array == NULL)
    return;
  if (size < 0)
    return;
  for (int c = 0; c < size; c++) {
    if (array[c] != NULL)
      free(array[c]);
  }
  free(array);
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

// Parse the path string and return all of the paths
int get_all_paths(char* path, char*** all_paths) {
  if (path == NULL || *all_paths == NULL)
    return -1;

  // Create a buffer to hold each path as it is parsed
  char* buffer = make_buffer();
  int path_length = strlen(path);
  int buffer_curr = 0;
  int buffer_end = path_length - 1;

  // Loop through path variable and extract the paths
  int i;
  int path_count = 0;
  for (i = 0; i < path_length; i++)
  {
    // Found valid path, so add it and increment paths counter
    if (path[i] == ':') {
      buffer[buffer_curr] = '\0';
      (*all_paths)[path_count] = calloc(buffer_curr+1, sizeof(char));
      strcpy((*all_paths)[path_count], buffer);
      memset(buffer, 0, buffer_curr);
      buffer_curr = 0;
      path_count++;
    }
    // Reached end of path
    else if (i == buffer_end) {
      buffer[buffer_curr] = path[i];
      buffer[buffer_curr+1] = '\0';
      (*all_paths)[path_count] = calloc(buffer_curr+2, sizeof(char));
      strcpy((*all_paths)[path_count], buffer);
      path_count++;
    }
    // Add next character to the path
    else {
      buffer[buffer_curr] = path[i];
      buffer_curr++;
    }
  }
  free(buffer);
  return path_count;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

// This method takes in a string and removes the spaces
// in the beginning and at the end of the string.
void remove_excess_whitespace(const char* string, char* modified_string) {
  if (string == NULL || modified_string == NULL)
    return;

  // Get starting and ending index of actual string
  int i = 0;
  while (isspace(string[i])) i++;
  int j = strlen(string) - 1;
  while (isspace(string[j])) j--;

  // Resize the string and return it
  int index = 0;
  int k; for (k = i; k <= j; k++) {
    modified_string[index] = string[k];
    index++;
  }
  modified_string[index] = '\0';
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

// This method takes in the input line from the shell and parses the
// commands and loads the data into the 'command_info' struct
void parse_input(struct command_info* cmd, char* input, char* modified_input) {
  if (cmd == NULL || input == NULL || modified_input == NULL)
    return;

  // Remove the extra whitespace from the front and back
  int alloc_size = 64;
  char* command = calloc(alloc_size, sizeof(char));
  char* buffer = calloc(alloc_size, sizeof(char));
  remove_excess_whitespace(input, modified_input);
  int end = strlen(modified_input) - 1;
  int i, j = 0;

  // Add the command to 'command' and the list of args to 'arguments'
  for (i = 0 ; !isspace(modified_input[i]) && (i != end + 1) ; i++) {
    command[i] = modified_input[i];
  }
  command[i] = '\0';
  strcpy(cmd->command, command);
  for ( ; i < strlen(modified_input); i++) {
    // If character is part of command
    if (!isspace(modified_input[i])) {
      buffer[j] = modified_input[i];
      j++;
    }
    // If reached the end of a command
    if ((isspace(modified_input[i]) && strlen(buffer) > 0) || i == end) {
      buffer[j] = '\0';
      if (equal_strings(buffer, "&")) {
        cmd->ampersand++;
      } else if (equal_strings(buffer, "|")) {
        if (cmd->pipe_index == 0)
          cmd->pipe_index = cmd->arg_count;
        cmd->pipe_count++;
      } else {
        int cnt = cmd->arg_count;
        cmd->arguments[cnt] = calloc(alloc_size, sizeof(char));
        strcpy(cmd->arguments[cnt], buffer);
        cmd->arg_count++;
      }
      memset(buffer, 0, alloc_size);
      j = 0;
    }
  }
  free(command);
  free(buffer);
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

// Returns 1 if twp strings are equal, 0 otherwise
int equal_strings(const char* str1, const char* str2) {
  int eq = strcmp(str1, str2);
  if (eq == 0) {
    return 1;
  } else {
    return 0;
  }
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

// Free dynamically allocated memory associated with the struct
void free_cmd_struct(struct command_info* cmd) {
  free(cmd->command);
  for (int i = 0; i < cmd->arg_count; i++) {
    free(cmd->arguments[i]);
  }
  free(cmd->arguments);
  free(cmd);
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

// Add file path to buffer, then add all of the arguments to it
void modify_args_array(char** args, char* file_path, char** buffer, int arg_count) {
  buffer[0] = make_buffer();
  strcpy(buffer[0], file_path);
  for (int i = 0; i < arg_count; i++) {
    buffer[i+1] = make_buffer();
    strcat(buffer[i+1], args[i]);
  }
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

// Clear all of the command related data
void clear_command_data(struct command_info* cmd) {
  memset(cmd->command, 0, 64);
  memset(cmd->arguments, 0, 128);
  cmd->arg_count = 0;
  cmd->ampersand = 0;
  cmd->pipe_index = 0;
  cmd->pipe_count = 0;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

// Add file path to buffer, then all args for read end execv
void extract_read_args(char** args, char* file_path, char** buffer, int start, int end) {
  int i = 0;
  buffer[i] = make_buffer();
  strcpy(buffer[0], file_path);
  for (int j = start; j < end; j++) {
    buffer[++i] = make_buffer();
    strcpy(buffer[i], args[j]);
  }
}
