#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define MAX_NAME 10		//tamanho maximo do nome dos programas
#define SHMEM_MAX 1024		//tamanho do segmento de memoria compartilhada

#define SIGUSR3 SIGWINCH
#define SIGUSR4 SIGURG

FILE *exec;		//arquivo de entrada do interpretador

int pidEscalonador = -1;	//variavel p/ guardar o pid do inicializaEscalonador
int segmentoEscalonador = -1;	//segmento de memoria compartilhada p/ comunicacao com o inicializaEscalonador
void SigIntHandler(int signal);		//handler para SIGINT
void inicializaEscalonador();		//inicia o inicializaEscalonador
void inicializaProcessos();		//parse do arquivo exec para chamar os programas e inserir no inicializaEscalonador
int inicializaPrograma(char *path);		//inicia o programa e retorna o pid


int main (void) {
	int status;
	char pSegmentoEscalonador[MAX_NAME +1];
	char *chamada[3] = {"./inicializaEscalonador", NULL, NULL};
	
	segmentoEscalonador = shmget(IPC_PRIVATE, sizeof(char) * SHMEM_MAX, IPC_CREAT|IPC_EXCL|S_IRUSR|S_IWUSR);
		
	signal(SIGINT, IntHandler);

	exec = fopen ("exec.txt", "r");
	if (exec == NULL) {
		printf("Erro ao abrir arquivo!\n"); 
		//fflush(stdout);
		exit(-1);
	}

	//inicializaEscalonador();
	
	if ((pidEscalonador = fork()) < 0) {	//cria processo que executara o inicializaEscalonador
		printf("Erro ao criar processo Escalonador\n"); 
		//fflush(stdout);
		exit(-3);
	}
	else if (pidEscalonador == 0) {		//inicializaEscalonador
		snprintf(pSegmentoEscalonador, MAX_NAME + 1 , "%d", segmentoEscalonador); // pSegmentoEscalonador aponta para SegmentoEscalonador e é passado para o escalonador
		chamada[1] = pSegmentoEscalonador;
		
		//finalmente, comeca a execucao do inicializaEscalonador
		execve("./escalonador", chamada, 0);
		printf("Erro ao executar Escalonador!\n"); 
		//fflush(stdout);
		exit(-4);
	}
	
	sleep(1); //espera a inicializacao do inicializaEscalonador
	
	if (pidEscalonador != -1) {
		inicializaProcessos();
		waitpid(pidEscalonador, &status, 0);
	}
	
	printf("inicializaEscalonador encerrado. Finalizando interpretador.\n");
	
	fclose(exec);
	
	shmctl(segmentoEscalonador, IPC_RMID, 0);
	
	return 0;
}


void IntHandler(int signal){
	if(pidEscalonador != -1) {
		kill(pidEscalonador, SIGINT);	//encerra Escalonador
	}
	
	fclose(exec);
	shmctl(segmentoEscalonador, IPC_RMID, 0);
	
	exit(0);
}

/*
void inicializaEscalonador(){
	char pSegmentoEscalonador[MAX_NAME +1];
	char *chamada[3] = {"./inicializaEscalonador", NULL, NULL};
	
	segmentoEscalonador = shmget(IPC_PRIVATE, sizeof(char) * SHMEM_MAX, IPC_CREAT|IPC_EXCL|S_IRUSR|S_IWUSR);
	
	if ((pidEscalonador = fork()) < 0) {	//cria processo que executara o inicializaEscalonador
		printf("[INT] Erro ao criar processo do inicializaEscalonador!\n"); fflush(stdout);
		exit(-3);
	}
	else if (pidEscalonador == 0) {		//inicializaEscalonador
		//guardando em pSegmentoEscalonador a referencia p/ o segmento de memoria compartilhada p/ o inicializaEscalonador pegar
		snprintf(pSegmentoEscalonador, MAX_NAME + 1 , "%d", segmentoEscalonador);
		chamada[1] = pSegmentoEscalonador;
		
		//finalmente, comeca a execucao do inicializaEscalonador
		execve("./inicializaEscalonador", chamada, 0);
		printf("[INT] Erro ao executar inicializaEscalonador!\n"); fflush(stdout);
		exit(-4);
	}
	
	sleep(1); //espera a inicializacao do inicializaEscalonador
}
*/

