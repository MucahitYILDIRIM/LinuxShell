#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <termios.h>

#define TRUE 1
#define FALSE !TRUE
#define LIMIT 256 
#define MAXLINE 1024 


#define GRN   "\x1B[32m" //renk kodları tanimlandi
#define BLU   "\x1B[34m"
#define RESET "\x1B[0m"

static char* currentDirectory;
void bgHandlerControl(int);
int program_quit();
int singleProccessingBg(char**);

void welcomeScreen() // karsilama ekrani tasarlandi 
{
        printf("\n\t============================================\n");
        printf("\t                   C Shell  \n");
        printf("\t--------------------------------------------\n");
        printf("\t                                  \n");
        printf("\t============================================\n");
        printf("\n\n");
}

void PromptBas() // kullanicinin her komutundan sonra prompt basmasi saglandi
{
        char hostn[1204] = "";
        gethostname(hostn, sizeof(hostn));
        printf(GRN "%s@%s:"RESET BLU "%s" RESET ">", getenv("LOGNAME"), hostn, getcwd(currentDirectory, 1024));
}

int  singleProccessing(char** temp,int background) //tekli komut icrasi
{
        
        if(background == 0) 
        {
               
                pid_t pid;
                pid=fork();
                if (pid == 0)
                {
                        if (execvp(temp[0],temp) == -1)
                        {

                                printf("Command not Found");
                                kill(getpid(),SIGTERM); 
                        }
                }
                else if (pid < 0)
                {
                        perror("exec");
                }
                else
                {
                        waitpid(pid,NULL,0);
                }
                return 1;
        }
        else if(background == 1) //arkaplanda calisma durumu
        {
                 return singleProccessingBg(temp);
        }
}

int singleProccessingBg(char ** args)
{
        pid_t pid;
        int status;

        struct sigaction act;
        act.sa_handler = bgHandlerControl; // arkaplanda islem tamamlanmasi durumunda prosesin handle ozelligi tutuldu
        sigemptyset(&act.sa_mask);
        act.sa_flags = SA_NOCLDSTOP;
        if (sigaction(SIGCHLD,&act,NULL)<0)
        {
                fprintf(stderr,"sigaction failed\n");
                return 1;
        }

        pid=fork();
        if (pid == 0)
        {
                if (execvp(args[0],args) == -1)
                {
                        perror("exec");
                        kill(getpid(),SIGTERM);
                }
        }
        else
        {
                printf("Proses PID:%d Degeriyle Olusturuldu\n",pid); // proses degeri yazildi
        }
        return 1; 
}

void bgHandlerControl(int signo) 
{
    int status, child_val,chid;
    chid = waitpid(-1, &status, WNOHANG);
    if (chid > 0)
    {
        if (WIFEXITED(status)) // arkaplanda calisan komutun statusu kontrol edildi
        {
            child_val = WEXITSTATUS(status);  
            printf("[%d] retval : %d \n",chid, child_val); 
        }
    }
}
        
void inputProcessing(char *args[],char *inputFile)
{
        
        pid_t pid;
        if (!(access (inputFile,F_OK) != -1)) // input dosyasinin varligi sorgulandi
        {   
                printf("%s Not Found!!\n",inputFile);
                return;
        }
        else{
                int fileDescriptor;
                if((pid=fork()) == -1)
                {
                        printf("Child olusturulamadi\n");
                        return;
                }
                if (pid==0)
                {
                        fileDescriptor=open(inputFile, O_RDONLY, 0600); // okunabilir olarak input dosyasi acildi

                                dup2(fileDescriptor,STDIN_FILENO);
                                close(fileDescriptor);

                                if (execvp(args[0],args)==-1)   
                                {
                                        perror("exec");
                                        kill(getpid(),SIGTERM);
                                } 
                }
                waitpid(pid,NULL,0);

        }
}

void outputProcessing(char *args[],char *outputFile)
{        
        pid_t pid;
        int fileDescriptor;

        if((pid=fork()) == -1)
        {
                printf("Child olusturulamadi\n");
                return;
        }
        if (pid==0)
        {
                fileDescriptor=open(outputFile, O_CREAT | O_TRUNC | O_WRONLY, 0600); // yazilabilir dosya acildi yoksa olusturuldu
                dup2(fileDescriptor,STDOUT_FILENO);
                close(fileDescriptor);

                if (execvp(args[0],args)==-1)  
                {
                        
                        perror("exec");
                        kill(getpid(),SIGTERM);
                } 
        }
        waitpid(pid,NULL,0);
}


int isPipe(char ** args)
{
        int numTokens = 0;
        while(args[numTokens] != NULL)
        {
                if(strcmp(args[numTokens],"|") == 0) // komutun pipe ile bagliligi sorgulandi
                {
                        return 1;
                }
                numTokens++;
        }
        return 0;
}

