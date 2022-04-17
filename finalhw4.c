#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#define LINELEN 1000 
#define JOBQLEN 100 
int argcount;
int check=0;
typedef struct job
{
    int jid;        
    pthread_t tid; 
    char *cmd;      
    char *stat;    
    char *start;   
    int errorstatus;
    char *stop;   
    char *Status;
    char fnout[10];
    char fnerr[10];
} job;

typedef struct queue
{
    int size;    
    job **buffer;
    int start; 
    int end;    
    int count;  
} queue;
int x=0;      
int noofjobsrunning;   
int global=0;
queue *q;   
int open_log(char *fn)
{
    int fd;
    if ((fd = open(fn, O_CREAT | O_APPEND | O_WRONLY, 0755)) == -1)
    {
        fprintf(stderr, "Error: failed to open \"%s\"\n", fn);
        perror("open");
        exit(EXIT_FAILURE);
    }
    return fd;
}
char *getTime(){
     time_t tim = time(NULL);
     return  ctime(&tim);
}
char *get(char *s)
{
    int i, c;
    char *value;
    i = -1;
    value = malloc(sizeof(char) * strlen(s));
    while ((c = s[++i]) != '\0')
        value[i] = c;
    value[i] = '\0';
    return value;
}
queue *queue_init(int n)
{
    queue *q = malloc(sizeof(queue));
    q->size = n;
    q->buffer = malloc(sizeof(job *) * n);
    q->start = 0;
    q->end = 0;
    q->count = 0;

    return q;
}
int queue_insert(queue *q, job *jp)
{
    if ((q == NULL) || (q->count == q->size))
        return -1;

    q->buffer[q->end % q->size] = jp;
    q->end = (q->end + 1) % q->size;
    ++q->count;

    return q->count;
}
void listalljobs(job *jobs, int n, char *input)
{
    int i;
    if (jobs != NULL && n != 0)
    {
        if (strcmp(input, "showjobs") == 0)
        {
            for (i = 0; i < n; ++i)
            {
                if (strcmp(jobs[i].stat, "complete") != 0){
                    printf("\nJob ID \t Command \t\t\t\t Status \n");
                    printf("%d \t %s \t\t\t\t %s \t",jobs[i].jid,jobs[i].cmd,jobs[i].stat);    
                }
            }
        }
        else if (strcmp(input, "submithistory") == 0)
        {
            for (i = 0; i < n; ++i)
            {
                if (strcmp(jobs[i].stat, "complete") == 0)
                    printf("\nJob ID \t Command \t\t Start Time \t\t End Time \t\t Status \n");
                    printf("%d \t %s \t\t %s \t\t %s \t\t %s",jobs[i].jid,jobs[i].cmd,jobs[i].start,jobs[i].stop,jobs[i].Status);
            }
        }
    }
}
job jobcreatefun(char *cmd, int jid)
{
   
    job j;
    j.jid = jid;
    j.cmd = get(cmd);
    if(jid >= argcount){
        j.stat = "waiting";
    }
    else{
        j.stat = "Running";
    }
    j.errorstatus = -1;
    j.Status = "failure";
    j.start = j.stop = NULL;
    sprintf(j.fnout, "%d.out", j.jid);
    sprintf(j.fnerr, "%d.err", j.jid);
    return j;
}
job *queue_delete(queue *q)
{
    if ((q == NULL) || (q->count == 0))
        return (job *)-1;

    job *j = q->buffer[q->start];
    q->start = (q->start + 1) % q->size;
    --q->count;

    return j;
}
int get_line(char *s, int n)
{
    int i, c;
    for (i = 0; i < n - 1 && (c = getchar()) != '\n'; ++i)
    {
        if (c == EOF)
            return -1;
        s[i] = c;
    }
    s[i] = '\0';
    return i;
}
void mainfun(job jobs[],int argcount)
{
    int i;           
    char line[LINELEN];
    char *input;      
    char *submitjob;   
    printf("submit\nshowjobs\nsubmithistory\n");
    i = 0;
    while (printf("\nEnter command> ") && get_line(line, LINELEN) != -1)
    {
        input=strtok(line, " ");
         char *mainvalue=strtok(NULL, "");
        if (input!=NULL)
        {
            if (strcmp(input, "submit") == 0)
            {
                submitjob =  mainvalue;
                jobs[i] = jobcreatefun(submitjob, i);
                queue_insert(q, jobs + i);
                printf("job %d added to the queue\n", i++);
                   
            }
            else if (strcmp(input, "showjobs") == 0 ||
                     strcmp(input, "submithistory") == 0)
                listalljobs(jobs, i, input);
        }
    }
    kill(0, SIGINT);
}
void *jobcompletion(void *arg)
{
    job *jp;    
    char *args[100]; 
    pid_t pid; 
    int h;
    jp = (job *)arg;
  ++noofjobsrunning;
        printf("check value = %d\n",check);
        if(check+1<=argcount){
    jp->stat = "Running";
    for(h=0;h<10;h++){
        sleep(h+1);
    }
    check++;
    jp->start = getTime();
    pid = fork();
    if (pid == 0) 
    {
        dup2(open_log(jp->fnout), STDOUT_FILENO); 
        dup2(open_log(jp->fnerr), STDERR_FILENO); 
         char *py=jp->cmd;
        char *str=strtok(py," ");
       int h=0;
        while(str!=NULL){
             args[h++]=str;
             str=strtok(NULL," ");
        }
        execvp(args[0], args);
        fprintf(stderr, "Error: command execution failed for \"%s\"\n", args[0]);
        perror("execvp");
        exit(EXIT_FAILURE);
    }
    else if (pid > 0) 
    {
        waitpid(pid, &jp->errorstatus, WUNTRACED);
        jp->stat = "complete";
         check--;
         printf("complete check value\n ");
        
        jp->stop = getTime();

        if (!WIFEXITED(jp->errorstatus)){
            fprintf(stderr, "Child process %d did not terminate normally!\n", pid);
            jp->Status="failure";
        }else{
            jp->Status="success";
        }
            
    }
    else
    {
        fprintf(stderr, "Error: process fork failed\n");
        perror("fork");
        exit(EXIT_FAILURE);
    }
}
    --noofjobsrunning;
    return NULL;
}
void *compute(void *arg)
{
    job *jp;
    noofjobsrunning = 0;
    for (;;)
    {
        if (q->count > 0 && noofjobsrunning < argcount)
        {
            jp = queue_delete(q);
            pthread_create(&jp->tid, NULL, jobcompletion, jp);
            pthread_detach(jp->tid);
        }
        sleep(1);
    }
    return NULL;
}
int main(int argc, char **argv)
{
    char *filenamenerr; 
    pthread_t tid; 
    if (argc != 2)
    {
        printf("Usage: %s jobs\n", argv[0]);
        exit(-1);
    }

    argcount = atoi(argv[1]);
    if (argcount < 1){
        argcount = 1;
    }
    else if (argcount > 8){
        argcount = 8;
    }
   job JOBS[argcount];
    global=argcount;
    filenamenerr = malloc(sizeof(char) * (strlen(argv[0]) + 10));
    sprintf(filenamenerr, "%s.err", argv[0]);
    dup2(open_log(filenamenerr), STDERR_FILENO);
    q = queue_init(JOBQLEN);
    pthread_create(&tid, NULL, compute, NULL);
    mainfun(JOBS,argcount);
   return 0;
}
