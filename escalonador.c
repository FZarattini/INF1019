#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>

#define MAX_NAME 50		//tamanho maximo do name dos programas
#define MAX_PROCESSES 200

//#define SIGUSR3 SIGWINCH
//#define SIGUSR4 SIGURG

#define SIGUSR3 SIGIO
#define SIGUSR4 SIGURG

//enum das políticas
typedef enum method {
	REALTIME, 
	PRIORITIES, 
	ROUNDROBIN
} Method;

//estrutura dos processos
typedef struct process {
	char name[MAX_NAME +1];
	int pid;
	Method method;
	int arg1, arg2;
} Process;

char *segmentoInterpretador;

Process realTIme[MAX_PROCESSES];
Process priorities[MAX_PROCESSES];
Process roundRobin[MAX_PROCESSES];

Process processoAtual = {.name="", .pid=-1, .method=-1, .arg1=-1, .arg2=-1}; // MUDAR ESSAS ATRIBUIÇÕES

//contadores
int countRealTimeProc = 0;
int countPrioritiesProc = 0;
int countRoundRobinProc = 0;

int countRemainingProc = 1; //muda p/ 0 quando o interpretador chegar no final do arquivo


//variaveis de tempo
unsigned long start = 0;
unsigned long now = 0;

//handlers
void insertProcess_Priorities(int sinal);
void insertProcess_RealTime(int sinal);
void insertProcess_RoundRobin(int sinal);
void endFile(int sinal); //detecta que o interpretador terminou de ler Exec.txt

//retorna a proxima politica a ser executada e caso REALTIME, process_index por referencia
int ProximaPolitica(int *process_index);
void methodPriority(int process_method, int i);
int CheckMaxProcesses();
int sort_RealTime(const void *a, const void *b);
int sort_Priorities(const void *a, const void *b);
void remove_RealTime(int index);
void remove_Priorities(int index);
void remove_RoundRobin(int index);
void stopProcess(Process p);
void continueProcess(Process p);
void RealTime(int i);
void Priority();
void RoundRobin();
int startProgram(char *path);

int main (int argc, char *argv[]) {
	int key;
	int process_method = -1;
	int i = -1;

	key = atoi(argv[1]);
	segmentoInterpretador = (char *) shmat(key, 0, 0);
	
	signal(SIGUSR1, insertProcess_Priorities);
	signal(SIGUSR2, insertProcess_RealTime);
	signal(SIGUSR3, insertProcess_RoundRobin);
	signal(SIGUSR4, endFile);
	
	//printf(" Escalonador iniciado!\n", now); fflush(stdout);
	printf("Iniciando Escalonador!\n");

	start = time(NULL);
		
	while(countRemainingProc || (countRealTimeProc + countPrioritiesProc + countRoundRobinProc > 0)) {
		now = ((unsigned long)time(NULL) - start) % 60;
		
		process_method = ProximaPolitica(&i);
		//methodPriority(process_method, i);
		if (process_method == REALTIME)
			RealTime(i);
		else if (process_method == PRIORITIES)
			Priority();
		else if (process_method == ROUNDROBIN)
			RoundRobin();		
		
	}
	
	//printf(" Todos os processos foram terminados!\n", now); fflush(stdout);
	printf("Todos os Processos finalizados. Terminando Escalonador!\n");
	//printf(" Encerrando escalonador.\n", now); fflush(stdout);
	
	shmdt(segmentoInterpretador);
	
	return 0;
}

/*int CheckMaxProcesses() {
	if (countRealTimeProc + countPrioritiesProc + countRoundRobinProc == MAX_PROCESSES) {
		//printf(": Numero maximo de processos atingido!\n", now);
		//fflush(stdout);
		printf("Nao eh possivel inserir mais processos!\n");		
		return 1;
	}
	
	return 0;
}*/

