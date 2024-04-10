
#include <stdlib.h>  //exit
#include <stdio.h>   //perror
#include <unistd.h>  //fork, pipe
#include <sys/wait.h>   //wait
#include <sys/types.h>
#include <assert.h>  // assert
#include <fcntl.h>   // O_RDWR, O_CREAT
#include <stdbool.h> // bool
#include <stdio.h>   // printf, getline
#include <stdlib.h>  // calloc
#include <string.h>  // strcmp
#include <unistd.h>  // execvp

#define MAXLINE 80
#define PROMPT "osh> "

const int BUF_SIZE = 4096; // constant variable
char hist[MAXLINE + 2];
bool equal(char *a, char *b) { return (strcmp(a, b) == 0); }

// read a line from console
// return length of line read or -1 if failed to read
// removes the \n on the line read
int fetchline(char **line) {
  size_t len = 0;
  size_t n = getline(line, &len, stdin);
  if (n > 0) {
    (*line)[n - 1] = '\0';
  }
  return n;
}
// ============================================================================
// Execute a child process.  
// Returns -1
// on failure.  On success, does not return to caller.
// ============================================================================

int child(char **args) {
  char *input_file = NULL;
  char *output_file = NULL;
  // Parse the arguments for input/output redirection
  for (int i = 0; args[i] != NULL; i++) {
      if (strcmp(args[i], ">") == 0) {
        output_file = args[i + 1];
        args[i] = NULL; // Nullify ">" to mark the end of command
        output_file = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (output_file == -1)
        {
        fprintf(stderr, "Error opening file for writing\n");
        return -1;
      }    
      // Redirect stdout to the file
      dup2(output_file, STDOUT_FILENO);

      // Remove ">" and the filename from the arguments list
      args[i] = NULL;
      args[i + 1] = NULL;
          break;
      } else if (strcmp(args[i], "<") == 0) {
          input_file = args[i + 1];
          args[i] = NULL; // Nullify "<" to mark the end of command
          int fd_input = open(input_file, O_RDONLY);
          if (fd_input < 0) {
              perror("Input file open failed");
              exit(EXIT_FAILURE);
          }
          dup2(fd_input, STDIN_FILENO); // Redirect stdin to input file
          close(fd_input); // Close file descriptor
          break;
      } else if (strcmp(args[i], "|") == 0){ 
          // Call the doPipe function
          doPipe(args, i);
          return 0; // No need to proceed further in this case
        }
  }
  // Execute command
  execvp(args[0], args);
  perror("execvp error");
  exit(EXIT_FAILURE);
}

// ============================================================================
// Execute the shell command that starts at args[start] and ends at args[end].
// For example, with
//   args = {"ls" "-l", ">", "junk", "&", "cat", "hello.c"}
// start = 5 and end = 6, we would perform the command "cat hello.c"
//
// Or, with args = {"ls", "|", "wc"} and start = 0 and end = 2, we would
// execute: ls | wc
// ============================================================================
void doCommand(char **args, int start, int end, bool waitfor)
{
    pid_t pid;
    pid = fork();
 if (pid < 0) {
   perror("Error during fork");
   exit(EXIT_FAILURE);
 }

 if (pid == 0) {
   // Child
   int sub_array_size = end - start + 1;
    // Allocate memory for the sub-array
    char **sub_args = (char *)malloc(sub_array_size * sizeof(char*));
    // Copy elements from the original array to the sub-array using memcpy
    memcpy(sub_args, args + start, sub_array_size * sizeof(char*));
    child(sub_args);
   
 } else {
   //Parent
   if (waitfor) {
        wait(NULL); // Wait for child process to finish if necessary
    }
 }
}

