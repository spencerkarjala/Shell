#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <math.h>
#include <errno.h>

#include "historyArray.h"

#define COMMAND_LENGTH 1024
#define NUM_TOKENS (COMMAND_LENGTH / 2 + 1)
#define PWD_BUF_LENGTH 512
#define MAX_HISTORY 10

void displayHistory(struct historyArray ha);
void signalHandler(int signalNumber);

int detectHardcodedCommands(char* pwd_buffer, char* input_buffer, char** tokens);
void getCurrentDirectory(char* pwd_buffer);
int detectHistoryCommands(struct historyArray* ha, 
                            char*  input_buffer, 
                            char** tokens);

struct historyArray* ha;


/**
 * Command Input and Processing
 */

/*
 * Tokenize the string in 'buff' into 'tokens'.
 * buff: Character array containing string to tokenize.
 *       Will be modified: all whitespace replaced with '\0'
 * tokens: array of pointers of size at least COMMAND_LENGTH/2 + 1.
 *       Will be modified so tokens[i] points to the i'th token
 *       in the string buff. All returned tokens will be non-empty.
 *       NOTE: pointers in tokens[] will all point into buff!
 *       Ends with a null pointer.
 * returns: number of tokens.
 */
int tokenize_command(char *buff, char *tokens[])
{
	int token_count = 0;
	_Bool in_token = false;
	int num_chars = strnlen(buff, COMMAND_LENGTH);
	for (int i = 0; i < num_chars; i++) {
		switch (buff[i]) {
		// Handle token delimiters (ends):
		case ' ':
		case '\t':
		case '\n':
			buff[i] = '\0';
			in_token = false;
			break;

		// Handle other characters (may be start)
		default:
			if (!in_token) {
				tokens[token_count] = &buff[i];
				token_count++;
				in_token = true;
			}
		}
	}
	tokens[token_count] = NULL;
	return token_count;
}

/**
 * Read a command from the keyboard into the buffer 'buff' and tokenize it
 * such that 'tokens[i]' points into 'buff' to the i'th token in the command.
 * buff: Buffer allocated by the calling code. Must be at least
 *       COMMAND_LENGTH bytes long.
 * tokens[]: Array of character pointers which point into 'buff'. Must be at
 *       least NUM_TOKENS long. Will strip out up to one final '&' token.
 *       tokens will be NULL terminated (a NULL pointer indicates end of tokens).
 * in_background: pointer to a boolean variable. Set to true if user entered
 *       an & as their last token; otherwise set to false.
 */
int read_command(char *buff, char *tokens[], _Bool *in_background)
{
	*in_background = false;

	// Read input
	int length = read(STDIN_FILENO, buff, COMMAND_LENGTH-1);

	if ((length < 0) && (errno != EINTR)){
    	perror("Unable to read command. Terminating.\n");
    	exit(-1);  /* terminate with error */
	}
	else if (length < 0) {
		return -1;
	}

	// Null terminate and strip \n.
	buff[length] = '\0';
	if (buff[strlen(buff) - 1] == '\n') {
		buff[strlen(buff) - 1] = '\0';
	}

	// Tokenize (saving original command string)
	int token_count = tokenize_command(buff, tokens);
	if (token_count == 0) {
		return 0;
	}

	// Extract if running in background:
	if (token_count > 0 && strcmp(tokens[token_count - 1], "&") == 0) {
		*in_background = true;
		tokens[token_count - 1] = 0;
	}
	return 0;
}

/**
 * Main and Execute Commands
 */
