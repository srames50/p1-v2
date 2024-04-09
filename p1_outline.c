
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
    // Execute command
    execvp(args[0], args);
    perror("execvp error");
    exit(EXIT_FAILURE);
    return -1;
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
      printf("Parent exiting\n");
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
int doPipe(char **args, int pipei)
{

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
    if (strcmp(args[i], "&") == 0 || strcmp(args[i], ";") == 0) {
      *end = i - 1;
      if (strcmp(args[i], ";") == 0) {
          return true;
      } else {
          return false;
      }
    }
    ++i;
  }
  *end = i - 1;
  return true;
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
      start = end + 2;                         // next command
    }
    start = 0; 
               // next line
    // remember current command into history
  }
  return 0;
}


