#include <stdio.h>
#include <string.h>
#include <unistd.h>
// #include <sys/types.h>
// #include <signal.h> //these are for kill
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h> //for open

void removeWhiteSpace(char **inputPtr){
    char *input = *inputPtr;
    int front = 0;
    while(front < strlen(input) && isspace(input[front])) {
        front++;
    }

    int back = strlen(input)-1;
    while (back >= 0 && isspace(input[back])) {
        back--;
    }

    input[back+1] = '\0';  // Remove trailing whitespace

    *inputPtr = input + front;  // Adjust the original pointer to skip leading whitespace
}

int syntaxCheck(char* input){
    // make sure < can only be before any | and > can only be after any |
    // make sure & can only be at the very end

    char *leMaxlocation = NULL; //location of the last <
    char *geMaxlocation = NULL; //location of the last >
    char *pipeMinlocation = NULL; //location of the first |
    char *pipeMaxlocation = NULL; //location of the last |
    char *ampMaxlocation = NULL; //location of the last &

    int i = 0;
    while(i < strlen(input)){
        if(input[i] == '<'){
            if(leMaxlocation != NULL) return 1;//multiple <
            leMaxlocation = &input[i];
        }else if(input[i] == '>'){
            if (geMaxlocation != NULL) return 1;//multiple >
            geMaxlocation = &input[i];
        }else if(input[i] == '|'){
            if(pipeMinlocation == NULL) pipeMinlocation = &input[i];
            pipeMaxlocation = &input[i];
        }else if(input[i] == '&'){
            if(ampMaxlocation == NULL) ampMaxlocation = &input[i];
            else return 1;//multiple &
        }
        i++;
    }
    if(leMaxlocation != NULL && pipeMinlocation != NULL && leMaxlocation > pipeMinlocation) return 1;//< after |
    if(geMaxlocation != NULL && pipeMaxlocation != NULL && geMaxlocation < pipeMaxlocation) return 1;//> before |
    if(ampMaxlocation != NULL && ampMaxlocation != &input[strlen(input)-1]) return 1;//& not at the end
    //> before <
    if(leMaxlocation != NULL && geMaxlocation != NULL && leMaxlocation > geMaxlocation) return 1;//< after >

    return 0;
}