int execPipe(char** args)
{
        char * temp[256][256];
        int i = 0;
        int j = 0;
        int counter = 0;
        while (args[counter] != NULL) // alinan komutlar bir degiskene kaydedildi
        {
                if (strcmp(args[counter],"|") == 0)
                { 
                        temp[i][j] = NULL;
                        j=0;       
                        i++;
                }
                else
                {
                        temp[i][j] = args[counter];
                        j++;   
                }
                counter++;
        } 
        temp[i][j]=NULL;
        int pipefd[i+1];
        pid_t pid[i+1];
        int iTemp = i;

        pipe(pipefd);
        int iCounter=0;
        while(iCounter <= i) // komutlarin pipe ile calismasi saglandi
        {
                 int convertCounter = 0;
                 char * token[256];
                while(temp[iCounter][convertCounter] != NULL)
                {
                         token[convertCounter] = temp[iCounter][convertCounter];
                         convertCounter++;
                }
                token[convertCounter]=NULL;
                pid[iCounter]=fork();
                if (pid[iCounter] == 0) 
                {
                        
                        if(iTemp == i){
                                dup2(pipefd[iTemp], STDOUT_FILENO);
                        }
                        else{
                                dup2(pipefd[iTemp], STDIN_FILENO); 
                        }
                        close (pipefd[iCounter]);
                        execvp(token[0],token);
                        perror("pipe");
                        return 1;
                }
                iCounter++;
                iTemp--;
        }

        iCounter=0;
        while(iCounter <= i)
        {
                close(pipefd[iCounter]);
                iCounter++;
        }

        iCounter=0;

        while(iCounter <= i)
        {
                waitpid(pid[iCounter],NULL,0); // islem sonucanmasi bekletildi
                iCounter++;
        }
        return 0;
}

int program_quit()
{
    int status;
    while (!waitpid(-1,&status,WNOHANG)){} //programdan cikis icin islemlerin tamamlanasi beklendi
    exit(0);
}


int main (int argc, char **argv, char **envp)
{
        welcomeScreen();
        char line[MAXLINE];
        char *tokens[LIMIT];
        int numTokens;

        
        while(1)
        {
                PromptBas();
                memset(line, '\0',MAXLINE);
                fgets(line,MAXLINE,stdin); // komut alindi
                if((tokens[0] = strtok(line," \n\t")) == NULL) continue; //komut tokenlere ayrildi
                numTokens = 1; 
                while((tokens[numTokens] = strtok(NULL, " \n\t")) != NULL) numTokens++; 
                int status=0;
                if(strcmp(tokens[0],"quit") == 0) // quit  komutu ile cikis saglandi
                {
                        program_quit();
                }
                char *temp[256];
                int commandNumber=0;
                char *executableCommand[256];
                int isExecutableCommand = 0;
                int countLen;
                int backgroundBg =0;
                for(countLen = 0; tokens[countLen] != NULL ; countLen++);
                
                if(strcmp(tokens[countLen-1],";") != 0) 
                {
                        tokens[countLen] = ";";
                }
                int len = 0 ;

                while(tokens[commandNumber]!= NULL)// komutlar ; ile ard arda calistirilabilir hale getirildi
                {
                                
                        if (strcmp(tokens[commandNumber],";") == 0)
                        {       
                                isExecutableCommand = 1;
                                executableCommand[len] = NULL;
                        }
                        else 
                        {
                                executableCommand[len] = tokens[commandNumber];
                                commandNumber++;
                                len++;
                        }
                        
                        if(isExecutableCommand == 1) // calistirlabilir komut elde edildi
                        {
                               

                                int pipe = isPipe(executableCommand); // pipe olup olmadigi kontrol edildi
                                if(pipe == 1)
                                {
                                        execPipe(executableCommand); 
                                }
                                else
                                {
                                        status = 0;
                                        int counter = 0;
                                        while(executableCommand[counter] != NULL)
                                        {
                                                if (strcmp(executableCommand[counter],"<") == 0) // giris yonlendirme islemi algilandi
                                                {
                                                        status=1;
                                                        break;
                                                        
                                                }
                                                else if (strcmp(executableCommand[counter],">") == 0) // cikis yonlendirme islemi algilandi
                                                {
                                                        status=2;
                                                        break;
                                                }
                                                temp[counter] = executableCommand[counter];
                                                counter++;
                                        }
                                        temp[counter]=NULL;

                                        if(status==1)
                                        {
                                                inputProcessing(temp,executableCommand[counter + 1]); 
                                        }  
                                        else if(status==2)
                                        {

                                                outputProcessing(temp,executableCommand[counter + 1]); 
                                        }
                                        else
                                        {
                                                backgroundBg =0; 
                                                char *tempBg[256];
                                                int jBg = 0;

                                                while(temp[jBg] != NULL) //arkaplan kontrolu
                                                {
                                                        if (strcmp(temp[jBg],"&") == 0)
                                                        {
                                                                backgroundBg=1;
                                                                break;
                                                                
                                                        }
                                                        tempBg[jBg] = temp[jBg];
                                                        jBg++;
                                                }
                                                tempBg[jBg]=NULL;
                                                singleProccessing(tempBg,backgroundBg);
                                        }

                                }

                                isExecutableCommand = 0;
                                commandNumber++;
                                len=0;
                                pipe = 0;
                                for(int m = 0; executableCommand[m]!=NULL;m++)
                                {               
                                        executableCommand[m]=NULL;
                                }
                        }

                } 
        }
}