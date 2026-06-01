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
#include <dirent.h>

#define MAX_LINE 256 /* 256 chars per line, per command, should be enough. */

job *job_list;
pid_t shell_pid;

// Recorre /proc para buscar zombis hijos del shell
void traverse_proc(void) {
    DIR *d; 
    struct dirent *dir;
    char buff[2048];
    d = opendir("/proc");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            sprintf(buff, "/proc/%s/stat", dir->d_name); 
            FILE *fd = fopen(buff, "r");
            if (fd){
                long pid;     // pid
                long ppid;    // ppid
                char state;   // estado: R (runnable), S (sleeping), T(stopped), Z (zombie)

                // La siguiente línea lee pid, state y ppid de /proc/<pid>/stat
                fscanf(fd, "%ld %s %c %ld", &pid, buff, &state, &ppid);

				if (ppid == shell_pid && state == 'Z') { // Si es estado es zombi y el pid padre es la shell se imprime
					printf("%ld\n", pid); // Solo mostramos el PID
				}
                fclose(fd);
            }
        }
        closedir(d);
    }
}

// Obtiene la posicion del job o devuelve 1 por defecto
int get_job_position(char *arg)
{
	if (arg == NULL)
	{
		return 1;
	}

	return atoi(arg);
}

// Maneja SIGHUP y SIGCHLD
void manejador(int s) 
{
  if (s == 1)
  {
    FILE *fp = fopen("hup.txt","a"); // abre un fichero en modo 'append'
	if (fp) { 
		fprintf(fp, "SIGHUP recibido.\n"); //escribe en el fichero
		fclose(fp);
	}
    return;
  }

  job *job;
  int status;
  int pid_wait;

  for (int i = list_size(job_list); i >= 1; i--) 
  {
    job = get_item_bypos(job_list, i);
    if (job == NULL)
    {
      continue;
    }
    pid_wait = waitpid(job->pgid, &status, WNOHANG | WUNTRACED | WCONTINUED);

    if (pid_wait == job->pgid) 
    {
      // Si cambia de estado, informar y actualizar la lista
      if (WIFEXITED(status)) 
      {
		printf("Background pid: %d, command: %s, Exited, info: %d\n", job->pgid, job->command, WTERMSIG(status));
        delete_job(job_list, job);
      } else if (WIFSIGNALED(status)) 
      {
		printf("Background pid: %d, command: %s, Signaled, info: %d\n", job->pgid, job->command, WTERMSIG(status));
        delete_job(job_list, job);
      } else if (WIFSTOPPED(status)) 
      {
        job->state = STOPPED;
		printf("Background pid: %d, command: %s, Suspended, info: %d\n", job->pgid, job->command, WTERMSIG(status));
      } else if (WIFCONTINUED(status))
      {
        job->state = BACKGROUND;
		printf("Background pid: %d, command: %s, Continued, info: %d\n", job->pgid, job->command, WTERMSIG(status));
      }
    }
  }
}

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

	job_list = new_list("jobs list"); // Crear la lista de trabajos
	shell_pid = getpid(); // Guardar el pid del shell

	ignore_terminal_signals(); // Ignorar senales de terminal en el shell
	signal(SIGCHLD, manejador); // Registrar manejador para trabajos hijos
	signal(SIGHUP, manejador); // Registrar manejador para SIGHUP

	while (1)   /* Program terminates normally inside get_command() after ^D is typed*/
	{   		
		printf("COMMAND->");
		fflush(stdout);
		get_command(inputBuffer, MAX_LINE, args, &background);  /* get next command */
		parse_redirections(args, &file_in, &file_out); // Detectar redirecciones simples
		
		if(args[0]==NULL) continue;   // if empty command

		// ---------------------------------------------------------------------------- COMANDOS INTERNOS ----------------------------------------------------------------------------

		if (strcmp(args[0], "cd") == 0) // comando cd
		{
			if (args[1] == NULL || strcmp(args[1], "~") == 0)
			{
				chdir(getenv("HOME"));
				continue;
			}

			if (chdir(args[1]) != 0)
			{
				perror("cd");
			}
			continue;
		}

		if (strcmp(args[0], "jobs") == 0) // comando jobs
		{
      		block_SIGCHLD(); // Bloquear SIGCHLD para evitar que se maneje mientras se accede a la lista de trabajos
			print_job_list(job_list);
      		unblock_SIGCHLD(); // Desbloquear SIGCHLD para permitir de nuevo el manejo de la lista de trabajos
			continue;
		}

		if (strcmp(args[0], "currjob") == 0) // comando currjob
		{
			block_SIGCHLD(); // Bloquear SIGCHLD para evitar que se maneje mientras se accede a la lista de trabajos
			if (empty_list(job_list)) // Si job_list esta vacia
			{
				printf("No hay trabajo actual\n");
			} else {
				job *item = get_item_bypos(job_list, 1); // Obtener el primer trabajo
				printf("Trabajo actual: PID=%d command=%s\n", item->pgid, item->command);
			}
			unblock_SIGCHLD(); // Desbloquear SIGCHLD para permitir de nuevo el manejo de la lista de trabajos
			continue;
		}

		if (strcmp(args[0], "deljob") == 0) // comando deljob
		{
			block_SIGCHLD(); // Bloquear SIGCHLD para evitar que se maneje mientras se modifica la lista de trabajos

			if (empty_list(job_list))
			{
				printf("No hay trabajo actual\n");

			} else {
				job *item = get_item_bypos(job_list, 1);
				if (item->state == STOPPED)
				{
					printf("No se permiten borrar trabajos en segundo plano suspendidos\n");
				} else {
					printf("Borrando trabajo actual de la lista de jobs: PID=%d command=%s\n", item->pgid, item->command);
					delete_job(job_list, item);
				}
			}
			
			unblock_SIGCHLD(); // Desbloquear SIGCHLD para permitir de nuevo el manejo de la lista de trabajos
			continue;
		}

		if (strcmp(args[0], "zjobs") == 0) // comando zjobs
		{
			traverse_proc();
			continue;
		}

		if (strcmp(args[0], "fg") == 0) // comando fg
		{
			int pos = get_job_position(args[1]);
			job *item;
			pid_t job_pid;
			char *job_command;

			block_SIGCHLD(); // Bloquear SIGCHLD para evitar que se maneje mientras se modifica la lista de trabajos
			item = get_item_bypos(job_list, pos);
			if (item == NULL)
			{
				unblock_SIGCHLD(); // Desbloquear SIGCHLD para permitir de nuevo el manejo de la lista de trabajos
				continue;
			}
			job_pid = item->pgid;
			job_command = strdup(item->command);
			delete_job(job_list, item);
			unblock_SIGCHLD(); // Desbloquear SIGCHLD para permitir de nuevo el manejo de la lista de trabajos

			tcsetpgrp(STDIN_FILENO, job_pid); // Dar el terminal al job
			killpg(job_pid, SIGCONT); // Reanudar el grupo de procesos
			pid_wait = waitpid(job_pid, &status, WUNTRACED); // Esperar al job en foreground
			tcsetpgrp(STDIN_FILENO, getpgrp()); // Recuperar el terminal para el shell

			if (WIFSTOPPED(status))
			{
				block_SIGCHLD(); // Bloquear SIGCHLD para evitar que se maneje mientras se modifica la lista de trabajos
				add_job(job_list, new_job(job_pid, job_command, STOPPED));
				unblock_SIGCHLD(); // Desbloquear SIGCHLD para permitir de nuevo el manejo de la lista de trabajos

				printf("Foreground pid: %d, command: %s, Suspended, info: %d\n", job_pid, job_command, WSTOPSIG(status));
			}
			else if (WIFEXITED(status))
			{
				printf("Foreground pid: %d, command: %s, Exited, info: %d\n", job_pid, job_command, WEXITSTATUS(status));
			}
			else if (WIFSIGNALED(status))
			{
				printf("Foreground pid: %d, command: %s, Signaled, info: %d\n", job_pid, job_command, WTERMSIG(status));
			}

			free(job_command);
			continue;
		}

		if (strcmp(args[0], "bg") == 0) // comando bg
		{
			int pos = get_job_position(args[1]);
			job *item;

			block_SIGCHLD(); // Bloquear SIGCHLD para evitar que se maneje mientras se modifica la lista de trabajos
			item = get_item_bypos(job_list, pos);
			if (item == NULL)
			{
				unblock_SIGCHLD(); // Desbloquear SIGCHLD para permitir de nuevo el manejo de la lista de trabajos
				continue;
			}
			item->state = BACKGROUND;
			unblock_SIGCHLD(); // Desbloquear SIGCHLD para permitir de nuevo el manejo de la lista de trabajos

			killpg(item->pgid, SIGCONT); // Reanudar el job en background
			printf("Background job running... pid: %d, command: %s\n", item->pgid, item->command);
			continue;
		}

		// ---------------------------------------------------------------------------- COMANDOS EXTERNOS ----------------------------------------------------------------------------

		/* the steps are:
			 (1) fork a child process using fork()
			 (2) the child process will invoke execvp()
			 (3) if background == 0, the parent will wait, otherwise continue 
			 (4) Shell shows a status message for processed command 
			 (5) loop returns to get_commnad() function
		*/

		// (1) fork a child process using fork()
		pid_fork = fork();

		if (pid_fork < 0)
		{
			perror("fork");
			continue;
		}

		// (2) the child process will invoke execvp()
		if (pid_fork == 0) // child process
		{
			
			setpgid(0, getpid()); // Crear un nuevo grupo de procesos para el hijo

			if (file_in)
			{
				FILE* file = fopen(file_in, "r");
				if (file == NULL)
				{
					fprintf(stderr, "Error: abriendo: %s\n", file_in);
					exit(255);
				}
				if (dup2(fileno(file), STDIN_FILENO) < 0)
				{
					fprintf(stderr, "Error: redireccionando entrada\n");
					fclose(file);
					exit(255);
				}
				fclose(file);
			}

			if (file_out)
			{
				FILE* file = fopen(file_out, "w");
				if (file == NULL)
				{
					fprintf(stderr, "Error: abriendo: %s\n", file_out);
					exit(255);
				}
				if (dup2(fileno(file), STDOUT_FILENO) < 0)
				{
					fprintf(stderr, "Error: redireccionando salida\n");
					fclose(file);
					exit(255);
				}
				fclose(file);
			}

			restore_terminal_signals(); // Restaurar senales por defecto en el hijo
			execvp(args[0], args); // Ejecutar el comando externo
			fprintf(stderr, "Error, command not found: %s\n", args[0]);
			exit(255);
		}

		setpgid(pid_fork, pid_fork); // Asegurar el grupo de procesos del hijo

		// (3) if background == 0, the parent will wait, otherwise continue 
		// (4) Shell shows a status message for processed command 
		if (background == 0)
		{
			// foreground
      		tcsetpgrp(STDIN_FILENO, pid_fork); // Dar el terminal al hijo en foreground
			pid_wait = waitpid(pid_fork, &status, WUNTRACED); // Esperar a que termine o se suspenda
			tcsetpgrp(STDIN_FILENO, getpgrp()); // Recuperar el terminal para el shell

			if (WIFSTOPPED(status))
			{
        		block_SIGCHLD(); // Bloquear SIGCHLD para evitar que se maneje mientras se modifica la lista de trabajos
				add_job(job_list, new_job(pid_fork, args[0], STOPPED));
        		unblock_SIGCHLD(); // Desbloquear SIGCHLD para permitir de nuevo el manejo de la lista de trabajos

				printf("Foreground pid: %d, command: %s, Suspended, info: %d\n", pid_wait, args[0], WSTOPSIG(status));
			} else if (WIFEXITED(status)){
				printf("Foreground pid: %d, command: %s, Exited, info: %d\n", pid_wait, args[0], WEXITSTATUS(status));
			} else if (WIFSIGNALED(status)){
				printf("Foreground pid: %d, command: %s, Signaled, info: %d\n", pid_wait, args[0], WTERMSIG(status));
			}
		} else {
			//background
      		block_SIGCHLD(); // Bloquear SIGCHLD para evitar que se maneje mientras se modifica la lista de trabajos
			add_job(job_list, new_job(pid_fork, args[0], BACKGROUND));
      		unblock_SIGCHLD(); // Desbloquear SIGCHLD para permitir de nuevo el manejo de la lista de trabajos

			printf("Background job running... pid: %d, command: %s\n", pid_fork, args[0]); // Informar del nuevo job
		}

  } // end while
}
