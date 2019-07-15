#include "includes.h"

// Determine whether a file exists using lstat()
int file_exists(char* file_path) {
  struct stat buffer;
  int code = (lstat(file_path, &buffer));
  return code;
}

// ----------------------------------------------------------------------------

// Search through all known paths for executable name
//  - return 1 if the executable is found, store it in the buffer
//  - return 0 if it is not found
int find_executable(char* exec_name, char* buffer, char*** all_paths, int path_count) {
  if (*all_paths == NULL || buffer == NULL || path_count <= 0)
    return 0;

  for (int i = 0; i < path_count; i++) {
    int alloc_size = strlen(exec_name) + 2;
    char* executable = calloc(alloc_size, sizeof(char));
    executable[0] = '/';
    strcat(executable, exec_name);
    strcpy(buffer, (*all_paths)[i]);
    strcat(buffer, executable);
    free(executable);
    if (file_exists(buffer) == 0) {
      return 1;
    }
  }
  return 0;
}

// ----------------------------------------------------------------------------

int main(int argc, char** argv) {
  setvbuf(stdout, NULL, _IONBF, 0);                        // Disable buffered output for grading purposes
  int status;                                              // The status of the child process
  int pipe_buffer_one_cnt = 0;                             // Size of pipe_buffer_one
  int pipe_buffer_two_cnt = 0;                             // Size of pipe_buffer_two
  pid_t pid, child_a, child_b;                             // Pid values for processes
  char* cwd_buffer = make_buffer();                        // Current working directory
  char* path_buffer = make_buffer();                       // Buffer to hold the path to the executable
  char* input_commands = make_buffer();                    // Input commands from the user
  char* modified_input = make_buffer();                    // Holds the parsed input command
  char** pipe_buffer_one = calloc(128, sizeof(char *));    // Holds the args to execv() for read end
  char** pipe_buffer_two = calloc(128, sizeof(char *));    // Holds the args to execv() for write end
  struct command_info* command_data = new_command_info();  // Create command struct

  // Get all of the paths where executables are located
  // Assume no more than 128 paths are given in path variable
  char* path;
  if (getenv(MYPATH) == NULL)
    path = DEFAULT_PATH;
  else
    path = getenv(MYPATH);
  char** all_paths = calloc(128, sizeof(char *));
  int number_of_paths = get_all_paths(path, &all_paths);
  if (number_of_paths == -1) {
    fprintf(stderr, "ERROR: <path variable is null>\n");
    return EXIT_FAILURE;
  }

  // Array to track background pids, initialize to -1
  int background_process_count = 0;
  pid_t* background_processes = calloc(BUFFER_SIZE, sizeof(pid_t));
  for (int i = 0; i < BUFFER_SIZE; i++) {
    background_processes[i] = -1;
  }

  // Infinite loop until user exits or error
  while (1) {

    // Allocate new space for the command_data struct
    if (command_data != NULL) free_cmd_struct(command_data);
    command_data = new_command_info();

    // Check to see if background processes have terminated
    for (int k = 0; k < background_process_count; k++) {
      if (background_processes[k] == -1) {
        continue;
      } else {
        int p = background_processes[k];
        if (waitpid(p, &status, WNOHANG) == p) {
          int exit_status = WEXITSTATUS(status);
          printf("[process %d terminated with exit status %d]\n", p, exit_status);
          background_processes[k] = -1;
        }
      }
    }

    // Obtain the current working directory and user commands
    getcwd(cwd_buffer, BUFFER_SIZE);
    printf("%s$ ", cwd_buffer);
    fgets(input_commands, BUFFER_SIZE, stdin);

    // Parse the input coming from the user
    parse_input(command_data, input_commands, modified_input);

    // See if the executable exists or not, set shorter variable names
    int rc = find_executable(command_data->command, path_buffer, &all_paths, number_of_paths);
    char *cmd = command_data->command;
    char** args = command_data->arguments;
    int arg_count = command_data->arg_count;

    // Handle the input
    if (equal_strings(cmd, "cd")) { // Case for command being 'cd'
      char* cd_dir = make_buffer();
      strcpy(cd_dir, ".");
      if (arg_count == 0 || (arg_count == 1 && equal_strings(args[0], "~"))) {
        strcpy(cd_dir, getenv("HOME"));
      } else if (arg_count > 0 && args[0][0] == '~' && strlen(args[0]) > 1) {
        strcpy(cd_dir, getenv("HOME"));
        args[0][0] = '/';
        strcat(cd_dir, args[0]);
      } else if (arg_count > 0) {
        strcpy(cd_dir, args[0]);
      }
      int return_code = chdir(cd_dir);
      free(cd_dir);
      if (return_code != 0) { perror("chdir() failed"); }
      continue;
    } else if (strcmp(cmd, "exit") == 0) { // User wishes to exit
      printf("bye\n");
      break;
    } else if (rc != 1) { // Executable not found
      fprintf(stderr, "ERROR: command \"%s\" not found\n", command_data->command);
      continue;
    } else if (command_data->ampersand > 1) { // Invalid use of ampersand
      fprintf(stderr, "ERROR: <there can only be one &>\n");
      continue;
    } else if (command_data->pipe_count > 1) {
      fprintf(stderr, "ERROR: <this program supports at most 1 pipe>\n");
      continue;
    }

    // Pipe data from first command to the second command
    if (command_data->pipe_count == 1) {
      char* path_one = make_buffer();
      char* path_two = make_buffer();
      char* cmd_one = command_data->command;
      char* cmd_two = command_data->arguments[command_data->pipe_index];
      int rc1 = find_executable(cmd_one, path_one, &all_paths, number_of_paths);
      int rc2 = find_executable(cmd_two, path_two, &all_paths, number_of_paths);
      if (rc1 != 1) {
        fprintf(stderr, "ERROR: command \"%s\" not found\n", cmd_one);
        free(path_one); free(path_two);
        continue;
      } else if (rc2 != 1) {
        fprintf(stderr, "ERROR: command \"%s\" not found\n", cmd_two);
        free(path_one); free(path_two);
        continue;
      }

      // Begin the piping process and fork
      int p[2];
      int rc = pipe(p);
      if (rc == -1) { // Check to see if the pipe failed
        fprintf(stderr, "ERROR: <pipe() failed>\n");
        continue;
      }

      // Make the first fork and check its return code
      child_a = fork();
      if (child_a == -1) {
        perror("ERROR: <fork() failed>");
        continue;
      }

      // Write with the first child
      if (child_a == 0) {
        close(p[0]);
        dup2(p[1], STDOUT_FILENO);
        close(p[1]);
        pipe_buffer_one_cnt = command_data->pipe_index + 1;
        modify_args_array(args, cmd, pipe_buffer_one, pipe_buffer_one_cnt-1);
        execv(path_one, pipe_buffer_one);
      }

      // Enter the parent to fork and read from the pipe
      if (child_a > 0) {
        if (command_data->ampersand != 1) {
          waitpid(child_a, &status, 0);
        } else {
          printf("[running background process \"%s\"]\n", cmd);
          background_processes[background_process_count++] = child_a;
        }
        child_b = fork();
        if (child_b == -1) {
          perror("ERROR: <fork() failed>");
          continue;
        }
        if (child_b == 0) { // Read with child 2
          close(p[1]);
          dup2(p[0], STDIN_FILENO);
          close(p[0]);
          int start_index = command_data->pipe_index + 1;
          pipe_buffer_two_cnt = arg_count - start_index + 1;
          char* command = command_data->arguments[command_data->pipe_index];
          extract_read_args(args, command, pipe_buffer_two, start_index, arg_count);
          execv(path_two, pipe_buffer_two);
        }
        if (child_b > 0) { // Parent waits for its second child
          close(p[1]);
          if (command_data->ampersand != 1) {
            waitpid(child_b, &status, 0);
          } else {
            printf("[running background process \"%s\"]\n", command_data->arguments[command_data->pipe_index]);
            background_processes[background_process_count++] = child_b;
          }
          free(path_one);
          free(path_two);
          continue;
        }
      }
    }

    // Regular fork without piping
    pid = fork();
    if (pid == -1) {
      perror("ERROR: <fork() failed>");
      return EXIT_FAILURE;
    }
    int argv_arr_length = arg_count + 1;
    char** argv_arr = calloc(128, sizeof(char *));
    modify_args_array(args, command_data->command, argv_arr, arg_count);
    if (pid == 0) { // Child executes command
      execv(path_buffer, argv_arr);
    } else {
      if (command_data->ampersand == 1) { // Run in background
        printf("[running background process \"%s\"]\n", cmd);
        background_processes[background_process_count++] = pid;
      } else { // Run in foreground
        waitpid(pid, &status, 0);
      }
    }
    if (argv_arr != NULL)     free_char_pointers(argv_arr, argv_arr_length);
  }

  // Free allocated memory for parent process and return exit success
  if (cwd_buffer != NULL)           free(cwd_buffer);
  if (path_buffer != NULL)          free(path_buffer);
  if (input_commands != NULL)       free(input_commands);
  if (modified_input != NULL)       free(modified_input);
  if (background_processes != NULL) free(background_processes);
  if (command_data != NULL)         free_cmd_struct(command_data);
  if (all_paths != NULL)            free_char_pointers(all_paths, number_of_paths);
  if (pipe_buffer_one != NULL)      free_char_pointers(pipe_buffer_one, pipe_buffer_one_cnt);
  if (pipe_buffer_two != NULL)      free_char_pointers(pipe_buffer_two, pipe_buffer_two_cnt);
  return EXIT_SUCCESS;
}
