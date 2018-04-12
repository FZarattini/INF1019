#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <sys/wait.h>

typedef struct processo{
	char* p_path;
	int p_prioridade;
	int p_inicio;
	int p_duracao;	
	struct processo *prox;
}Processo;

typedef struct cabeca{
	Processo *corr;
}Cabeca; 
	
int main(void){
	int segmento1, segmento2, segmento3, segmento4, segmento5, *fimArq = 0, *prioridade, status, *inicio, *duracao;
	char* path;
	long pos;
	
	segmento1 = shmget(IPC_PRIVATE, 50*sizeof(char), IPC_CREAT|IPC_EXCL|S_IRUSR|S_IWUSR);
	segmento2 = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT|IPC_EXCL|S_IRUSR|S_IWUSR);
	segmento3 = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT|IPC_EXCL|S_IRUSR|S_IWUSR);
	segmento4 = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT|IPC_EXCL|S_IRUSR|S_IWUSR);
	segmento5 = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT|IPC_EXCL|S_IRUSR|S_IWUSR);	

	path = (char*)shmat(segmento1, 0, 0);
	prioridade = (int*)shmat(segmento2,0,0);
	inicio = (int*)shmat(segmento3,0,0);
	duracao = (int*)shmat(segmento4,0,0);
	fimArq = (int*)shmat(segmento5,0,0);
	
	if(fork()!=0){
		//Interpretador

		FILE *in;
		char* path;
			

		in = fopen("exec.txt", "r");

		while(EOF){
			//lê exec.txt
			pos = ftell(in); //guarda posição do inicio da linha
			if(fscanf(in,"%s", path)){}
			else{
				fseek(in, pos, SEEK_SET); //volta para inicio da linha se formato nao for correto
				if(fscanf(in, "%s PR=%d", path, prioridade)){}
				else{
					fseek(in, pos, SEEK_SET); //volta para inicio da linha se formato nao for correto
					if(fscanf(in, "%s I=%d D=%d", path, inicio, duracao)){}
					else{
						printf("Erro de leitura\n"); //caso não esteja em nenhum formato correto					
					}				
				}			
			}
				
			sleep(1);
		}
		*fimArq = 1;

		waitpid(-1, NULL, 0);
	}else{
		//Escalonador
		
		//ENCADEAR LISTA AQUI	
		
		while(*fimArq != 1){
			
		}
	}
}
