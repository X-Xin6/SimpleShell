#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h> //this was automatic with gcc-6... get with it, SOCS
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>


#define MAXJOBS  20
pid_t bgCpids[20];
int Num_Child = 0;
pid_t s_pid;
int Builtin_Commands(char** argv, int bg, int cnt);
void evaluate(char **args,int bg);
int  simple_pipe(  char* first[], int cnt1,char* second[],int cnt2);

void freeArray(char* Command[],int cnt){
    for (int i = 0; i < cnt-1; i++) {
      free(Command[i]);
    }
}

void copyArray(char* args[], char* shortCommand[],int start,  int cnt2){
      for (int i = 0; i < cnt2; i++) {
        if (i == cnt2-1) { 
          shortCommand[i] = '\0';
          break;
        }
    shortCommand[i] = malloc((strlen(args[i+start])+1) * sizeof(char));
    strcpy(shortCommand[i], args[i+start]);
  }
}


int getcmd(char *prompt, char *args[], int *background)
{
    int length, i = 0;
     char *token, *loc;
     char *line = NULL;
     size_t linecap = 0;
     printf("%s", prompt);
     length = getline(&line, &linecap, stdin);
     if (length <= 0) {
         exit(-1);
          }
          // Check if background is specified..
          if ((loc = index(line, '&')) != NULL) {
              *background = 1;
              *loc = ' ';
              }
               else
               *background = 0;
               while ((token = strsep(&line, " \t\n")) != NULL) {
                   for (int j = 0; j < strlen(token); j++)
                   if (token[j] <= 32)
                   token[j] = '\0';
                   if (strlen(token) > 0)
                   args[i++] = token;
                   }
                   return i;
 }

// Signal handler function
void handler(int signum)
{
        switch (signum) {
                case SIGINT:
                        fprintf(stderr, "\nAn interrupt signal has been received.\n");  
                        fflush(stderr);
                         if(getpid()!=s_pid){
                           exit(EXIT_SUCCESS);
                           }

                case SIGCHLD:;     
                         pid_t deadChild = waitpid(-1, NULL, WNOHANG);
                          if ( deadChild > 0) {
                          fprintf(stderr, "\nA  dead child has been reaped.\n");
                          fprintf(stderr, "Exiting...\n");
                          fflush(stderr);
                           for (int i = 0; i < Num_Child; i++) {
                          if (deadChild == bgCpids[i]) {
                              bgCpids[i] = 0;
                               }
                            }
                          }
                default:
                        return;
        }
        fflush(stdout);
}

// Signal catcher function
void signals() {
        struct sigaction act;
        act.sa_handler = handler;
        sigemptyset(&act.sa_mask); 
        act.sa_flags = SA_RESTART;
        sigaction(SIGINT, &act, NULL);
        sigaction(SIGCHLD, &act, NULL);
        signal(SIGTSTP, SIG_IGN); 
}      

int  SImple_output_exe(char* argv[],int cnt) 
{        //looking for output sign ">
        for( int i =0; argv[i] != NULL; ++i)
        {
            if(strcmp(argv[i], ">") == 0) 
            {
                if(argv[i+1] == NULL)// no argument after ">"
                  perror("command '>'[option]?"),exit( 1);
                argv[i] = NULL; 
                //open/create a file
                int fd =open(argv[i+1], O_RDWR|O_CREAT|O_TRUNC, 0664);
                if(fd == -1)perror("open"),exit( 1);
                //redirection
                dup2(fd, 1); //dup2(oldfd, newfd);
                close(fd);
                return (cnt-2);
            }
        }
        return cnt;
}

                   
void evaluate(char *args[],int bg){
    bg=0;
    int status;
    int cnt = getcmd("\n>> ", args, &bg);

    while (Builtin_Commands(args, bg, cnt) == 1) {
      for (int i = 0; i < cnt; i++) { 
        memset(args[i], 0, strlen(args[i]));
      }
      cnt = getcmd("\n>> ", args, &bg);
    }
    /*(1) fork a child process using fork()*/
    pid_t pid = fork();
    if (pid < 0) {
      perror("fork error");
      exit(1);
    }

    if ((pid = fork()) == 0) { //child
          if (bg == 1) {
        signal(SIGINT, SIG_IGN); 
          }
      char* cArgs[cnt+1];
     copyArray(args, cArgs,0,  cnt+1);
      cnt = SImple_output_exe(cArgs, cnt);


      /*(2) the child process will invoke execvp()*/


      if (execvp(cArgs[0], cArgs) < 0) {
        	printf("%s: Command not found.\n", args[0]);
        perror("execvp error");
        exit(1);
      }
    }

/*(3) if background is not specified, the parent will wait,
otherwise parent starts the next command.*/

 //parent
      if (bg == 0) { //if foreground, will wait until completion or interrupt
        waitpid(pid, &status, 0);
      } else {
        bgCpids[Num_Child++] = pid; //maintains jobs list
        printf("background job '%s' : PID %d\n", args[0], pid);
      }
       freeArray(args,cnt-2);
  }

  void print_jobs(){
     printf("Background jobs:\n\n");
    for (int i = 0; i < Num_Child; i++) {
      if (bgCpids[i] != 0) {
        printf("index%d : %drunining\n", i, bgCpids[i]);
      } else {
        
           printf("%d : terminated\n", i);
      }
    }
  }