int main(int argc, char* argv[]) {

	char input_buffer[COMMAND_LENGTH];
	char *tokens[NUM_TOKENS];
	char pwd_buffer[PWD_BUF_LENGTH];
    ha = createHistoryArray();

	while (true) {

		//start signal handler for SIGINT (ctrl+c) signals
    	signal(SIGINT, signalHandler);

    	//retrieve current working directory and print to stdout
        getCurrentDirectory(pwd_buffer);

		// Get command
		// Use write because we need to use read() to work with
		// signals, and read() is incompatible with printf().
		write(STDOUT_FILENO, "$ ", strlen("$ "));
		_Bool in_background = false;

		//-1 is returned iff SIGINT interrupts read()
		if (read_command(input_buffer, tokens, &in_background) == -1) {
			continue;
		}

		//clean up zombie processes
		while (waitpid(-1, NULL, WNOHANG) > 0)
			;

        /*
         * if a history command is requested, figure out what user wants
         * returns a value based on whether the program should continue
         * to fork() instructions or start over
         */
        int historyStatus = 1;
        if(input_buffer[0] == '!') {
            historyStatus = detectHistoryCommands(ha, input_buffer, tokens);
            if(historyStatus == 0) {
                continue;
            }
        }

        //if the user enters nothing, ignore that line and start over
        if (strcmp(input_buffer, "") == 0 || strcmp(input_buffer, "\0") == 0) {
            continue;
        }

        /*
         * at this point, user command is considered valid
         * print command to history and begin processing
         */
        addToHistoryArray(ha, tokens, in_background);

        /*
         * looks for 'exit', 'pwd', 'cd', and 'list' commands;
         * if a command is found, it executes associated instructions
         * and returns 0 to restart for user input
         */
        if (detectHardcodedCommands(pwd_buffer, input_buffer, tokens) == 0) {
        	continue;
        }
        

        //if user asks for their input history, print it to stdout
        if (strcmp(input_buffer, "history") == 0) {
            displayHistory(*ha);
            continue;
        }

        /*
         * in the case where a command that requests the process be run
         * in background is recalled using !* commands, manually check
         * whether the new process should run in the background
         */
        int i = 1;
        while (tokens[i] != NULL) {
            if (strcmp(tokens[i], "&") == 0) {
                in_background = true;
                tokens[i] = '\0';
            }
            i++;
        }

		//fork a child process
		pid_t newPid;
		newPid = fork();
		int status;

		//if fork failed, write error message
		if (newPid < 0) {
			write(STDERR_FILENO, "fork() failed.", strlen("fork() failed."));
		}

		/*
		 * if current process is a child, execute user's command
		 * if command doesn't exist, print that and terminate child
		 */
		else if (newPid == 0) {
			execvp(input_buffer, tokens);
            write(STDOUT_FILENO, tokens[0], strlen(tokens[0]));
			write(STDOUT_FILENO, ": Unknown command.\n", strlen(": Unknown command.\n"));
			exit(0);
		}

		/*
		 *if current process is the parent, wait for child to finish
		 *if child will continue in the background, skip and go to next loop
		 */
		else if (newPid >= 0 && !in_background){
			waitpid(newPid, &status, 0);
		}

	}
	return 0;
}

int detectHardcodedCommands(char* pwd_buffer, char* input_buffer, char** tokens) {

		//if user wants to exit, quit program with no error
		if (strcmp(input_buffer, "exit") == 0) {
			removeHistoryArray(ha);
			exit(0);
		}
		//print working directory for the user to stdout
		else if (strcmp(input_buffer, "pwd") == 0) {
			write(STDOUT_FILENO, pwd_buffer, strlen(pwd_buffer));
			write(STDOUT_FILENO, "\n", strlen("\n"));
			return 0;
		}

		//change directories to where the user wants
		else if (strcmp(input_buffer, "cd") == 0) {

			//if given directory doesn't exist, error; else, change directories
			if (chdir(tokens[1]) == -1) {
                write(STDOUT_FILENO, "Invalid directory.\n", strlen("Invalid directory.\n"));
            }
			return 0;
		}

		//display the type of command the user requests
		else if (strcmp(input_buffer, "type") == 0) {

			//error if no argument given
			if (tokens[1] == NULL) {
				write(STDOUT_FILENO, "No argument specified.\n", strlen("No argument specified.\n"));
			}

			//case 1: argument given is a shell internal command
			else if (strcmp(tokens[1], 	  "exit") == 0 ||
						strcmp(tokens[1], "pwd" ) == 0 ||
						strcmp(tokens[1], "cd"  ) == 0 ||
						strcmp(tokens[1], "type") == 0) {
				write(STDOUT_FILENO, tokens[1], strlen(tokens[1]));
				write(STDOUT_FILENO, " is a shell300 builtin\n", strlen(" is a shell300 builtin\n"));
			}

			//case 2: argument given is external to shell300
			else {
				write(STDOUT_FILENO, tokens[1], strlen(tokens[1]));
				write(STDOUT_FILENO, " is external to shell300\n", strlen(" is external to shell300\n"));
			}
			return 0;
		}

	return 1;
}

