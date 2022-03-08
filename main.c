/*
	Written by Spencer Wise. Assignment 3 for CS344 - Creates a small shell
	that handles a few built-in commands (namely exit, status, and cd), otherwise
	forks off children processess and calls execvp() to handle other commands.
*/

# include <dirent.h>
# include <fcntl.h>
# include <signal.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <sys/stat.h>
# include <sys/types.h>
# include <sys/wait.h>
# include <time.h>
# include <unistd.h>

int chg_fg_mode = 0; // Used to tell main() when to change foreground-only mode on and off

/// <summary>
/// A small handler function used when SIGTSTP is called to change the global variable chg_fg_mode
/// </summary>
/// <param name="signo"></param>
void handle_SIGTSTP(int signo) {
	chg_fg_mode = 1;
}


int main(void) {

	struct sigaction SIGTSTP_action = { {0} };		// Initialize SIGTSTP handler

	// Set SIGTSTP handler variables (with handle_SIGTSTP as the handler function)
	SIGTSTP_action.sa_handler = handle_SIGTSTP;
	sigfillset(&SIGTSTP_action.sa_mask);
	SIGTSTP_action.sa_flags = 0;

	sigaction(SIGTSTP, &SIGTSTP_action, NULL);  // Set SIGTSTP handler to SIGTSTP

	signal(SIGINT, SIG_IGN);  // Set parent to ignore SIGINT

	char input[2049];		// Saves the user input

	int foreground_only = 0;	// Flag used to keep track of foreground only mode

	// Messages displayed when foreground-only modes are turned on and off
	char* message1 = "\nForeground-only mode now on (& is now ignored)";
	char* message2 = "\nForeground-only mode now off";

	int output_re = 0;		// Flag used to signal when to redirect output
	int input_re = 0;		// Flag used to signal when to redirect input
	int run_background = 0;		// Flag used to have commands run in the background
	int i;		// Used in loops

	char* token;		// Used by strtok_r to parse the user input
	char* savePtr;		// Used by strtok_r to parse the user input

	char exit_status[200];		// Used to keep track of the status of the last command run
	strcpy(exit_status, "exit value 0");		// Initialize it to "exit value 0"

	int child_status;		// Used by waitpid to store child status
	int child_pid_count = 0;		// Keeps track of the number of child background process
	int child_array[200];		// Stores the pids of the background child processess

	pid_t pid = getpid();		// The parent's pid
	pid_t child_pid;			// Used to store a child process pid

	// Begin command line loop
	while (1) {

		// Clear certain flags at the start of each input loop
		output_re = 0;
		input_re = 0;
		run_background = 0;

		// Print ":" and get user input
		printf(": ");
		fgets(input, 2048, stdin);

		// If chg_fg_mode flag is set, reset it and toggle foreground_only mode flag with appropriate message printed
		if (chg_fg_mode == 1) {
			chg_fg_mode = 0;
			if (foreground_only == 0) {
				printf("%s\n", message1);
				fflush(stdout);
				foreground_only = 1;
			}
			else if (foreground_only == 1) {
				printf("%s\n", message2);
				fflush(stdout);
				foreground_only = 0;
			}
		}
		else if (input[0] == '\n') { // If the input is a blank line, do nothing
			 ;
		}
		else if (input[0] == '#') { // If the input is a comment, do nothing
			 ;
		}
		else {

			input[strlen(input) - 1] = '\0';		// Strip off newline char

			// If the last two chars of input are " &", set run_background flag and strip the last two chars off input
			if (input[strlen(input) - 1] == '&' && input[strlen(input) - 2] == ' ') {
				
				// Also set run_background flag only if foreground only mode is off
				if (foreground_only == 0) {
					run_background = 1;
				}

				input[strlen(input) - 1] = '\0';
				input[strlen(input) - 1] = '\0';
			}

			// Loop over each char of input string and replace $$ with pid
			for (i = 0; input[i] != '\0'; i++) {

				// If char is '$' and next char is '$'
				if (input[i] == '$') {
					if (input[i + 1] == '$') {

						// Calculate new temp string length and create temp variables
						int new_length = strlen(input) + 5;
						char new_string[new_length];
						char pid_string[7];

						// Turn pid into a string
						snprintf(pid_string, 7, "%d", pid);

						// Copy first i chars from input string to temp string and null terminate it
						strncpy(new_string, input, i);
						new_string[i] = '\0';

						// Add the pid to the temp string, then the rest of the input string
						strcat(new_string, pid_string);
						strcat(new_string, input + i + 2);

						// Replace the input with it and null terminate it
						strcpy(input, new_string);
						input[strlen(new_string)] = '\0';

					}
				}
			}

			token = strtok_r(input, " ", &savePtr); // Get first word of input command
			
			if (strncmp(token, "exit", 4) == 0) { // If first word of the command is 'exit'

				// Iterate over the array of background processes and kill them off before exiting
				for (i = 0; i < child_pid_count; i++) {

					kill(child_array[i], SIGKILL);
				}

				fflush(stdout);
				return 0;

			}
			else if (strncmp(token, "cd", 2) == 0) { // If first word of command is 'cd'

				token = strtok_r(NULL, "", &savePtr); // Get second word in input

				// If there is no second word, change directory to HOME
				if (token == NULL) {

					// Find HOME directory
					char* home_dir = getenv("HOME");

					// Change dir to HOME directory
					chdir(home_dir);
				}
				else { // Else change directory to given file path
	
					chdir(token);
				}

			}
			else if (strncmp(token, "status", 6) == 0) { // If first word is 'status'

				printf("%s\n", exit_status); // Print exit status
				fflush(stdout);
			}
			else { // If first word isn't one of the built-in commands

				child_pid = fork(); // Fork off a child process

				if (child_pid == -1) {
					perror("Fork() failed!");
					exit(1);
				}
				else if (child_pid == 0) {  // If in the child process

					// Set child processes to ignore SIGSTP
					signal(SIGTSTP, SIG_IGN);

					// The below code parsing an input and storing as an array is adapted from here: https://stackoverflow.com/questions/15539708/passing-an-array-to-execvp-from-the-users-input
					// Create variables to store parsed input
					char* args[512];
					char** next = args;

					while (token != NULL) // Loop over each word in input
					{
						if (input_re == 1) { // If input redirect flag is set

							// Open the token as a file path
							int sourceFD = open(token, O_RDONLY);
							if (sourceFD == -1) { // If open failed, exit
								exit(3);
							}
							else {

								int result = dup2(sourceFD, 0); // Redirect input using dup2
								if (result == -1) {
									exit(3);
								}

								input_re = 2; // Used later to determine if input needs to be redirected for bg commands
							}
						}
						else if (output_re == 1) { // If output redirect flag is set

							// Open the token as a file path
							int targetFD = open(token, O_WRONLY | O_CREAT | O_TRUNC, 0640);
							if (targetFD == -1) { // If open failed, exit
								exit(3);
							}
							else {

								int result2 = dup2(targetFD, 1); // Redirect output using dup2()
								if (result2 == -1) {
									exit(3);
								}

								output_re = 2; // Used later to determine if output needs to be redirected for bg commands
							}

						}
						else if (strcmp(token, "<") == 0 && input_re == 0) { // If token is '<' and input hasn't been redirected yet, set input_re flag
							input_re = 1;
						}
						else if (strcmp(token, ">") == 0 && output_re == 0) { // If token is '>' and output hasn't been redirect yet, set output_re flag
							output_re = 1;
						}
						else { // If none of these conditions are met, add the token to the array
							*next++ = token;
						}

						token = strtok_r(NULL, " ", &savePtr); // Advance to next word in command
					}

					*next = NULL;

					// If it is a bg process and input is not already redirected, redirect to /dev/null
					if (run_background == 1 && input_re != 2) {
						
						int sourceFD = open("/dev/null", O_RDONLY);

						dup2(sourceFD, 0);
					}

					// If it is a bg process and output is not already redirected, redirect to /dev/null
					if (run_background == 1 && output_re != 2) {

						int targetFD = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC);

						dup2(targetFD, 1);
					}

					// Set foreground processes to handle SIGINT in default manner
					if (run_background != 1) {
						signal(SIGINT, SIG_DFL);
					}

					execvp(args[0], args);		// Execute command
					exit(2);		// Exit with status 2 if the command was not found

				}
				else {	// In the parent process


					if (run_background == 1) { // If run in background flag is set

						// Add child pid to the array and increment the count
						child_array[child_pid_count] = child_pid;
						child_pid_count ++;
						if (child_pid_count == 200) {
							child_pid_count = 0;
						}

						printf("Running child %d in background\n", child_pid);
						fflush(stdout);

						child_pid = waitpid(child_pid, &child_status, WNOHANG);	// Run wait command in background
					}
					else { // Run the wait command in foreground

						child_pid = waitpid(child_pid, &child_status, 0);

						// If chg_fg_mode flag is set, reset it and toggle foreground_only mode flag with appropriate mesage printed
						if (chg_fg_mode == 1) {
							chg_fg_mode = 0;
							if (foreground_only == 0) {
								printf("%s\n", message1);
								fflush(stdout);
								foreground_only = 1;
							}
							else if (foreground_only == 1) {
								printf("%s\n", message2);
								fflush(stdout);
								foreground_only = 0;
							}
						}

						if (WIFEXITED(child_status)) { // Check exit status of the child

							if (WEXITSTATUS(child_status) == 1) { // If child exited with status 1, set exit value to 1
								strcpy(exit_status, "exit value 1");
							}
							else if (WEXITSTATUS(child_status) == 2) { // If child exited with status 2, command was not recognized
								printf("Command not recognized.\n");
								fflush(stdout);
								strcpy(exit_status, "exit value 1");
							}
							else if (WEXITSTATUS(child_status) == 3) { // If child exited with status 3, file was not found
								printf("File not found.\n");
								fflush(stdout);
								strcpy(exit_status, "exit value 1");
							}
							else { // Command executed successfully, set exit_status to 0
								strcpy(exit_status, "exit value 0");
							}
						}
						else {  // Command executed abnormally, print error message and set exit_status to special message

							printf("\nChild process %d terminated by signal %d\n", child_pid, WTERMSIG(child_status));
							fflush(stdout);

							// Create and set exit_status to special message
							char temp_string[] = "Terminated by signal ";
							char temp_num_string[3];
							snprintf(temp_num_string, 3, "%d", WTERMSIG(child_status));
							strcat(temp_string, temp_num_string);
							strcpy(exit_status, temp_string);
						}
					}
				}

			}

		}

		// Check if any background commands have finished running
		for (i = 0; i < child_pid_count; i++) {

			child_pid = waitpid(child_array[i], &child_status, WNOHANG);


			if (WIFEXITED(child_status) && child_pid != -1 && child_pid != 0) { // If a new child terminated normally, print out message and set exit_status
				printf("Background pid %d is done: exit value %d\n", child_pid, WEXITSTATUS(child_status));
				fflush(stdout);
				strcpy(exit_status, "exit value 0");
			}
			else if (WIFSIGNALED(child_status) && child_pid != -1 && child_pid != 0) { // If a new child terminated abnormally, print out message and set exit_status

				printf("Background pid %d is done: terminated by signal %d\n", child_pid, WTERMSIG(child_status));
				fflush(stdout);

				// Create and set exit status to special message
				char temp_string2[] = "Terminated by signal ";
				char temp_num_string2[3];
				snprintf(temp_num_string2, 3, "%d", WTERMSIG(child_status));
				strcat(temp_string2, temp_num_string2);
				strcpy(exit_status, temp_string2);
			}
		}

	}

	return 0;
}