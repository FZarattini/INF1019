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

/*
*Criar as politicas de escalonamento (FALTA REAL TIME)
*Implementar as prioridades entre as politicas (FALTA REAL TIME)
*Fazer uns programas IO-bound e CPU-bound pra testar
*Relatorio
*/

#define NUMPROCESSOS 100
#define MAXNOME 10		//tamanho maximo do nome dos programas

#define SIGUSR3 SIGWINCH
#define SIGUSR4 SIGURG


typedef enum politica {REALTIME, PRIORIDADE, ROUNDROBIN} Politica;

typedef struct processo {
	char nome[MAXNOME +1];
	int pid;
	Politica politica;
	int a1, a2;
} Processo;

char *segCompInt;
Processo RT[NUMPROCESSOS];
Processo PR[NUMPROCESSOS];
Processo RR[NUMPROCESSOS];
Processo processoAtual = {.nome="", .pid=-1, .politica=-1, .a1=-1, .a2=-1};
int processosPendentes = 1; //muda p/ 0 quando o interpretador chegar no final do arquivo
int numRT = 0;
int numPR = 0;
int numRR = 0;
unsigned long inicio = 0;
unsigned long segAtual = 0;

void RecebePR(int sinal);
void RecebeRT(int sinal);
void RecebeRR(int sinal);
void FimExec(int sinal); //detecta que o interpretador terminou de ler Exec.txt
//retorna a proxima politica a ser executada e caso REALTIME, idxProcesso por referencia
int ProximaPolitica(int *idxProcesso);
void Esc(int pol, int i);
int ChecaNumProc();
int OrdenaRT(const void *a, const void *b);
int OrdenaPR(const void *a, const void *b);
void RemoveRT(int idx);
void RemovePR(int idx);
void RemoveRR(int idx);
void Interrompe(Processo p);
void Continua(Processo p);
void RealTime(int i);
void Prioridade();
void RoundRobin();
int IniciaPrograma(char *path);

int main (int argc, char *argv[]) {
	int chave;
	int pol = -1;
	int i = -1;

	chave = atoi(argv[1]);
	segCompInt = (char *) shmat(chave, 0, 0);
	
	signal(SIGUSR1, RecebePR);
	signal(SIGUSR2, RecebeRT);
	signal(SIGUSR3, RecebeRR);
	signal(SIGURG, FimExec);
	
	printf("[ESC s=%ld] Escalonador iniciado!\n", segAtual); fflush(stdout);
	
	inicio = time(NULL);
		
	while(processosPendentes || (numRT + numPR + numRR > 0)) {
		segAtual = ((unsigned long)time(NULL) - inicio) % 60;
		
		pol = ProximaPolitica(&i);
		Esc(pol, i);
	}
	
	printf("[ESC s=%ld] Todos os processos foram terminados!\n", segAtual); fflush(stdout);
	printf("[ESC s=%ld] Encerrando escalonador.\n", segAtual); fflush(stdout);
	
	shmdt(segCompInt);
	
	return 0;
}

int ChecaNumProc() {
	if (numRT + numPR + numRR == NUMPROCESSOS) {
		printf("[ESC s=%ld]: Numero maximo de processos atingido!\n", segAtual);
		fflush(stdout);
		return 1;
	}
	
	return 0;
}