// ============================================================================
// Execute the two commands in 'args' connected by a pipe at args[pipei].
// For example, with
//   args = {"ls" "-a", "|", "wc""}
// and pipei = 2, we will perform the command "ls -a | wc", pipei is the index of the pipe, so you can split commands between parent and child.
//
// We split off the command for the parent from 'args' into 'parentArgs'.  In
// our example, parentArgs = {"ls", "-a"}
//
// The parent will write, via a pipe, to the child
// ============================================================================
int doPipe(char **args, int pipei){
    int pipefd[2];
    if(pipe(pipefd) < 0){
      perror("Error creating pipe ~ dopipe");
      exit(EXIT_FAILURE);
    }

    //fork child process
    pid_t pid = fork();
    if(pid == -1){
      perror("Error forking child ~ dopipe");
      exit(EXIT_FAILURE);
    }

    if(pid == 0){
      close(pipefd[0]); // close the read end of the pipe
      dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to the write end of the pipe
      close(pipefd[1]); // close the write end of the pipe

      char **commandArgs = malloc((pipei + 1) * sizeof(char *)); // create a new array to hold command and args

      int i = 0;
      while (equal(args[i], "|") == 0) {
        commandArgs[i] = strdup(args[i]);
        i++;
      }
      commandArgs[pipei] = NULL;

      execvp(commandArgs[0], commandArgs); // execute the command

    }else {
      close(pipefd[1]); // Close the write end of the pipe

      dup2(pipefd[0], STDIN_FILENO); // Redirect stdin to the read end of the pipe

      close(pipefd[0]); // Close the read end of the pipe

      execvp(args[pipei + 1], &args[pipei + 1]); // Execute the command after the pipe symbol

      waitpid(pid, NULL, 0); // Wait for child process to finish
    }
    return -1;
}

// ============================================================================
// Parse the shell command, starting at args[start].  For example, if
// start = 0 and args holds:
//    {"ls", "-a", ">", "junk.txt", "&", "cat", "hello.c", ";"}
// then parse will find the "&" terminator at index 4, and set end to this
// value.  This tells the caller that the current command runs from index
// 'start' thru index 'end'.
//
// We return true if the command terminates with ";" or end-of-line.  We
// return false if the command terminates with "&"
// ============================================================================
bool parse(char **args, int start, int *end)
{
  int i = start;
  while (args[i] != NULL) {
    if (strcmp(args[i], ";") == 0 || strcmp(args[i], "&") == 0) {
      *end = i - 1; // Set end index to the previous argument
      if (strcmp(args[i], "&") == 0) {
        return false; // Return false for background execution
      } else {
        return true; // Return true for foreground execution 
      }
    }
    i++;
  }
  *end = i - 1; // Set end index to the last argument
  return true; // Return true for end-of-line
}

// ============================================================================
// Tokenize 'line'.  Recall that 'strtok' writes NULLs into its string
// argument to delimit the end of each sub-string.
// Eg: line = "ls -a --color" would produce tokens: "ls", "-a" and "--color"
// ============================================================================
char **tokenize(char *line)
{
  char **tokens = (char **)malloc(BUF_SIZE * sizeof(char *));
  char *token;
  int i = 0;
  token = strtok(line, " ");
  while (token != NULL){
    tokens[i] = strdup(token);
    i++;
    token = strtok(NULL, " ");
  }
  tokens[i] == NULL;
  return tokens;
}
//ascii art
void printAsciiArt()
{
    printf("  |\\_/|        ****************************     (\\_/)\n");
    printf(" / @ @ \\       *  \"Purrrfectly pleasant\"  *    (='.'=)\n");
    printf("( > º < )      *     Shyam Ramesh         *    (\")_(\")\n");
    printf(" `>>x<<´       *        CSS430            *\n");
    printf(" /  O  \\       ****************************\n\n");
}

// ============================================================================
// Main loop for our Unix shell interpreter
// ============================================================================
int main()
{
  bool should_run = true;          // loop until false
  char *line = calloc(1, MAXLINE); // holds user input

  int start = 0; // index into args array
  while (should_run)
  {
    printf(PROMPT);   // osh>
    fflush(stdout);   // because no "\n"
    fetchline(&line); // fetch NUL-terminated line
    if (equal(line, ""))
      continue; // blank line

    if (equal(line, "exit"))
    { // cheerio
      should_run = false;
      continue;
    }
    if (equal(line, "ascii")){
      printAsciiArt();
    }
    if (equal(line, "!!"))
    {
      if (strlen(hist) == 0)
        {
            printf("No commands in history.\n");
            continue;
        }
        printf("Executing last command: %s\n", hist);
        strcpy(line, hist);
    }
    strncpy(hist, line, MAXLINE); 
    // process lines
    char **args = tokenize(line); // split string into tokens
    // loop over to find chunk of independent commands and execute
    while (args[start] != NULL)
    {
      int end;
      bool waitfor = parse(args, start, &end);// parse() checks if current command ends with ";" or "&"  or nothing. if it does not end with anything treat it as ; or blocking call. Parse updates "end" to the index of the last token before ; or & or simply nothing
      doCommand(args, start, end, waitfor);    // execute sub-command
      wait(NULL);
      start = end + 2;                         // next command
    }
    start = 0; 
  }
  return 0;
}