int main(int argc, char ** argv){
    bool noArg = false;

    int flag = 0;
    while (flag < argc){

        if(strcmp(argv[flag], "-n")==0) noArg = true;
        flag++;
    }
    int saved_stdin = dup(STDIN_FILENO);
    while (1) {
        dup2(saved_stdin, STDIN_FILENO);
        int status;
        if(!noArg) printf("my_shell$ ");
        
        // Read command from standard input
        
        
        
        char input[512];//GIVEN:max character <= 512
        char *inputPtr = input;
        if(fgets(input, 512, stdin)== NULL){
            //printf("\nexiting my_shell\n");
            //printf("\nline 28\n");
            break;
        }

        input[strlen(input) - 1] = '\0'; //remove the newline character
        removeWhiteSpace(&inputPtr);
        if (syntaxCheck(inputPtr) == 1) {
            printf("ERROR: Syntax error\n");
            continue;
        }
        int background = 0;
        if (input[strlen(input)-1]== '&') {
            background = 1;
            input[strlen(input) - 1] = '\0';//remove the & character
        }
        
        if (!strcmp(input, "exit")){ //if input is exit, exit the shell now
            //printf("\nexiting my_shell\n");
            break;
        }


        
        // Parse command using strtok
        char *token = strtok(input, "|");
        
        

        char **commands = NULL;
        size_t numOfCommands = 0;
        while(token != NULL){
            removeWhiteSpace(&token);

            commands = realloc(commands, sizeof(char*) * (numOfCommands+1));
            
            commands[numOfCommands++] = strdup(token);
            //printf("%s\n", commands[numOfCommands]);//print the command for minideadline
            token = strtok(NULL, "|");
        }

        
        //strace??
        //execute the first command using execvp
        int currentCommand = 0;
        // char pipeBuffer[512] = "";
        // pipeBuffer[0] = '\0';
        
        //Ececute the command(s)===============================================
        while(currentCommand < numOfCommands){
            char * argument [32]; // how long should the argument be??
            size_t arguments = 0;
            //memset(argument, 0, sizeof(argument));
            

            //output redirect ================================================
            //if at the last command, check for output redirect by looking for '>'
            //then parse >
            //command > file   (command might be the first command so may have input redirect, which is why we check output redirect before hand)
            //update the current command to be the command before >
            int outputRedirect = 0;
            char *outputFile = NULL;
            if(currentCommand + 1 == numOfCommands){
                
                if(strchr(commands[currentCommand], '>') != NULL){
                    //printf("currentCommand: %d\n", currentCommand);
                    //printf("numOfCommands: %zu\n", numOfCommands);
                    //printf("currentCommand + 1 == numOfCommands\n");
                    //printf("currentCommand: %d\n", currentCommand);
                    //printf("numOfCommands: %zu\n", numOfCommands);
                    outputRedirect = 1;
                    //printf("output will be redirexted\n");
                    char *commandCopy = strdup(commands[currentCommand]);
                    if (commandCopy == NULL) {
                        perror("ERROR: strdup failed");
                        exit(1);
                    }
                    char *firstCommand = strtok(commandCopy, ">"); // Parse the command before >
                    removeWhiteSpace(&firstCommand);
                    commands[currentCommand] = strdup(firstCommand);  // Store the command in currentCommand
                    firstCommand = strtok(NULL, ">"); // Parse the file after >
                    removeWhiteSpace(&firstCommand);
                    outputFile = strdup(firstCommand); // Store the file name in outputFile

                    free(commandCopy);  // Free the duplicated string

                }
            }


            //input redirect ================================================
            int inputRedirect = 0;
            char *inputFile = NULL;
            if(currentCommand == 0){
                //first check if there is input redirection by looking for '<'
                //then parse <
                //command < file
                //update the current command to be the command before <



                //printf("99\n");
                if(strchr(commands[currentCommand], '<') != NULL){
                    //printf("101\n");
                    inputRedirect = 1;
                    char *commandCopy = strdup(commands[currentCommand]);
                    if (commandCopy == NULL) {
                        perror("ERROR: strdup failed");
                        exit(1);
                    }
                    char *firstCommand = strtok(commandCopy, "<"); // Parse the command before <
                    removeWhiteSpace(&firstCommand);
                    commands[currentCommand] = strdup(firstCommand);  // Store the command in currentCommand
                    firstCommand = strtok(NULL, "<"); // Parse the file after <
                    removeWhiteSpace(&firstCommand);
                    inputFile = strdup(firstCommand); // Store the file name in inputFile
                    free(commandCopy);  // Free the duplicated string
                }
            }

                char *token2 = strtok(commands[currentCommand], " ");//parse the first command for execution
                
                while(token2 != NULL){
                    removeWhiteSpace(&token2);
                    argument[arguments] = strdup(token2);
                    token2 = strtok(NULL, " "); //parse out the first word for exevp
                    
                    arguments++;
                }

                argument[arguments] = NULL;

            int fd[2];
            if (pipe(fd) == -1) {
                perror("ERROR: Pipe failed\n");
                exit(1);
            }
            pid_t pid = fork();
            if(pid == 0){ //child process
                close(fd[0]);
                
                if (currentCommand == 0 && inputRedirect == 1) {  // First command with input redirection
                    int inputRedirectFile = open(inputFile, O_RDONLY);
                    if (inputRedirectFile < 0) {
                        perror("ERROR: open error");
                        exit(1);
                    }
                    if (dup2(inputRedirectFile, STDIN_FILENO) < 0) {
                        perror("ERROR: dup2 error");
                        exit(1);
                    }
                    close(inputRedirectFile);
                }

                if(currentCommand + 1 < numOfCommands) { // Not the last command
                    dup2(fd[1], STDOUT_FILENO); //pipe to the next command
                }

                if (currentCommand + 1 >= numOfCommands && outputRedirect == 1) {  // Last command with output redirection
                    int outputRedirectFile = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    if (outputRedirectFile < 0) {
                        perror("ERROR: open error");
                        exit(1);
                    }
                    if (dup2(outputRedirectFile, STDOUT_FILENO) < 0) {
                        perror("ERROR: dup2 error");
                        exit(1);
                    }
                    close(outputRedirectFile);
                    printf("outputRedirectFile");
                }
                // else if(background == 1){
                //     //printf("background\n");
                //     //back ground process
                //     //direct output to /dev/null
                //     int devNull = open("/dev/null", O_WRONLY);
                //     dup2(devNull, STDOUT_FILENO);
                //     close(devNull);
                // }
            close(fd[1]);
                //printf("%s\n%s\n", argument[0],argument[1]);
                //=================================================================
            //printf("Command: %s\n", argument[0]);
            // size_t i = 0;
            // while (argument[i] != NULL) {
            //     printf("argument[%zu]: %s\n", i, argument[i]);
            //     i++;
            // }
            //    =================================================================

            if (execvp(argument[0], argument)== -1) {
                //printf("execvp, something is wrong\n");
                perror("ERROR: execvp");
                exit(1);
            }   
                
                exit(0);
            }else{
            //parent process
                if(background == 0) waitpid(pid, &status, 0);
                if (currentCommand > 0) {
            // close the read end of the previous pipe
                    //close(STDIN_FILENO);
                }
                if (currentCommand + 1 < numOfCommands) {
                    
                    dup2(fd[0], STDIN_FILENO);
                }
                
                close(fd[0]);
                close(fd[1]);


                if(currentCommand + 1 == numOfCommands) {
                    //printf("%s", pipeBuffer);
                    fflush(stdout);
                    free(commands);
                }
                currentCommand++;
            }
        }


    }

    //clean up here
    //printf("\nI got out!x\n");
    exit(0);
    return 0;
}