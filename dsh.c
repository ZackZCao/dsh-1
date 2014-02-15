#include "dsh.h"

#define   LOG_FILE      "proxy.log"

void seize_tty(pid_t callingprocess_pgid); /* Grab control of the terminal for the calling process pgid.  */
void continue_job(job_t *j); /* resume a stopped job */
void spawn_job(job_t *j, bool fg); /* spawn a new job */



int logfd;

void unix_error(char *msg) /* Unix-style error */
{
  char* logString;
  fprintf(stderr, "%s: %s\n", msg, strerror(errno));
  format_log_entry(logString, msg, sterror(errno));
  if(!write(logfd, logString, strlen(logString))) {
    fprintf(stderr, "Log write error: %s\n", sterror(errno));
  }
  //exit(0);
}

/* Sets the process group id for a given job and process */
int set_child_pgid(job_t *j, process_t *p)
{
    if (j->pgid < 0) /* first child: use its pid for job pgid */
        j->pgid = p->pid;
    return(setpgid(p->pid,j->pgid));
}

/* Creates the context for a new child by setting the pid, pgid and tcsetpgrp */
void new_child(job_t *j, process_t *p, bool fg)
{
         /* establish a new process group, and put the child in
          * foreground if requested
          */

         /* Put the process into the process group and give the process
          * group the terminal, if appropriate.  This has to be done both by
          * the dsh and in the individual child processes because of
          * potential race conditions.  
          * */

         p->pid = getpid();

         /* also establish child process group in child to avoid race (if parent has not done it yet). */
         set_child_pgid(j, p);

         if(fg) // if fg is set
		seize_tty(j->pgid); // assign the terminal

         /* Set the handling for job control signals back to the default. */
         signal(SIGTTOU, SIG_DFL);
}

/* Spawning a process with job control. fg is true if the 
 * newly-created process is to be placed in the foreground. 
 * (This implicitly puts the calling process in the background, 
 * so watch out for tty I/O after doing this.) pgid is -1 to 
 * create a new job, in which case the returned pid is also the 
 * pgid of the new job.  Else pgid specifies an existing job's 
 * pgid: this feature is used to start the second or 
 * subsequent processes in a pipeline.
 * */

void spawn_job(job_t *j, bool fg) 
{

	pid_t pid;
	process_t *p;
  int status = 0;

	for(p = j->first_process; p; p = p->next) {

	  /* YOUR CODE HERE? */
	  /* Builtin commands are already taken care earlier */
	  
	  switch (pid = fork()) {

          case -1: /* fork failure */
            perror("fork");
            exit(EXIT_FAILURE);

          case 0: /* child process  */
            p->pid = getpid();	    
            new_child(j, p, fg);
            execvp(p->argv[0], p->argv);
            
	    /* YOUR CODE HERE?  Child-side code for new process. */
            perror("New child should have done an exec");
            exit(EXIT_FAILURE);  /* NOT REACHED */
            break;    /* NOT REACHED */

          default: /* parent */
            /* establish child process group */
            p->pid = pid;
            set_child_pgid(j, p);
            waitpid(pid, &status, 0);
            /* YOUR CODE HERE?  Parent-side code for new process.  */
          }

            /* YOUR CODE HERE?  Parent-side code for new job.*/
	    seize_tty(getpid()); // assign the terminal back to dsh

	}
}

/* Sends SIGCONT signal to wake up the blocked job */
void continue_job(job_t *j) 
{
     if(kill(-j->pgid, SIGCONT) < 0)
          perror("kill(SIGCONT)");
}


/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 * it immediately.  
 */
bool builtin_cmd(job_t *last_job, int argc, char **argv) 
{

	    /* check whether the cmd is a built in command
        */

        if (!strcmp(argv[0], "quit")) {
            /* Your code here */
            exit(EXIT_SUCCESS);
	      }
        else if (!strcmp("jobs", argv[0])) {
          job_t *j = last_job;
          if(last_job->first_process->pid && last_job->first_process->status) {
            printf("%s %d\n", last_job->commandinfo, last_job->first_process->status);
            if(job_is_completed(j)) {
              delete_job(j, last_job); 
            }
            j = j->next;
            while(j) {
                printf("%s %d\n", j->pgid, j->first_process->status);
                if(job_is_completed(j)) {
                  delete_job(j, last_job); 
                }
                j = j->next;
              }
              return true;
          }
          printf("no jobs!\n");
        }
	else if (!strcmp("cd", argv[0])) {
      char cwd[1024];
      if(!chdir(argv[1])) {
        unix_error("Chdir error: ");
      }

            /* Your code here */
        }
        else if (!strcmp("bg", argv[0])) {
          while(1) {
            break;
          }
            /* Your code here */
        }
        else if (!strcmp("fg", argv[0])) {
          if(last_job) {
            continue_job(last_job);
            printf("%s\n", last_job->commandinfo);          
          }
          else
            fprintf(stderr, "No job to foreground!");
            /* Your code here */
        }
        return false;       /* not a builtin command */
}

/* Build prompt messaage */
char* promptmsg() 
{
    /* Modify this to include pid */
  int pid = getpid();
  char *prompt = (char*)malloc(50);
  sprintf(prompt, "dsh-%d$ ", pid);
  //printf("%d\n", pid);
 /* char buffer[1024];
  size_t contentSize = 1;
  char *content = malloc(sizeof(char) * 1024);
  strcat(content, "dsh$ ");
  contentSize += strlen("dsh$ ");
  while(fgets(buffer, 1024, stdin))
  {
    char *old = content;
    contentSize += strlen(buffer);
    content = realloc(content, contentSize);
    if(content == NULL)
    {
        perror("Failed to reallocate content");
        free(old);
        exit(2);
    }
    //printf("%s\n", buffer);
    strcat(content, buffer);
    if(strstr(buffer, ";\r\n"))
      break;
}*/
	return prompt;
}

int main() 
{

	init_dsh();
	DEBUG("Successfully initialized\n");
  int status;
  logfd = open(LOG_FILE, O_CREAT | O_TRUNC | O_WRONLY, 0666);  

	while(1) {
        job_t *j = NULL;
		if(!(j = readcmdline(promptmsg()))) {
      //printf("%s\n", j->commandinfo);
			if (feof(stdin)) { /* End of file (ctrl-d) */
				fflush(stdout);
				printf("\n");
				exit(EXIT_SUCCESS);
      }
			continue; /* NOOP; user entered return or spaces with return */
		}

        /* Only for debugging purposes to show parser output; turn off in the
         * final code */
    printf("pid:%d\n", j->pgid);
    process_t* proc = j->first_process;
    if(builtin_cmd(j, proc->argc, proc->argv)) 
       continue;
    else if(!j->bg) {
       spawn_job(j, !j->bg);
     }
     else
      spawn_job(j, !j->bg);
    if(PRINT_INFO) print_job(j);

        /* Your code goes here */
        /* You need to loop through jobs list since a command line can contain ;*/
        /* Check for built-in commands */
        /* If not built-in */
            /* If job j runs in foreground */
            /* spawn_job(j,true) */
            /* else */
            /* spawn_job(j,false) */
    }
}
