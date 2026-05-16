// mysh - a simple Unix shell

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

int main(void) {
    // character array that hold user input 
    char line[1024];
    int loop = 1;

    char *args[64]; // holds the tokens found in line 
    int arg_count; // tracks how many tokens found

    // I THINK THIS ALLOWS both < and > logic to work 
    char *filenameOutput = NULL; // gets file name if there is a ">"
    char *filenameInput = NULL; // gets file name if there is a "<"

    char *left_args[64];
    char *right_args[64];
    int pipe_index = -1;

    // a while loop that contiuosly prompts the user 
    while(loop == 1){
        // resest filenames after redirection 
        filenameOutput = NULL; 
        filenameInput = NULL; 

        //resets after grep
        pipe_index = -1;

        printf("mysh> ");
        
        fflush(stdout); // outputs all the data in the standard output buffer


        if (fgets(line, sizeof (line), stdin) == NULL){ //  reads a line of input from user. IF the user enter ctrl + d which then output a NULL then break from the loop
            printf("\n"); 

            break; // break out of the loop ( i don't know if this is the right syntax)
        }

        // removes trailing newline that happens after press Enter
        char *newline = strchr(line, '\n');  //strchr searches the varible line '\n' and returns the address of '\n' if found (if found it becomes true)
        if (newline) *newline = '\0'; // if found it replaces the variable at the newline address with a null terminator
        
        // Parsing step/ Tokenization
        arg_count = 0; 
        args[arg_count] = strtok(line, " "); //takes line as first input
        while(args[arg_count] != NULL){
            arg_count++;

            args[arg_count] = strtok(NULL, " "); // subsequent inputs are NULL

        } // args now contain each word as a separate string and final element is NULL -- exec family of function requres a NULL-terminated array of strings 

        // ---- DEBUG LOOP ----
        // for (int i = 0; i < arg_count; i++){
        //     printf("Token %d: %s\n", i, args[i]); // prints each element in the array of strings
        // }

        

        if(strcmp(args[0], "cd")== 0){ //cd must be handled in the parent process ie before the fork happens | strcmp - compares string (case sensitive) and 0 means strings are equal
            if (args[1] == NULL){
                chdir(getenv("HOME"));
            } else if (chdir(args[1]) == -1) {
                perror("cd failed");
            } else {
                chdir(args[1]);
            }
            continue;
        }

        if (strcmp(args[0], "exit") == 0){
            break;
        }

        for (int i = 0; i < arg_count; i++) {
            if (strcmp(args[i], "|") == 0) {
                pipe_index = i;
                args[i] = NULL; // split the array
                continue;
            }
        }

        if (pipe_index == -1){
            for (int i = 0; i < arg_count; i ++){
                //OUTPUT REDIRECTION
                if (strcmp(args[i], ">") == 0){ // check for ">" in the string array 
                    
                    if (args[i+1] == NULL){ // if there is no file name prints an error
                        printf("Error: no file specified\n");
                        break; 
                    }
                    filenameOutput = args[i+1]; // the next thing after > is the filename 
                    args[i] = NULL; // execvp should not see ">" as an argument
                    
                    

                    break;
                }

                
                //INPUT REDIRCTION
                if (strcmp(args[i], "<") == 0){ // check for ">" in the string array 
                    
                    if (args[i+1] == NULL){ // if there is no file name prints an error
                        printf("Error: no file specified\n");
                        break; 
                    }
                    filenameInput = args[i+1]; // the next thing after > is the filename 
                    args[i] = NULL; // execvp should not see ">" as an argument
                    
                    break;
                }
            }
        }

        for (int i = 0; i < arg_count; i++){
            printf("Token %d: %s\n", i, args[i]); // prints each element in the array of strings
        }


        if (pipe_index != -1 && (pipe_index == 0 || args[pipe_index + 1] == NULL)) {
            printf("Invalid pipe usage\n");
            continue;
        }

        if (pipe_index != -1) {
            int pipefd[2];
            pipe(pipefd); // pipefd[0] = read, pipefd[1] = write

            pid_t pid1 = fork();

            if (pid1 == 0) {
                // LEFT CHILD (ls)

                close(pipefd[0]); //  don't read

                dup2(pipefd[1], STDOUT_FILENO); // redirect stdout pipe
                close(pipefd[1]);

                execvp(args[0], args);
                perror("execvp failed");
                exit(1);
            }
            pid_t pid2 = fork();

            if (pid2 == 0) {
                // RIGHT CHILD (grep)

                close(pipefd[1]); //  don't write

                dup2(pipefd[0], STDIN_FILENO); // redirect stdin pipe
                close(pipefd[0]);

                execvp(args[pipe_index + 1], &args[pipe_index + 1]);
                perror("execvp failed");
                exit(1);
            }

            close(pipefd[0]);
            close(pipefd[1]);

            waitpid(pid1, NULL, 0);
            waitpid(pid2, NULL, 0);
            continue;
            
        }
            pid_t pid = fork(); // negative number mean fork failed, 0 means you are in the childs process, positive number means you are in the parent process and the # is the child's PID

            if (pid == 0){ // in the child process

                if (filenameOutput != NULL){ // only when you have a '>' and a filename is when this is called are writing to a file 
                    printf("DEBUG filenameOutput: %s\n", filenameOutput);
                    int fd = open(filenameOutput, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd < 0){
                        perror("file could not be created");
                        exit(1); // terminate the child
                    }
                    if (dup2(fd, STDOUT_FILENO) < 0){ // redirect stdout to the file 
                        perror ("dup2");
                        close(fd);
                        exit(1);
                    }

                    close(fd);
                } 
                if (filenameInput != NULL){ // mean you are reading a file 
                    int fd = open(filenameInput, O_RDONLY);
                    if (fd == -1){
                        perror("file does not exits");
                        exit(1); // terminate the child
                    }

                    dup2(fd, STDIN_FILENO); // redirect stdin to the file 
                    close(fd);
                }
                
                execvp(args[0], args); //1st argument is the program name 2nd argument is entire args array 
                perror("execvp failed"); // runs if it fails
                exit(1); // without this the child would continue to run throught the shell 
            } else if (pid > 0){ // in the parent process 
                waitpid(pid, NULL, 0); // 2nd argument is pointer to status variable... we dont care about exit code so we pass NULL 
                // 3rd argument is a flag field - 0 means block until child exits 
            } else if (pid < 0){ // fork failed
                perror("fork failed");
                continue;
            }
        
        
    }
    return 0;
}
