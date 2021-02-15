/* Mason Collett - CS344 Assignment 3 - smallsh
*  2/6/2021
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>

/*
* Function to replace all instances of $$ with the pid
* Implemented with help from: 
* https://stackoverflow.com/questions/779875/what-function-is-to-replace-a-substring-from-a-string-in-c
* Only works for replacing $$ at the end of a string :/
*/
char *str_replace(char *orig, char *rep, char *with) {
	int i;
	int size = strlen(orig);
	for(i = 0; orig[i]; i++){
		if(orig[i]=='$' && orig[i+1] =='$'){
			orig[i] = '\0';  // cut off $$
			strcat(orig,with); // replace with pid
		}
	}
	return orig;
}

// global variable okay'ed on piazza
int fg = 0;

void catch_signal() {					
	if(fg == 1) {
		printf("Exiting foreground-only mode\n");
		fflush(stdout);
		fg = 0;
	}
	else {
		printf("Entering foreground-only mode (& is now ignored)\n");
		fflush(stdout);	
		fg = 1;
	}
}


void main(int argc, char* argv[])
{
    char user_input[512];
    char *commandList[512];
	char* otherCommandList[512];
    char strbuffer[512];
	int exit_value = 0;
	int is_bg = 0;
	pid_t spawnPid = -5;
	int childStatus;

	
	// create signal handlers
	struct sigaction sigint = {0};
	sigint.sa_handler = SIG_IGN;
	sigfillset(&sigint.sa_mask);
	sigint.sa_flags = 0;
	sigaction(SIGINT,&sigint,NULL);
	
	struct sigaction sigstp = {0};
	sigstp.sa_handler = catch_signal;
	sigfillset(&sigstp.sa_mask);
	sigstp.sa_flags = 0;
	sigaction(SIGTSTP,&sigstp,NULL);

    while(1){
        // Get command
        printf(": ");
		fflush(stdout);	
        strcpy(user_input, "\n");
		fgets(user_input, 512, stdin);

        // Tokenize command, setup input and output files
        char *token = strtok(user_input, " ");
		char infile[256] = "";
        char outfile[256] = "";
        int count = 0; // keeps track of how many commands and args there are
		int is_empty = 0;
		int i;

        // iterate through command token, assess input/output file, $$
		while(token != NULL ) {
            // read in input file name						
			if (strcmp(token, "<") == 0) {
                // advance past <
				token = strtok(NULL, " ");						
				sscanf(token, "%s", infile);
				// check for $$, replace if needed
				char token_pid[100];				
				sprintf(token_pid, "%d", getpid());
				strcat(token_pid, "\0"); 
				char* replace = "$$";
				strcpy(infile, str_replace(infile, replace, token_pid));
				token = strtok(NULL, " ");
			}
            // read in output file
			else if (strcmp(token, ">") == 0) {	
                // advance past >		
				token = strtok(NULL, " ");
				sscanf(token, "%s", outfile);
				// check for $$, replace
				char token_pid[100];				
				sprintf(token_pid, "%d", getpid());
				strcat(token_pid, "\0"); 
				char* replace = "$$";
				strcpy(outfile, str_replace(outfile, replace, token_pid));
				token = strtok(NULL, " ");
			}
            // everything else goes into command list
			else {
					sscanf(token, "%s", strbuffer);
					// check for $$, replace if needed
					char token_pid[100];				
					sprintf(token_pid, "%d", getpid());
					strcat(token_pid, "\0"); 
					char* replace = "$$";
					strcpy(strbuffer, str_replace(strbuffer, replace, token_pid));
					//strcat(strbuffer, "\0"); 
					commandList[count] = strdup(strbuffer);
					count = count + 1;
					token = strtok(NULL, " ");
			}
		}

		// check for background process, remove it from the array
		// tried multiple ways of "deleting" the last element from the 
		// array- tried copying it to another array.  ended up being able
		// to just say 'last value = null', which i had tried and didn't work.
		// so anyways, most of this can probably be deleted, but i'm too scared to
		// break anything at this point.
		if(strcmp(commandList[count-1],"&") == 0){
			int i = 0;
			for(i = 0; i < count-1 ; i++){
				otherCommandList[i] = commandList[i];
			}
			otherCommandList[count-1] = '\0'; // place the null terminator
			commandList[count-1] = NULL;  // this is the only line that matters
			is_bg = 1; // and this one
		}
		else {
			is_bg = 0;
		}
		// if nothing wa entered, set it to null
		// any other way of handling this didn't work
		if (user_input[0] == '\n'){								
			commandList[0] = NULL;
		}

		// print command list for debugging
		// for(i = 0; i<count; i++){
		// 	printf("commandlist: %s\n",commandList[i]);
		// }

		// now actually do stuff with the commands
		// check to make sure not a comment 
		if(commandList[0] != NULL){
			// make sure not a comment
			if(strcmp(commandList[0],"#") != 0 && strchr(commandList[0], '#') == NULL && strlen(commandList[0]) != 0){
				// exit command
				if(strcmp(commandList[0],"exit")==0){
					exit(0);
				}
				// cd command
				else if(strcmp(commandList[0],"cd")==0){
					// check for additional arg
					if(count == 2){
						chdir(commandList[1]);
					}
					// if no arg, go to HOME
					else if(count == 1){
						chdir(getenv("HOME"));
					}
					else{
						printf("Error: too many arguments given.  cd expects one or zero args.\n");
						fflush(stdout);
					}
				}
				// status command
				else if(strcmp(commandList[0],"status")==0){
					if(WIFEXITED(childStatus)){
						printf("exit value %i\n", WEXITSTATUS(childStatus));
						fflush(stdout);
					}
					else{
						printf("terminated by signal %i\n", childStatus);
						fflush(stdout);
					}
				}

				// all other commands:
				else{
					// implemented with help from lecture
					// https://oregonstate.instructure.com/courses/1798831/pages/exploration-process-api-executing-a-new-program?module_item_id=20163875
					int result;

					// fork new process
					spawnPid = fork();

					switch(spawnPid){
						// error
						case -1: {
							perror("Unable to fork :(\n");
							exit(1);
							break;
						}
						// works properly
						// In the child process
						case 0: {
							// signal swap
							if(is_bg){
								sigint.sa_handler = SIG_DFL;
								sigaction(SIGINT,&sigint,NULL);
							}

							// check for input file
							// https://repl.it/@cs344/54sortViaFilesc
							if(strlen(infile) != 0){
								int sourceFD = open(infile, O_RDONLY);
								if (sourceFD == -1) { 
									perror("Can't open input file.  "); 
									exit(1); 
								}
								// redirect stdin to source file
								result = dup2(sourceFD, 0);
								if (result == -1) { 
									perror("Can't redirect stdin  "); 
									exit(2); 
								}
								fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
							}
							// check for output file
							// https://repl.it/@cs344/54sortViaFilesc
							if(strlen(outfile)!= 0){
								int targetFD = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
								if (targetFD == -1) { 
									perror("Can't open output file  "); 
									exit(1); 
								}									
								// Redirect stdout to target file
								result = dup2(targetFD, 1);
								if (result == -1) { 
									perror("Can't redirect stdout  "); 
									exit(2); 
								}
								fcntl(targetFD, F_SETFD, FD_CLOEXEC);
							}
							// Run command
							if (execvp(commandList[0], (char* const*)commandList)<0) {
								printf("%s: no such file or directory\n", commandList[0]);
								fflush(stdout);
								exit(2);
							}
							break;
						}
						default: {
							// Not a background process (no &)
							if(fg == 1 || is_bg == 0){
								waitpid(spawnPid, &childStatus, 0);
								//exit_value = WEXITSTATUS(childStatus);
								break;
							}
							// is a background process
							else{
								//waitpid(spawnPid, &childStatus, WNOHANG);
								printf("background pid is %d\n",spawnPid);
								fflush(stdout);
							}
						}	
					} 
				}
				// check for completed processes
				while((spawnPid = waitpid(-1, &childStatus, WNOHANG)) > 0) {
					printf("child %d terminated\n", spawnPid);
					fflush(stdout);
					if(WIFEXITED(childStatus)) {
						printf("exit value %d\n", WEXITSTATUS(childStatus));
						fflush(stdout);
					}
					else {
						printf("terminated by signal %d\n", childStatus);
						fflush(stdout);
					}
				}

				// for (i=0; commandList[i]; i++) {
				// 	commandList[i] = NULL;
				// }
				// // printf("command list at end\n");
				// // fflush(stdout);
				// // for (i=0; i<count; i++) {
				// // 	printf("command: %s\n",commandList[i]);
				// // 	fflush(stdout);
				// // }
			}
			for (i=0; commandList[i]; i++) {
				commandList[i] = NULL;
			}
			is_bg = 0;
			infile[0] = '\0';
			outfile[0] = '\0';
		}
    }
}