void insertProcess_RealTime(int sinal) {
	Process new;
	int i, end;
	int insert_quantity = 1;

	//if (CheckMaxProcesses())
	//	return;
	
	if (countRealTimeProc + countPrioritiesProc + countRoundRobinProc == MAX_PROCESSES) {
		//printf(": Numero maximo de processos atingido!\n", now);
		//fflush(stdout);
		printf("Nao eh possivel inserir mais processos!\n");		
		return;
	}
	
	new.method = REALTIME;

	sscanf(segmentoInterpretador, " %s %d %d", new.name, &new.arg1, &new.arg2);

	end = new.arg1 + new.arg2;

	//printf(" Recebeu %s I = %d D = %d\n",  new.name, new.arg1, new.arg2); fflush(stdout);
	printf("Recebeu %s I = %d D = %d\n", new.name, new.arg1, new.arg2);	
	
	//Verifica validade dos argumentos
	if (end > 60) {
		//printf(" Tempo de execucao invalido!\n", now);
		printf("Tempo de Execucao nao eh valido\n");
		//fflush(stdout);
		insert_quantity = 0;
	}
	else {
		for (i=0; i<countRealTimeProc; i++) { //checando conflitos com outros processos
			if (new.arg1 >= realTIme[i].arg1 && new.arg1 < realTIme[i].arg1 + realTIme[i].arg2) {
				//printf(" Conflito no start do intervalo de execucao!\n", now);
				//fflush(stdout);
				printf("A execucao do Processo %s entra em conflito com outro processo\n", new.name);	
				insert_quantity = 0;
				break;
			}
			else if (end > realTIme[i].arg1 && end <= realTIme[i].arg1 + realTIme[i].arg2) {
				//printf(" Conflito no start do intervalo de execucao!\n", now);
				//fflush(stdout);
				printf("A execucao do Processo %s entra em conflito com outro processo\n", new.name);
				//printf("")					
				insert_quantity = 0;
				break;
			}
		}
	}
	
	if (insert_quantity) {
		new.pid = startProgram(new.name);
	
		//insert_quantity
		realTIme[countRealTimeProc] = new;
		countRealTimeProc++;

		//ordena
		qsort(realTIme, countRealTimeProc, sizeof(Process), sort_RealTime);
		
		/*for (i=0; i<countRealTimeProc; i++) {
			printf(" realTIme[%d].start = %d realTIme[%d].end = %d\n", 
			i, realTIme[i].arg1, i, realTIme[i].arg1+realTIme[i].arg2); fflush(stdout);
		}*/
	}
}

void insertProcess_Priorities(int sinal) {
	Process new;
	//int i;

	//if (CheckMaxProcesses())
	//	return;

	if (countRealTimeProc + countPrioritiesProc + countRoundRobinProc == MAX_PROCESSES) {
		//printf(": Numero maximo de processos atingido!\n", now);
		//fflush(stdout);
		printf("Nao eh possivel inserir mais processos!\n");		
		return;
	}
	
	new.method = PRIORITIES;
	sscanf(segmentoInterpretador, " %s %d", new.name, &new.arg1);
	printf("Recebeu %s com Prioridade = %d\n",  new.name, new.arg1);
	//fflush(stdout);
	if (new.arg1 < 1 || new.arg1 > 7) {
		printf("Valor de Prioridade Invalida!\n");
		//fflush(stdout);
	}
	else {
		new.pid = startProgram(new.name);
	
		//insert_quantity
		priorities[countPrioritiesProc] = new;
		countPrioritiesProc++;
		
		//ordena
		qsort(priorities, countPrioritiesProc, sizeof(Process), sort_Priorities);
		/*for (i=0; i<countPrioritiesProc; i++) { //checando ordenacao dos processos
			printf(" priorities[%d].priorities = %d\n",  i, priorities[i].arg1); fflush(stdout);
		}*/
	}
}	

void insertProcess_RoundRobin (int sinal) {
	Process new;
	//int i;
	
	//if (CheckMaxProcesses())
	//	return;
	
	if (countRealTimeProc + countPrioritiesProc + countRoundRobinProc == MAX_PROCESSES) {
		//printf(": Numero maximo de processos atingido!\n", now);
		//fflush(stdout);
		printf("Nao eh possivel inserir mais processos!\n");		
		return;
	}
	
	new.method = PRIORITIES;
	sscanf(segmentoInterpretador, " %s", new.name);
	printf(" Recebeu %s\n",  new.name);
	fflush(stdout);
	
	new.pid = startProgram(new.name);
	
	roundRobin[countRoundRobinProc] = new;
	countRoundRobinProc++;
	
	/*for (i=0; i<countRoundRobinProc; i++) { //checando ordenacao dos processos
		printf(" roundRobin[%d].name = %s\n",  i, roundRobin[i].name); fflush(stdout);
	}*/
}

void endFile(int sinal) {
	countRemainingProc = 0;
}

int ProximaPolitica(int *process_index) {
	int i;
	
	//se existem processos realTIme pro tempo atual, retorna REALTIME
	if (countRealTimeProc != 0) {
		for (i = 0; i < countRealTimeProc; i++) {
			if (now >= realTIme[i].arg1 && now < realTIme[i].arg1 + realTIme[i].arg2) {
				*process_index = i;
				return REALTIME;
			}
		}
	}
	//se existem processos priorities, retorna PRIORITIES
	if (countPrioritiesProc != 0) {
		return PRIORITIES;
	}
	//senão, existem processos roundRobin, retorna ROUNDROBIN
	else if (countRoundRobinProc != 0) {
		return ROUNDROBIN;
	}
	else return -1;
}

