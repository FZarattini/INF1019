#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define MAX_NAME 10		//Tamanho maximo do nome dos programas
#define SHMEM_MAX 1024		//Tamanho do segmento de memoria compartilhada

//Sinais que possuem como comportamento padrão não fazerem nada, apenas para que possamos usar os handlers
#define SIGUSR3 SIGIO
#define SIGUSR4 SIGURG

//Arquivo de entrada do interpretador
FILE *exec;


//Variáveis Globais
int segmentoEscalonador = -1;	//Segmento de memoria compartilhada
int pidEscalonador = -1;	//PID do Escalonador

//Declaração das funções auxiliares
void inicializaEscalonador();			
void inicializaProcessos();

//Handler		
void IntHandler(int signal);	//Encerra Escalonador	


int main (void) {
	int status;
	char pSegmentoEscalonador[MAX_NAME +1];
	char *call[3] = {"./inicializaEscalonador", NULL, NULL};
	
	segmentoEscalonador = shmget(IPC_PRIVATE, sizeof(char) * SHMEM_MAX, IPC_CREAT|IPC_EXCL|S_IRUSR|S_IWUSR);
	
	//Sinal para a finalização do Escalonador	
	signal(SIGINT, IntHandler);

	exec = fopen ("exec.txt", "r");
	if (exec == NULL) {
		printf("Erro ao abrir arquivo!\n"); 
		exit(-1);
	}
	
	//Cria Processo que irá executar o Escalonador
	if ((pidEscalonador = fork()) < 0) {
		printf("Erro ao criar processo Escalonador\n"); 
		exit(-3);
	}
	else if (pidEscalonador == 0) {	
		snprintf(pSegmentoEscalonador, MAX_NAME + 1 , "%d", segmentoEscalonador); // pSegmentoEscalonador aponta para SegmentoEscalonador e é passado para o escalonador
		call[1] = pSegmentoEscalonador;
		
		//Executa Escalonador
		execve("./escalonador", call, 0);
		printf("Erro ao executar Escalonador!\n"); 
		exit(-4);
	}
	
	//Garante que Escalonador é inicializado antes de continuar
	sleep(1); 
	
	if (pidEscalonador != -1) {
		//Lê do arquivo os Programa e seus argumentos
		inicializaProcessos();
		//Espera Escalonador terminar
		waitpid(pidEscalonador, &status, 0);
	}
	
	//Fecha arquivo exec.txt
	fclose(exec);
	
	shmctl(segmentoEscalonador, IPC_RMID, 0);
	
	return 0;
}

//Função que Encerra o Escalonador
void IntHandler(int signal){
	if(pidEscalonador != -1) {
		kill(pidEscalonador, SIGINT);	//encerra Escalonador
	}
	
	fclose(exec);
	shmctl(segmentoEscalonador, IPC_RMID, 0);
	
	exit(0);
}


//Lê arquivo de entrada e envia os nomes dos programas e seus argumentos para a memória compartilhada, levantando os sinais
//corretos para que o escalonador possa inserir os Processos nas Filas corretas.
void inicializaProcessos(){
	char nomeProg[MAX_NAME + 1];
	char *shmEscalonador;
	char temp_name[MAX_NAME];	
	char prox;	
	int arg1, arg2;		
	
	//Attach da área de memória compartilhada para a variável 
	shmEscalonador = (char *) shmat(segmentoEscalonador, 0, 0);
	
	//Lê o nome do Programa 
	while (fscanf(exec, " Exec %s", nomeProg) == 1) {
		snprintf(shmEscalonador, MAX_NAME + 1, "%s", nomeProg); //Salva na memória compartilhada o nome do programa sem imprimir na saída
		
		fscanf(exec, "%c", &prox); //Lê próximos caracteres um a um
		while (prox == ' ') {
			fscanf(exec, "%c", &prox);
		}
		
		if (prox == 'P') {	//Política Prioridades
			fscanf(exec, "R=%d", &arg1);
			
			//Concatena o argumento lido ao nome salvo na memória compartilhada
			snprintf(temp_name, MAX_NAME, " %d", arg1); 
			strcat(shmEscalonador, temp_name); 
			
			strcat(shmEscalonador, temp_name);

			//Envia SIGUSR1 para que Escalonador saiba que deve inserir na Fila de Prioridades
			kill(pidEscalonador, SIGUSR1);
			printf("%s PR = %d enviado!\n", nomeProg, arg1); fflush(stdout);
		}
		else if (prox == 'I') { //Política Real Time
			fscanf(exec, "=%d D=%d", &arg1, &arg2);
			
			//Concatena o argumento lido ao nome salvo na memória compartilhada
			snprintf(temp_name, MAX_NAME, " %d", arg1);
			strcat(shmEscalonador, temp_name);
			snprintf(temp_name, MAX_NAME, " %d", arg2);
			strcat(shmEscalonador, temp_name);
			strcat(shmEscalonador, temp_name);

			//Envia SIGUSR2 para que Escalonador saiba que deve inserir na Fila de Real Time
			kill(pidEscalonador, SIGUSR2);
			printf("%s I = %d D = %d\n enviado!\n", nomeProg, arg1, arg2); fflush(stdout);
		}
		else if (prox == '\n') { //Política Round Robin
			temp_name[0] = '\0'; //Limpamos a variável que estava vindo com lixo

			//Não possui argumento
			strcat(shmEscalonador, temp_name);
			
			//Envia SIGUSR3 para que Escalonador saiba que deve inserir na Fila de Real Time
			kill(pidEscalonador, SIGUSR3);
			printf("%s enviado!\n", nomeProg); fflush(stdout);
		}
		

		sleep(1);
	}
	
	//Envia SIGUSR4 para que Escalonador saiba que terminou a leitura do Arquivo
	kill(pidEscalonador, SIGUSR4);
	
	//Detach da variável ligada à memória compartilhada
	shmdt(shmEscalonador);
}