void RecebeRT(int sinal) {
	Processo novo;
	int i, fim;
	int insere = 1;

	if (ChecaNumProc())
		return;
	
	novo.politica = REALTIME;
	sscanf(segCompInt, " %s %d %d", novo.nome, &novo.a1, &novo.a2);
	fim = novo.a1 + novo.a2;
	printf("[ESC s=%ld] Recebeu %s I = %d D = %d\n", segAtual,
	novo.nome, novo.a1, novo.a2); fflush(stdout);
	if (fim > 60) {
		printf("[ESC s=%ld] Tempo de execucao invalido!\n", segAtual);
		fflush(stdout);
		insere = 0;
	}
	else {
		for (i=0; i<numRT; i++) { //checando conflitos com outros processos
			if (novo.a1 >= RT[i].a1 && novo.a1 < RT[i].a1 + RT[i].a2) {
				printf("[ESC s=%ld] Conflito no inicio do intervalo de execucao!\n", segAtual);
				fflush(stdout);
				insere = 0;
				break;
			}
			else if (fim > RT[i].a1 && fim <= RT[i].a1 + RT[i].a2) {
				printf("[ESC s=%ld] Conflito no inicio do intervalo de execucao!\n", segAtual);
				fflush(stdout);
				insere = 0;
				break;
			}
		}
	}
	
	if (insere) {
		novo.pid = IniciaPrograma(novo.nome);
	
		//insere
		RT[numRT] = novo;
		numRT++;

		//ordena
		qsort(RT, numRT, sizeof(Processo), OrdenaRT);
		
		/*for (i=0; i<numRT; i++) {
			printf("[ESC s=%ld] RT[%d].inicio = %d RT[%d].fim = %d\n", segAtual,
			i, RT[i].a1, i, RT[i].a1+RT[i].a2); fflush(stdout);
		}*/
	}
}

void RecebePR(int sinal) {
	Processo novo;
	//int i;

	if (ChecaNumProc())
		return;
	
	novo.politica = PRIORIDADE;
	sscanf(segCompInt, " %s %d", novo.nome, &novo.a1);
	printf("[ESC s=%ld] Recebeu %s PR = %d\n", segAtual, novo.nome, novo.a1);
	fflush(stdout);
	if (novo.a1 < 1 || novo.a1 > 7) {
		printf("[ESC s=%ld] Prioridade Invalida!\n", segAtual);
		fflush(stdout);
	}
	else {
		novo.pid = IniciaPrograma(novo.nome);
	
		//insere
		PR[numPR] = novo;
		numPR++;
		
		//ordena
		qsort(PR, numPR, sizeof(Processo), OrdenaPR);
		/*for (i=0; i<numPR; i++) { //checando ordenacao dos processos
			printf("[ESC s=%ld] PR[%d].PR = %d\n", segAtual, i, PR[i].a1); fflush(stdout);
		}*/
	}
}

void RecebeRR (int sinal) {
	Processo novo;
	//int i;
	
	if (ChecaNumProc())
		return;
	
	novo.politica = PRIORIDADE;
	sscanf(segCompInt, " %s", novo.nome);
	printf("[ESC s=%ld] Recebeu %s\n", segAtual, novo.nome);
	fflush(stdout);
	
	novo.pid = IniciaPrograma(novo.nome);
	
	RR[numRR] = novo;
	numRR++;
	
	/*for (i=0; i<numRR; i++) { //checando ordenacao dos processos
		printf("[ESC s=%ld] RR[%d].nome = %s\n", segAtual, i, RR[i].nome); fflush(stdout);
	}*/
}

void FimExec(int sinal) {
	processosPendentes = 0;
}

int ProximaPolitica(int *idxProcesso) {
	int i;
	
	//se existem processos RT pro tempo atual, retorna REALTIME
	if (numRT != 0) {
		for (i = 0; i < numRT; i++) {
			if (segAtual >= RT[i].a1 && segAtual < RT[i].a1 + RT[i].a2) {
				*idxProcesso = i;
				return REALTIME;
			}
		}
	}
	//se existem processos PR, retorna PRIORIDADE
	if (numPR != 0) {
		return PRIORIDADE;
	}
	//senão, existem processos RR, retorna ROUNDROBIN
	else if (numRR != 0) {
		return ROUNDROBIN;
	}
	else return -1;
}

int OrdenaRT(const void *a, const void *b) {
	Processo *p1;
	Processo *p2;
	
	p1 = (Processo*)a;
	p2 = (Processo*)b;
	
	return (p1->a1 - p2->a1);
}