int sort_RealTime(const void *a, const void *b) {
	Process *p1;
	Process *p2;
	
	p1 = (Process*)a;
	p2 = (Process*)b;
	
	return (p1->arg1 - p2->arg1);
}

int sort_Priorities(const void *a, const void *b) {
	Process *p1;
	Process *p2;
	
	p1 = (Process*)a;
	p2 = (Process*)b;
	
	return (p1->arg1 - p2->arg1);
}

void stopProcess(Process p) {
	printf(" Parando %s\n",  p.name); fflush(stdout);
	kill(p.pid, SIGSTOP);
}

void continueProcess(Process p) {
	printf(" Executando %s\n",  p.name); fflush(stdout);
	kill(p.pid, SIGCONT);
}

void RealTime(int i) {
	int terminou;

	if (realTIme[i].pid != processoAtual.pid) {
		if(processoAtual.pid != -1)
			stopProcess(processoAtual);
		processoAtual = realTIme[i];
		continueProcess(processoAtual);
	}
	
	terminou = waitpid(processoAtual.pid, 0, WNOHANG);
	if (terminou > 0) {
		printf(" %s Encerrou!\n",  processoAtual.name); fflush(stdout);
		processoAtual.method = processoAtual.pid = -1;
		remove_RealTime(i);
	}
}

void Priority() {
	int terminou;
	
	//testando se o process ainda não está sendo executado
	if ((processoAtual.method != PRIORITIES) ||
	(processoAtual.method == PRIORITIES && processoAtual.arg1 != priorities[0].arg1)) {
		if (processoAtual.pid != -1)
			stopProcess(processoAtual);
		processoAtual = priorities[0];
		continueProcess(processoAtual);
	}
	
	terminou = waitpid(processoAtual.pid, 0, WNOHANG);
	if (terminou > 0) {
		printf(" %s Encerrou!\n",  processoAtual.name); fflush(stdout);
		processoAtual.method = processoAtual.pid = -1;
		remove_Priorities(0);
	}
}

void RoundRobin() {
	int terminou;
	
	if (processoAtual.pid != roundRobin[0].pid) {
		if(processoAtual.pid != -1)
			stopProcess(processoAtual);
		processoAtual = roundRobin[0];
		continueProcess(processoAtual);
		sleep(1);
	}
	
	terminou = waitpid(processoAtual.pid, 0, WNOHANG);
	if (terminou > 0) {
		printf(" %s Encerrou!\n",  processoAtual.name); fflush(stdout);
		processoAtual.method = processoAtual.pid = -1;
		remove_RoundRobin(0);
	}
	else {	//tira e poe no final
		remove_RoundRobin(0);
		roundRobin[countRoundRobinProc] = processoAtual;
		countRoundRobinProc++;
	}
}

void remove_Priorities (int index) {
	int i;
	for (i = index; i<countPrioritiesProc-1; i++) {
		priorities[i] = priorities[i+1];
	}
	countPrioritiesProc--;
}

void remove_RealTime (int index) {
	int i;
	for (i = index; i<countRealTimeProc-1; i++) {
		realTIme[i] = realTIme[i+1];
	}
	countRealTimeProc--;
}

void remove_RoundRobin (int index) {
	int i;
	for (i = index; i<countRoundRobinProc-1; i++) {
		roundRobin[i] = roundRobin[i+1];
	}
	countRoundRobinProc--;
}

/*void methodPriority(int process_method, int i) {
	if (process_method == REALTIME)
		RealTime(i);
	else if (process_method == PRIORITIES)
		Priority();
	else if (process_method == ROUNDROBIN)
		RoundRobin();
}*/

int startProgram(char *path) {
	int pid;
	char prog[MAX_NAME + 2] = "./";
	char *chamada[3] = {NULL, NULL, NULL};
	
	strcat(prog, path);
	chamada[0] = prog;
	
	if ((pid = fork()) < 0) { //cria process que executara o programa
		printf("[ESC] Erro ao criar process do programa %s!\n", path); fflush(stdout);
		exit(-1);
	}
	else if (pid == 0) { //new programa
		execve(prog, chamada, 0);
		printf("[ESC] Erro ao iniciar o programa %s!\n", path); fflush(stdout);
		exit(-2);
	}
	
	kill(pid, SIGSTOP);
	
	return pid;
}
