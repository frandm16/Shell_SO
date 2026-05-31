/**
UNIX Shell Project

Sistemas Operativos
Grados I. Informatica, Computadores & Software
Dept. Arquitectura de Computadores - UMA

Some code adapted from "Fundamentos de Sistemas Operativos", Silberschatz et al.

To compile and run the program:
   $ gcc Shell_project.c job_control.c -o Shell
   $ ./Shell          
	(then type ^D to exit program)

**/

#include "job_control.h"   // remember to compile with module job_control.c 
#include <string.h>

#define MAX_LINE 256 /* 256 chars per line, per command, should be enough. */

job *job_list;

// -----------------------------------------------------------------------
//                            MAIN          
// -----------------------------------------------------------------------

int main(void)
{
	char inputBuffer[MAX_LINE]; /* buffer to hold the command entered */
	int background;             /* equals 1 if a command is followed by '&' */
	char *args[MAX_LINE/2];     /* command line (of 256) has max of 128 arguments */
	// probably useful variables:
	int pid_fork, pid_wait; 	/* pid for created and waited process */
	int status;             	/* status returned by wait */
	char *file_in, *file_out; 	/* file names for redirection */

	const char *home_dir = getenv("HOME");
	job_list = new_list("jobs list");

	ignore_terminal_signals();

	while (1)   /* Program terminates normally inside get_command() after ^D is typed*/
	{   		
		printf("COMMAND->");
		fflush(stdout);
		get_command(inputBuffer, MAX_LINE, args, &background);  /* get next command */
		
		if(args[0]==NULL) continue;   // if empty command

		if (strcmp(args[0], "cd") == 0)
		{
			if (args[1] == NULL || strcmp(args[1], "~") == 0)
			{
				chdir(home_dir);
				continue;
			}

			if (chdir(args[1]) != 0)
			{
				perror("cd");
			}
		}

		/* the steps are:
			 (1) fork a child process using fork()
			 (2) the child process will invoke execvp()
			 (3) if background == 0, the parent will wait, otherwise continue 
			 (4) Shell shows a status message for processed command 
			 (5) loop returns to get_commnad() function
		*/
		pid_fork = fork();

		if (pid_fork < 0)
		{
			perror("fork");
			continue;
		}

		if (pid_fork == 0)
		{
			setpgid(0, 0);
			if (!background)
			{
				tcsetpgrp(STDIN_FILENO, getpid());
			}
			restore_terminal_signals();
			execvp(args[0], args);
			fprintf(stderr, "Error, command not found: %s\n", args[0]);
			exit(255);
		}

		setpgid(pid_fork, pid_fork);

		if (background)
		{
			printf("Background job running... pid: %d, command: %s\n", pid_fork, args[0]);
			continue;
		}

		tcsetpgrp(STDIN_FILENO, pid_fork);
		pid_wait = waitpid(pid_fork, &status, WUNTRACED);
		tcsetpgrp(STDIN_FILENO, getpgrp());
		if (pid_wait < 0)
		{
			perror("waitpid");
			continue;
		}

		if (WIFEXITED(status))
		{
			printf("Foreground pid: %d, command: %s, Exited, info: %d\n",
				pid_wait, args[0], WEXITSTATUS(status));
		}
		else if (WIFSIGNALED(status))
		{
			printf("Foreground pid: %d, command: %s, Signaled, info: %d\n",
				pid_wait, args[0], WTERMSIG(status));
		}
		else if (WIFSTOPPED(status))
		{
			printf("Foreground pid: %d, command: %s, Suspended, info: %d\n",
				pid_wait, args[0], WSTOPSIG(status));
			//add_job(job_list, new_job(get_pid, get_command, STOPPED))
		}

	} // end while
}