//prints current working directory to stdout
void getCurrentDirectory(char* pwd_buffer) {
    getcwd(pwd_buffer, PWD_BUF_LENGTH);
    write(STDOUT_FILENO, pwd_buffer, strlen(pwd_buffer));
    return;
}

/*
 * detects if user wants to repeat a previous command
 * returns 1 if program should continue to fork(),
 *         0 if program should return to beginning
 */
int detectHistoryCommands(struct historyArray* ha, 
                                char*  input_buffer, 
                                char** tokens) {

    //case 1: if user wants to repeat last command
    if(input_buffer[1] == '!') {

    	//if no commands have been entered, error
        if(ha->size == 0) {
            write(STDOUT_FILENO, "SHELL: Unknown history command.\n", strlen("SHELL: Unknown history command.\n"));
            return 0;
        }

        //if a command can be repeated, set it to internal buffers for later exec()
        else{
            strcpy(input_buffer, ha->items[ha->lastItem % MAX_HISTORY]);
            write(STDOUT_FILENO, ha->items[ha->lastItem % MAX_HISTORY], strlen(ha->items[ha->lastItem % MAX_HISTORY]));
            write(STDOUT_FILENO, "\n", strlen("\n"));
        }
    }

    //case 2: if user gives an integer command to repeat
    else if(input_buffer[1] >= 48 && input_buffer[1] < 58) {

    	//check each character of the command; if a non-letter character is present error
    	int i = 2;
    	while (input_buffer[i] != '\0') {
    		if ((input_buffer[i] < 48 || input_buffer[i] >= 58) && input_buffer[i] != '\0') {
    			write(STDOUT_FILENO, "SHELL: Unknown history command.\n", strlen("SHELL: Unknown history command.\n"));
    			return 0;
    		}
    		i++;
    	}

        char* intArr = &input_buffer[1];
        int numVal = atoi(intArr);

        //if command is not one of the last ten, error
        if(numVal > ha->lastItem || numVal < ha->firstItem) {
            write(STDOUT_FILENO, "SHELL: Unknown history command.\n", strlen("SHELL: Unknown history command.\n"));
            return 0;
        }
        strcpy(input_buffer, ha->items[numVal % MAX_HISTORY]);
        write(STDOUT_FILENO, ha->items[numVal % MAX_HISTORY], strlen(ha->items[numVal % MAX_HISTORY]));
        write(STDOUT_FILENO, "\n", strlen("\n"));
    }

    //case 3: user gives undefined command to repeat
    else {
        write(STDOUT_FILENO, "SHELL: Unknown history command.\n", strlen("SHELL: Unknown history command.\n"));
        return 0;
    }

    //re-initialize specified user command to pass into later exec()
    tokenize_command(input_buffer, tokens);
    return 1;
}

//prints the last ten entries in the history array to stdout
void displayHistory(struct historyArray ha) {
	int int32max = 10;
    char* c = malloc(int32max * sizeof(char));

    for (int i = ha.firstItem; i <= ha.lastItem; i++) {
    	int numberOfDigits = log10(i)+2;
    	snprintf(c, numberOfDigits, "%d", i);

        write(STDOUT_FILENO, c, strlen(c));
        write(STDOUT_FILENO, "\t", strlen("\t"));
        write(STDOUT_FILENO, ha.items[i % MAX_HISTORY], strlen(ha.items[i % MAX_HISTORY]));
        write(STDOUT_FILENO, "\n", strlen("\n"));
    }

    free(c);
    return;
}

//if a SIGINT signal is detected, re-print history
void signalHandler(int signalNumber) {
    write(STDOUT_FILENO, "\n", strlen("\n"));
    displayHistory(*ha);
    return;
}