int OrdenaPR(const void *a, const void *b) {
	Processo *p1;
	Processo *p2;
	
	p1 = (Processo*)a;
	p2 = (Processo*)b;
	
	return (p1->a1 - p2->a1);
}

void Interrompe(Processo p) {
	printf("[ESC s=%ld] Parando %s\n", segAtual, p.nome); fflush(stdout);
	kill(p.pid, SIGSTOP);
}

void Continua(Processo p) {
	printf("[ESC s=%ld] Executando %s\n", segAtual, p.nome); fflush(stdout);
	kill(p.pid, SIGCONT);
}

void RealTime(int i) {
	int terminou;

	if (RT[i].pid != processoAtual.pid) {
		if(processoAtual.pid != -1)
			Interrompe(processoAtual);
		processoAtual = RT[i];
		Continua(processoAtual);
	}
	
	terminou = waitpid(processoAtual.pid, 0, WNOHANG);
	if (terminou > 0) {
		printf("[ESC s=%ld] %s Encerrou!\n", segAtual, processoAtual.nome); fflush(stdout);
		processoAtual.politica = processoAtual.pid = -1;
		RemoveRT(i);
	}
}

void Prioridade() {
	int terminou;
	
	//testando se o processo ainda não está sendo executado
	if ((processoAtual.politica != PRIORIDADE) ||
	(processoAtual.politica == PRIORIDADE && processoAtual.a1 != PR[0].a1)) {
		if (processoAtual.pid != -1)
			Interrompe(processoAtual);
		processoAtual = PR[0];
		Continua(processoAtual);
	}
	
	terminou = waitpid(processoAtual.pid, 0, WNOHANG);
	if (terminou > 0) {
		printf("[ESC s=%ld] %s Encerrou!\n", segAtual, processoAtual.nome); fflush(stdout);
		processoAtual.politica = processoAtual.pid = -1;
		RemovePR(0);
	}
}

void RoundRobin() {
	int terminou;
	
	if (processoAtual.pid != RR[0].pid) {
		if(processoAtual.pid != -1)
			Interrompe(processoAtual);
		processoAtual = RR[0];
		Continua(processoAtual);
		sleep(1);
	}
	
	terminou = waitpid(processoAtual.pid, 0, WNOHANG);
	if (terminou > 0) {
		printf("[ESC s=%ld] %s Encerrou!\n", segAtual, processoAtual.nome); fflush(stdout);
		processoAtual.politica = processoAtual.pid = -1;
		RemoveRR(0);
	}
	else {	//tira e poe no final
		RemoveRR(0);
		RR[numRR] = processoAtual;
		numRR++;
	}
}

void RemovePR (int idx) {
	int i;
	for (i = idx; i<numPR-1; i++) {
		PR[i] = PR[i+1];
	}
	numPR--;
}

void RemoveRT (int idx) {
	int i;
	for (i = idx; i<numRT-1; i++) {
		RT[i] = RT[i+1];
	}
	numRT--;
}

void RemoveRR (int idx) {
	int i;
	for (i = idx; i<numRR-1; i++) {
		RR[i] = RR[i+1];
	}
	numRR--;
}

void Esc(int pol, int i) {
	if (pol == REALTIME)
		RealTime(i);
	else if (pol == PRIORIDADE)
		Prioridade();
	else if (pol == ROUNDROBIN)
		RoundRobin();
}

int IniciaPrograma(char *path) {
	int pid;
	char prog[MAXNOME + 2] = "./";
	char *chamada[3] = {NULL, NULL, NULL};
	
	strcat(prog, path);
	chamada[0] = prog;
	
	if ((pid = fork()) < 0) { //cria processo que executara o programa
		printf("[ESC] Erro ao criar processo do programa %s!\n", path); fflush(stdout);
		exit(-1);
	}
	else if (pid == 0) { //novo programa
		execve(prog, chamada, 0);
		printf("[ESC] Erro ao iniciar o programa %s!\n", path); fflush(stdout);
		exit(-2);
	}
	
	kill(pid, SIGSTOP);
	
	return pid;
}