int Builtin_Commands(char** argv, int bg, int cnt) {

    if (!strcmp(argv[0],"cd") ) { 
    if (chdir(argv[1]) < 0) {
      printf("Errors: Unable to change the directory. \n");
    }
    return 1;
  }
   if (!strcmp(argv[0],"pwd") ) { 
    printf("%s\n", getcwd(NULL, 0));
    return 1;
  }

  if (!strcmp(argv[0],"exit") ) { 
    exit(0);
    return 1; 
  }
    if (strcmp(argv[0], "jobs") == 0) { 
      //jobs input
      print_jobs();
    return 1;
  }

  if (strcmp(argv[0], "fg") == 0) { //fg input
    int index = atoi(argv[1]); 
    if (index < Num_Child) { 
       tcsetpgrp(0, getpgid(index));
      printf("Foreground job became %d : with PID %d\n", index, bgCpids[index]);
      waitpid(bgCpids[index], NULL, 0);
        tcsetpgrp(0, getpgid(s_pid));

    } else {
      printf("Invalid index. \n");
    }
    return 1;
  }
  for (int i = 0; i < cnt; i++) {
    if (strcmp(argv[i], "|") == 0) {
      int cnt1=i+1;
      int cnt2=cnt-i;
      char* Input[cnt1];
      char* Output[cnt2];
      copyArray(argv,Input,0,cnt1);
      copyArray(argv,Output,i+1,cnt2);
      simple_pipe( Input,cnt1,Output,cnt2);
      freeArray( Input,cnt1);
      freeArray(Output,cnt2); 
      return 1;
    }
  }
  return 0;
}
int  simple_pipe(  char* first[],int cnt1,char* second[],int cnt2){
    //2 ends of the pipe
    int fd[2];
    if(pipe(fd)==-1){
        return  1;
    }
    int pid1=fork();
    if(pid1<0){
        return 2;
    }

    if(pid1==0){
        //child process ping
        dup2(fd[1],STDOUT_FILENO);
        close(fd[0]);
        close(fd[1]);
        if (execvp(first[0], first) < 0) { //if execvp fails, process exits
        perror("execvp error");
        exit(EXIT_FAILURE);
      }
    }
    int pid2=fork();
    if(pid2<0){
        return 3;
    }
    if(pid2==0){
        //Child  process 2(grep)
        dup2(fd[0] ,STDIN_FILENO);
        close(fd[0]);
        close(fd[1]);
        if (execvp(second[0], second) < 0) { //if execvp fails, process exits
        perror("execvp error");
        exit(EXIT_FAILURE);
      }
    }
    close(fd[0]);
    close(fd[1]);
    waitpid(pid1,NULL,0);
    waitpid(pid2,NULL,0);
    return 0;
} 

int main(void){
char *args[20];
// Call the signal catching function
signals();
while(1) {
      int bg;
      s_pid=getpid();
     evaluate(args,bg);
 }
}