void inicializaProcessos(){
	char *shmEscalonador;
	char nomeProg[MAX_NAME + 1];
	char aux[MAX_NAME];	//string auxiliar p/ guardar argumentos e pid
	char prox;	//char auxiliar
	int a1, a2;		//argumentos para as chamadas ao inicializaEscalonador
	
	shmEscalonador = (char *) shmat(segmentoEscalonador, 0, 0);
	
	//le do arquivo "Exec" e o nome do programa a ser chamado
	while (fscanf(exec, " Exec %s", nomeProg) == 1) {
		snprintf(shmEscalonador, MAX_NAME + 1, "%s", nomeProg);
		
		fscanf(exec, "%c", &prox);
		while (prox == 32) {	//ignorando espacos, mas nao quebras de linha
			fscanf(exec, "%c", &prox);
		}
		
		if (prox == 'P') {	//PRIORIDADE!
			fscanf(exec, "R=%d", &a1);
			
			//concatenando na memoria compartilhada o parametro p/ leitura do inicializaEscalonador
			snprintf(aux, MAX_NAME, " %d", a1);
			strcat(shmEscalonador, aux);
			
			//pid = inicializaPrograma(nomeProg); //iniciar processo
			//snprintf(aux, MAX_NAME, " %d", pid);//insere o pid na memoria compartilhada
			strcat(shmEscalonador, aux);
			kill(pidEscalonador, SIGUSR1);//mandar sinal p/ insercao de processo com prioridade lida
			printf("Enviou %s PR = %d\n", nomeProg, a1); fflush(stdout);
		}
		else if (prox == 'I') { //REAL TIME!
			fscanf(exec, "=%d D=%d", &a1, &a2);
			
			//concatenando na memoria compartilhada os parametros p/ leitura do inicializaEscalonador
			snprintf(aux, MAX_NAME, " %d", a1);
			strcat(shmEscalonador, aux);
			snprintf(aux, MAX_NAME, " %d", a2);
			strcat(shmEscalonador, aux);
			
			//pid = inicializaPrograma(nomeProg); //iniciar processo
			//snprintf(aux, MAX_NAME, " %d", pid);//insere o pid na memoria compartilhada
			strcat(shmEscalonador, aux);
			kill(pidEscalonador, SIGUSR2);//mandar sinal p/ insercao de processo com prioridade lida
			printf("Enviou %s I = %d D = %d\n", nomeProg, a1, a2); fflush(stdout);
		}
		else if (prox == '\n' || prox == 10) { //ROUND ROBIN!
			//pid = inicializaPrograma(nomeProg); //iniciar processo
			//snprintf(aux, MAX_NAME, " %d", pid);//insere o pid na memoria compartilhada
			strcat(shmEscalonador, aux);
			kill(pidEscalonador, SIGUSR3);//mandar sinal p/ insercao de processo com prioridade lida
			printf("Enviou %s\n", nomeProg); fflush(stdout);
		}
		
		//printf("[INT]  %s %d %d %d\n", nomeProg, a1, a2, pid); fflush(stdout);
		//printf("[INT] Iniciou %s\n", shmEscalonador); fflush(stdout);
		sleep(1);
	}
	
	kill(pidEscalonador, SIGUSR4);
	
	shmdt(shmEscalonador);
}

int inicializaPrograma(char *path) {
	int pid;
	char prog[MAX_NAME + 2] = "./";
	char *chamada[3] = {NULL, NULL, NULL};
	
	strcat(prog, path);
	chamada[0] = prog;
	
	if ((pid = fork()) < 0) { //cria processo que executara o programa
		printf("Erro ao criar processo do programa %s!\n", path);
		exit(-5);
	}
	else if (pid == 0) { //novo programa
		execve(prog, chamada, 0);
		printf("Erro ao iniciar o programa %s!\n", path);
		exit(-6);
	}
	
	kill(pid, SIGSTOP);
	
	return pid;
}
