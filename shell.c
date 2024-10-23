#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

// Global flag for the 3 signals that we want to ignore.
int signalCatch = 0;

void signalHandler(int sig){
    signalCatch++;
    // Nothing more, just ignore the signal.
}

int main(int argc, char const *argv[])
{
    pid_t pid, wpid, tempPid;
    int status, status2;
    size_t length = 1000;
    char* currentDir = (char*) malloc(1024);
    char* lastPortionOfDir = (char*) malloc(1024);
    char* usrInput = (char*) malloc(1000);
    char* usrInputCopy = (char*) malloc(1000);
    char* tokenizedDir;
    char* fileLoc;
    char* suspendedJobsName[100] = {NULL};
    pid_t suspendedJobsID[100] = {0};
    int argumentsNum = 0;
    int searchUsrBin = 0;
    int i, j;
    int suspendedJobsNum = 0;
    char** args = (char**) malloc(1024);
    char** realArgs = (char**) malloc(1024);

    char* blanckSpc = " ";
    char* cmdFull;
    char* tempCmdFull;
    int cmdIndex;

    int input = 0, outputOW = 0, outputAP = 0, redirectionNum = 0, IOerror = 0;
    char *inputDest, *outputDest;

    //pipes
    int commandNum = 1;
    int pipeLoopFlg, forkFlg;
    int pipeErr = 0;
    char* seperateCmd = (char*) malloc(1024);
    char* seperateCmdCopy = (char*) malloc(1024);
    char* usrInputCopy2 = (char*) malloc(1024);
    char* seperateCmdCopy2 = (char*) malloc(1024);
    char* savedPtr1;

    //signal
    struct sigaction action;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    action.sa_handler = signalHandler;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGQUIT, &action, NULL);
    sigaction(SIGTSTP, &action, NULL);

    while (true){
        // Zero the flags out.
        commandNum = 1;
        forkFlg = 0;
        searchUsrBin = 0;
        argumentsNum = 0;
        input = 0;
        outputOW = 0;
        outputAP = 0;
        redirectionNum = 0;
        pipeErr = 0;
        IOerror = 0;

        // Print the dir prefix in the prompt. 
        getcwd(currentDir, 1024);
        tokenizedDir = strtok(currentDir, "/");
        if (tokenizedDir == NULL){
            lastPortionOfDir = "/"; 
        }
        while (tokenizedDir != NULL){
            lastPortionOfDir = tokenizedDir;
            tokenizedDir = strtok(NULL, "/");
        }

        // Get the user Input.
        printf("[nyush %s]$ ", lastPortionOfDir);
        fflush(stdout);
        if (ferror(stdin))
            clearerr(stdin);
        getline(&usrInput, &length, stdin);
        // Check if stdin is closed.
        if (feof(stdin)){
            for (i = 0; i < suspendedJobsNum; i++){
                tempPid = suspendedJobsID[i];
                kill(tempPid, SIGTERM);
                kill(tempPid, SIGKILL);
                waitpid(tempPid, NULL, 0);
                suspendedJobsID[i] = 0;
                free(suspendedJobsName[i]);
                suspendedJobsName[i] = NULL;
            }
            printf("\n");
            exit(0);
        }
        // Check if one the 3 signals was received.
        if (signalCatch){
            signalCatch = 0;
            continue;
        }
        // Truncate the malloc'ed strings.
        strcpy(usrInputCopy, usrInput);
        for (i = 0; i < 1000; i++){
            if (usrInput[i] == '\n')
                break;
        }
        usrInput[i] = '\0';
        usrInputCopy[i] = '\0';

        // Check if the user enters a blank line or just '\n"
        for (i = 0; i < strlen(usrInput); i++){
            if (usrInput[i] != ' ')
                break;
        }
        if (i == strlen(usrInput) || strlen(usrInput) == 0)
            continue;

        // Check if there are pipes
        for (i = 0; i < strlen(usrInput); i++){
            if (usrInput[i] == '|'){
                if (i == strlen(usrInput)-1 || i == 0)
                    pipeErr++;
                commandNum++;
            }
        }
        if (pipeErr){
            fprintf(stderr, "Error: invalid command\n");
            continue;
        }

        if (commandNum == 1){
            // Check if the user have entered an absolute (relative) path or not.
            tokenizedDir = strtok(usrInput, " ");
            if (strchr(tokenizedDir, '/') == NULL){
                fileLoc = malloc(strlen(tokenizedDir) + 10);
                strcpy(fileLoc, "/usr/bin/");
                strcat(fileLoc, tokenizedDir);
                searchUsrBin++;
            }

            // Check for arguments number.
            while (tokenizedDir != NULL){
                tokenizedDir = strtok(NULL, " ");
                if (tokenizedDir == NULL)
                    break;
                argumentsNum++;
            }

            // Prepare the argument string for execv().
            char* args[argumentsNum + 2];
            tokenizedDir = strtok(usrInputCopy, " ");

            // Implement the built-in command "cd".
            if (strcmp(tokenizedDir, "cd") == 0){
                if (argumentsNum == 0 || argumentsNum >= 2){
                    fprintf(stderr, "Error: invalid command\n");
                    continue;
                }
                if (chdir(strtok(NULL, " ")) != 0){
                    fprintf(stderr, "Error: invalid directory\n");
                    continue;
                }
                continue;
            }
            // Implement the built-in command "jobs"
            else if (strcmp(tokenizedDir, "jobs") == 0){
                if (argumentsNum != 0){
                    fprintf(stderr, "Error: invalid command\n");
                    continue;
                }
                for (i = 0; i < suspendedJobsNum; i++){
                    printf("[%d] %s\n", i+1, suspendedJobsName[i]);
                }
                continue;
            }
            // Implement the built-in command "fg"
            else if (strcmp(tokenizedDir, "fg") == 0){
                if (argumentsNum == 0 || argumentsNum >= 2){
                    fprintf(stderr, "Error: invalid command\n");
                    continue;
                }
                cmdIndex = atoi(strtok(NULL, " "));
                if (cmdIndex < 1 || cmdIndex > suspendedJobsNum){
                    fprintf(stderr, "Error: invalid job\n");
                    continue;
                }
                wpid = suspendedJobsID[cmdIndex-1];
                tempCmdFull = malloc(strlen(suspendedJobsName[cmdIndex-1]) + 1);
                strcpy(tempCmdFull, suspendedJobsName[cmdIndex-1]);
                for (i = cmdIndex-1; i < suspendedJobsNum; i++){
                    if (i == suspendedJobsNum-1){
                        suspendedJobsID[i] = 0;
                        free(suspendedJobsName[i]);
                        suspendedJobsName[i] = NULL;
                    }
                    else{
                        suspendedJobsID[i] = suspendedJobsID[i+1];
                        suspendedJobsName[i] = realloc(suspendedJobsName[i], strlen(suspendedJobsName[i+1])+1);
                        if (suspendedJobsName[i] == NULL) {
                            fprintf(stderr, "Error: realloc\n");
                            exit(1);
                        }
                        strcpy(suspendedJobsName[i], suspendedJobsName[i+1]);
                    }
                }
                --suspendedJobsNum;
                if (kill(wpid, SIGCONT) == -1){
                    fprintf(stderr, "Error: kill\n");
                    exit(1);
                }
                while (waitpid(wpid, &status2, WUNTRACED) == -1){
                }
                if (WIFSTOPPED(status2)){
                    suspendedJobsID[suspendedJobsNum] = wpid;
                    suspendedJobsName[suspendedJobsNum] = malloc(strlen(tempCmdFull)+1);
                    strcpy(suspendedJobsName[suspendedJobsNum], tempCmdFull);
                    ++suspendedJobsNum;
                }
                free(tempCmdFull);
                if (signalCatch)
                    signalCatch = 0;
                continue;
            }
            //Implement the built-in command "exit"
            else if (strcmp(tokenizedDir, "exit") == 0){
                if (argumentsNum > 0){
                    fprintf(stderr, "Error: invalid command\n");
                    continue;
                }
                if (suspendedJobsNum > 0){
                    fprintf(stderr, "Error: there are suspended jobs\n");
                    continue;
                }
                exit(0);
            }

            // Check the path is absolute or not, then prepare the args[] array.
            if (searchUsrBin == 0)
                args[0] = tokenizedDir;
            else
                args[0] = fileLoc;
            // Fix the args[0] problem
            realArgs[0] = tokenizedDir;
            args[argumentsNum + 1] = NULL;
            realArgs[argumentsNum + 1] = NULL;
            for(i = 1; i <= argumentsNum; i++){
                tokenizedDir = strtok(NULL, " ");
                if (tokenizedDir == NULL) 
                    break;
                args[i] = tokenizedDir;
                realArgs[i] = tokenizedDir;
            }

            // Determine if there is I/O redirection and the destination file.
            for (i = 1; i <= argumentsNum; i++){
                if (strcmp(args[i], "<") == 0){
                    if (i == argumentsNum)
                        IOerror++;
                    input++;
                    inputDest = args[i+1];
                    if (i < argumentsNum-1){
                        if ((strcmp(args[i+2], "<") != 0) && (strcmp(args[i+2], ">") != 0) && (strcmp(args[i+2], ">>") != 0))
                            IOerror++;
                    }
                    redirectionNum++;
                }
                else if (strcmp(args[i], ">") == 0){
                    if (i == argumentsNum)
                        IOerror++;
                    outputOW++;
                    outputDest = args[i+1];
                    if (i < argumentsNum-1){
                        if ((strcmp(args[i+2], "<") != 0) && (strcmp(args[i+2], ">") != 0) && (strcmp(args[i+2], ">>") != 0))
                            IOerror++;
                    }
                    redirectionNum++;
                }
                else if (strcmp(args[i], ">>") == 0){
                    if (i == argumentsNum)
                        IOerror++;
                    outputAP++;
                    outputDest = args[i+1];
                    if (i < argumentsNum-1){
                        if ((strcmp(args[i+2], "<") != 0) && (strcmp(args[i+2], ">") != 0) && (strcmp(args[i+2], ">>") != 0))
                            IOerror++;
                    }
                    redirectionNum++;
                }
            }

            // Check for I/O command error.
            if (input > 1 || outputAP > 1 || outputOW > 1 || (outputAP && outputOW) || IOerror){
                fprintf(stderr, "Error: invalid command\n");
                continue;
            }

            // execute the command.
            char* exePath = args[0];
            pid = fork();
            // Fork error
            if (pid < 0){
                fprintf(stderr, "Error: fork\n");
                exit(1);
            }
            // Child process.
            else if (pid == 0){
                // If we need to redirect I/O.
                if (input || outputAP || outputOW){
                    // Prepare the new arguments array.
                    int newArgumentsNum = argumentsNum - (redirectionNum*2);
                    char* newArgs[newArgumentsNum+2];
                    newArgs[0] = args[0];
                    for (i = 1; i <= newArgumentsNum; i++){
                        newArgs[i] = args[i];
                    }
                    newArgs[newArgumentsNum+1] = NULL;
                    // input redirection.
                    if (input){
                        int inputFd = open(inputDest, O_RDONLY);
                        if (inputFd < 0) {
                            fprintf(stderr, "Error: invalid file\n");
                            exit(1);
                        }
                        dup2(inputFd, 0);
                        close(inputFd);
                    }
                    // output redirection (Append).
                    if (outputAP){
                        int outputFd = open(outputDest, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
                        if (outputFd < 0) {
                            fprintf(stderr, "Error: invalid file\n");
                            exit(1);
                        }
                        dup2(outputFd, 1);
                        close(outputFd);
                    }
                    // output redirection (Overwrite).
                    if (outputOW){
                        int outputFd = open(outputDest, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                        if (outputFd < 0) {
                            fprintf(stderr, "Error: invalid file\n");
                            exit(1);
                        }
                        dup2(outputFd, 1);
                        close(outputFd);
                    }
                    execv(exePath, newArgs);
                    fprintf(stderr, "Error: invalid program\n");
                    exit(1);
                }
                // If no I/O redirection needed.
                execv(exePath, realArgs);
                fprintf(stderr, "Error: invalid program\n");
                exit(1);
            }
            // Parent process.
            else{
                while (waitpid(pid, &status, WUNTRACED) == -1){}
                // Check if the child is supended.
                if (WIFSTOPPED(status)){
                    // Get the full command name with arguments.
                    int cmdFullLen = strlen(args[0]);
                    for (i = 1; i <= argumentsNum; i++){
                        cmdFullLen += strlen(args[i]) + 1;
                    }
                    cmdFull = (char*) malloc(cmdFullLen + 1);
                    strcpy(cmdFull, args[0]);
                    for (i = 1; i <= argumentsNum; i++){
                        strcat(cmdFull, blanckSpc);
                        strcat(cmdFull, args[i]);
                    }
                    // Add the name and pid to the suspended jobs's list.
                    suspendedJobsID[suspendedJobsNum] = pid;
                    suspendedJobsName[suspendedJobsNum] = malloc(strlen(cmdFull) + 1);
                    strcpy(suspendedJobsName[suspendedJobsNum], cmdFull);
                    free(cmdFull);
                    ++suspendedJobsNum;
                }
                if (searchUsrBin != 0){
                    free(fileLoc);
                }
                // Check if one of the 3 signals was received.
                if (signalCatch){
                    signalCatch = 0;
                    continue;
                }
            }
        }
        // There are pipes.
        else{
            // Check if there are invalid file I/O in the pipe.
            strcpy(usrInputCopy2, usrInput);
            seperateCmdCopy2 = strtok(usrInputCopy2, "|");
            for (i = 0; i < commandNum; i++){
                for (j = 0; j < strlen(seperateCmdCopy2); j++){
                    if (i == 0 && seperateCmdCopy2[j] == '>')
                        pipeErr++;
                    if (i == commandNum-1 && seperateCmdCopy2[j] == '<')
                        pipeErr++;
                    if (i > 0 && i < commandNum-1 && (seperateCmdCopy2[j] == '>' || seperateCmdCopy2[j] == '<'))
                        pipeErr++;
                }
                seperateCmdCopy2 = strtok(NULL, "|");
            }
            if (pipeErr){
                fprintf(stderr, "Error: invalid command\n");
                continue;
            }

            int pipes[commandNum-1][2];
            for (i = 0; i < commandNum-1; i++){
                int pipe_return = pipe(pipes[i]);
                if (pipe_return < 0) {
                    fprintf(stderr, "Error: pipe\n");
                    exit(1);
                }
            }
            seperateCmd = strtok_r(usrInput, "|", &savedPtr1);
            for (pipeLoopFlg = 0; pipeLoopFlg < commandNum; pipeLoopFlg++){
                strcpy(seperateCmdCopy, seperateCmd);

                // Zero the flags out.
                searchUsrBin = 0;
                argumentsNum = 0;
                input = 0;
                outputOW = 0;
                outputAP = 0;
                redirectionNum = 0;

                // Check if the user have entered an absolute (relative) path or not.
                tokenizedDir = strtok(seperateCmd, " ");
                if (strchr(tokenizedDir, '/') == NULL){
                    fileLoc = malloc(strlen(tokenizedDir) + 10);
                    strcpy(fileLoc, "/usr/bin/");
                    strcat(fileLoc, tokenizedDir);
                    searchUsrBin++;
                }

                // Check for arguments number.
                while (tokenizedDir != NULL){
                    tokenizedDir = strtok(NULL, " ");
                    if (tokenizedDir == NULL)
                        break;
                    argumentsNum++;
                }

                // Prepare the argument string for execv().
                tokenizedDir = strtok(seperateCmdCopy, " ");
                // Check the path is absolute or not, then prepare the args[] array.
                if (searchUsrBin == 0)
                    args[0] = tokenizedDir;
                else
                    args[0] = fileLoc;
                args[argumentsNum + 1] = NULL;
                for(i = 1; i <= argumentsNum; i++){
                    tokenizedDir = strtok(NULL, " ");
                    if (tokenizedDir == NULL) 
                        break;
                    args[i] = tokenizedDir;
                }
                // Determine if there is I/O redirection and the destination file.
                for (i = 1; i <= argumentsNum; i++){
                    if (strcmp(args[i], "<") == 0){
                        input++;
                        inputDest = args[i+1];
                        redirectionNum++;
                    }
                    else if (strcmp(args[i], ">") == 0){
                        outputOW++;
                        outputDest = args[i+1];
                        redirectionNum++;
                    }
                    else if (strcmp(args[i], ">>") == 0){
                        outputAP++;
                        outputDest = args[i+1];
                        redirectionNum++;
                    }
                }
                // execute the command.
                char* exePath = args[0];
                forkFlg++;
                pid = fork();
                // Fork error
                if (pid < 0){
                    fprintf(stderr, "Error: fork\n");
                    exit(1);
                }
                // Child process.
                else if (pid == 0){
                    
                    // implement the pipes
                    if (pipeLoopFlg == 0){
                        dup2(pipes[0][1], 1);
                    }
                    else if (pipeLoopFlg == commandNum-1){
                        dup2(pipes[commandNum-2][0], 0);
                    }
                    else{
                        dup2(pipes[pipeLoopFlg-1][0], 0);
                        dup2(pipes[pipeLoopFlg][1], 1);
                    }

                    // Close all pipe fds
                    for (j = 0; j < commandNum-1; j++){
                        close(pipes[j][0]);
                        close(pipes[j][1]);
                    }

                    // If we need to redirect I/O.
                    if (input || outputAP || outputOW){
                        // Prepare the new arguments array.
                        int newArgumentsNum = argumentsNum - (redirectionNum*2);
                        char* newArgs[newArgumentsNum+2];
                        newArgs[0] = args[0];
                        for (i = 1; i <= newArgumentsNum; i++){
                            newArgs[i] = args[i];
                        }
                        newArgs[newArgumentsNum+1] = NULL;
                        // input redirection.
                        if (input){
                            int inputFd = open(inputDest, O_RDONLY);
                            if (inputFd < 0) {
                                fprintf(stderr, "Error: invalid file\n");
                                exit(1);
                            }
                            dup2(inputFd, 0);
                            close(inputFd);
                        }
                        // output redirection (Append).
                        if (outputAP){
                            int outputFd = open(outputDest, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
                            if (outputFd < 0) {
                                fprintf(stderr, "Error: invalid file\n");
                                exit(1);
                            }
                            dup2(outputFd, 1);
                            close(outputFd);
                        }
                        // output redirection (Overwrite).
                        if (outputOW){
                            int outputFd = open(outputDest, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                            if (outputFd < 0) {
                                fprintf(stderr, "Error: invalid file\n");
                                exit(1);
                            }
                            dup2(outputFd, 1);
                            close(outputFd);
                        }
                        execv(exePath, newArgs);
                        fprintf(stderr, "Error: invalid program\n");
                        exit(1);
                    }
                    // If no I/O redirection needed.
                    execv(exePath, args);
                    fprintf(stderr, "Error: invalid program\n");
                    exit(1);
                }
                seperateCmd = strtok_r(NULL, "|", &savedPtr1);
            }
            // Close all pipe fds
            for (i = 0; i < commandNum-1; i++) {
                close(pipes[i][0]);
                close(pipes[i][1]);
            }
            // Wait for all children to complete
            for (i = 0; i < commandNum; i++) {
                wait(NULL);
            }
            if (searchUsrBin != 0){
                free(fileLoc);
            }
            // Check if one of the 3 signals was received.
            if (signalCatch){
                signalCatch = 0;
                continue;
            }
        } 
    }
    return 0;
}

