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

#define MAX_NAME 50		//Tamanho maximo do name dos programas
#define MAX_PROCESSES 200	//Tamanho máximo das Filas 

//Sinais que possuem como comportamento padrão não fazerem nada, apenas para que possamos usar os handlers
#define SIGUSR3 SIGIO
#define SIGUSR4 SIGURG

//Politicas de escalonamento possiveis
typedef enum method {
	REALTIME, 
	PRIORITIES, 
	ROUNDROBIN
} Method;

//Struct de processos
typedef struct process {
	char name[MAX_NAME +1];
	int pid;
	Method method;
	int arg1, arg2;
} Process;

//Processo que estará sendo executado a cada momento. Seus campos são atualizados sempre que um processo novo é executado
Process processoAtual = {.name="", .pid=-1, .method=-1, .arg1=-1, .arg2=-1};

char *segmentoInterpretador;


//contadores
int countRealTimeProc = 0;
int countPrioritiesProc = 0;
int countRoundRobinProc = 0;

//Indica se ainda existem processos para serem inseridos nas filas
int hasProcessRemaining = 1;

//Filas para cada uma das políticas de escalonamento
Process realTime[MAX_PROCESSES];
Process priorities[MAX_PROCESSES];
Process roundRobin[MAX_PROCESSES];

//Variaveis de manutençao de tempo
unsigned long start = 0;
unsigned long now = 0;

//Handlers dos sinais levantados pelo interpretador
void insertProcess_Priorities(int sinal); // Insere processo recebido na lista de Prioridades
void insertProcess_RealTime(int sinal); // Insere processo recebido na lista de Real Time
void insertProcess_RoundRobin(int sinal); // Insere processo recebido na lista de Round Robin
void endFile(int sinal); // É executado quando a leitura do arquivo termina

//Declaração das funcões auxiliares
int nextMethod(int *process_index);
int sort_RealTime(const void *a, const void *b);
int sort_Priorities(const void *a, const void *b);
int startProgram(char *path);
void methodPriority(int process_method, int i);
void stopProcess(Process p);
void continueProcess(Process p);
void RealTime(int i);
void Priority();
void RoundRobin();
void remove_RealTime(int index);
void remove_Priorities(int index);
void remove_RoundRobin(int index);

int main (int argc, char *argv[]) {
	int key;
	int process_method = -1;
	int i = -1;
	
	//Chave do segmento de memória é passada como string para escalonador e deve ser transformada para inteiro
	key = atoi(argv[1]);

	//Attach do segmento de memória
	segmentoInterpretador = (char *) shmat(key, 0, 0);
	
	//Sinais utilizados e seus handlers
	signal(SIGUSR1, insertProcess_Priorities);
	signal(SIGUSR2, insertProcess_RealTime);
	signal(SIGUSR3, insertProcess_RoundRobin);
	signal(SIGUSR4, endFile);
	
	printf("Iniciando Escalonador!\n");

	//Inicializa contador de tempo
	start = time(NULL);
		
	while(hasProcessRemaining || (countRealTimeProc + countPrioritiesProc + countRoundRobinProc > 0)) {
		
		//Atualiza o tempo atual a cada iteração		
		now = ((unsigned long)time(NULL) - start) % 60;
		
		//Guarda qual será a proxima politica de escalonamento utilizada de acordo com a ordem de prioridade de politicas (REALTIME >>> PRIORIDADES >>> ROUNDROBIN)
		process_method = nextMethod(&i);
		
		//Realiza escalonamento de acordo a política recebida acima
		if (process_method == REALTIME)
			RealTime(i);
		else if (process_method == PRIORITIES)
			Priority();
		else if (process_method == ROUNDROBIN)
			RoundRobin();		
		
	}

	//Todos os processos foram executados até o fim
	printf("Todos os Processos foram finalizados. Encerrando Escalonador!\n");
	
	//Detach do segmento de memória compartilhada
	shmdt(segmentoInterpretador);
	
	return 0;
}

//Insere os processos Real Time recebidos do interpretador na Fila Real Time
void insertProcess_RealTime(int sinal) {
	Process new;
	int i, end;
	int insert_quantity = 1; // Caso nao possa inserir, este valor é modificado para 0
	
	//Verifica se a fila já está cheia
	if (countRealTimeProc + countPrioritiesProc + countRoundRobinProc == MAX_PROCESSES) {
		printf("A Fila de Real Time esta cheia, nao eh possivel inserir mais processos!\n");		
		return;
	}
	
	new.method = REALTIME;
	
	//Passa o path e os argumentos para o novo processo criado
	sscanf(segmentoInterpretador, " %s %d %d", new.name, &new.arg1, &new.arg2);

	//Tempo de término do processo = Inicio + Duração
	end = new.arg1 + new.arg2;

	printf("Recebeu %s I = %d D = %d\n", new.name, new.arg1, new.arg2);	
	
	//Verifica validade dos argumentos
	//Se > 60, causará conflito com certeza
	if (end > 60) {
		printf("Tempo de execucao causara conflito com outros processos\n");
		insert_quantity = 0;
	}
	else {
		for (i=0; i<countRealTimeProc; i++) { 
			//Verificando conflitos com outros processos Real Time de acordo com tempo de inicio
			if (new.arg1 >= realTime[i].arg1 && new.arg1 < realTime[i].arg1 + realTime[i].arg2) {
				printf("A execucao do Processo %s entra em conflito com outro processo\n", new.name);	
				insert_quantity = 0;
				break;
			}//Verificando conflitos com outros processos Real Time de acordo com tempo de duração
			else if (end > realTime[i].arg1 && end <= realTime[i].arg1 + realTime[i].arg2) {
				printf("A execucao do Processo %s entra em conflito com outro processo\n", new.name);			
				insert_quantity = 0;
				break;
			}
		}
	}
	
	//Caso haja conflito detectado, não insere o processo
	if (!insert_quantity)
		return;

	//Executa o processo
	new.pid = startProgram(new.name);
	
	//Insere na Fila
	realTime[countRealTimeProc] = new;
	countRealTimeProc++;

	//Ordena a Fila de Real Time
	qsort(realTime, countRealTimeProc, sizeof(Process), sort_RealTime);
}


//Insere os processos Prioridades recebidos do interpretador na Fila de Prioridades
void insertProcess_Priorities(int sinal) {
	Process new;

	//Verifica se Fila já está cheia
	if (countRealTimeProc + countPrioritiesProc + countRoundRobinProc == MAX_PROCESSES) {
		printf("A Fila de Prioridades esta cheia, nao eh possivel inserir mais processos!\n");		
		return;
	}
	
	new.method = PRIORITIES;

	//Passa o path e argumento para a estrutura Processo criada
	sscanf(segmentoInterpretador, " %s %d", new.name, &new.arg1);
	
	printf("Recebeu %s com Prioridade = %d\n",  new.name, new.arg1);
	
	//Se a Prioridade lida do arquivo for menor que 0 ou maior que 7, não é uma prioridade válida
	if (new.arg1 < 0 || new.arg1 > 7) {
		printf("Valor de Prioridade Invalida!\n");
	}
	else {
		//Executa Processo
		new.pid = startProgram(new.name);
	
		//Insere processo na fila
		priorities[countPrioritiesProc] = new;
		countPrioritiesProc++;
		
		//Ordena a fila de acordo com as prioridades dos Processos
		qsort(priorities, countPrioritiesProc, sizeof(Process), sort_Priorities);
	}
}	

//Insere os processos Round Robin recebidos do interpretador na Fila Round Robin
void insertProcess_RoundRobin (int sinal) {
	Process new;
	
	//Verifica se a Fila já está cheia
	if (countRealTimeProc + countPrioritiesProc + countRoundRobinProc == MAX_PROCESSES) {
		printf("A Fila de Round Robin esta cheia, nao eh possivel inserir mais processos!\n");		
		return;
	}
	
	new.method = ROUNDROBIN;
	
	//Guarda path e argumento na estrutura Processo criada
	sscanf(segmentoInterpretador, " %s", new.name);
	printf(" Recebeu %s\n",  new.name);
	
	//Executa Processo
	new.pid = startProgram(new.name);
	
	//Insere na Fila RoundRobin
	roundRobin[countRoundRobinProc] = new;
	countRoundRobinProc++;

}

//Handler : Quando interpretador termina de ler arquivo, atualiza hasProcessRemaining para 0
void endFile(int sinal) {
	hasProcessRemaining = 0;
}

//Retorna a próxima política que deve ser escalonada de acordo com a prioridade das políticas (REAL TIME >>> PRIORIDADES >>> ROUND ROBIN)
//Recebe indice do processo Real Time, se tiver
int nextMethod(int *process_index) {
	int i;
	
	//Se existe algum processo Real Time para ser executado no tempo atual, retorna REALTIME
	if (countRealTimeProc != 0) {
		for (i = 0; i < countRealTimeProc; i++) {
			if (now < realTime[i].arg1 || now >= realTime[i].arg1 + realTime[i].arg2)
				continue;

			*process_index = i;
			return REALTIME;
		}
	}
	//Se existe algum processo na Fila de Prioridades, retorna PRIORITIES
	if (countPrioritiesProc != 0) {
		return PRIORITIES;
	}
	//Senão, se existem processos na Fila de Round Robin, retorna ROUNDROBIN
	else if (countRoundRobinProc != 0) {
		return ROUNDROBIN;
	}
	else return -1;
}

//Inicia Execução dos Programas
int startProgram(char *path) {
	int pid;
	char prog[MAX_NAME + 2] = "./"; // Comando para executar Programas no terminal
	char *command[3] = {NULL, NULL, NULL};
	
	strcat(prog, path); //Forma string no formato "./nomedoprograma"
	command[0] = prog; //command = {./nomedoprograma,NULL,NULL}
	
	//Criando Processo que irá executar o Programa
	if ((pid = fork()) < 0) { 
		printf("Erro ao tentar criar processo do programa - Modulo escalonador %s!\n", path); fflush(stdout);
		exit(-1);
	}
	else if (pid == 0) { 
		execve(prog, command, 0); //Executa o Programa
		printf("Erro ao tentar iniciar o programa %s!\n", path); fflush(stdout); //Caso execve falhe
		exit(-2);
	}
	
	//Interrompe execução do Processo
	kill(pid, SIGSTOP);
	
	return pid;
}

//Ordena Processos Real Time pelo tempo de inicio
int sort_RealTime(const void *a, const void *b) {
	Process *p1;
	Process *p2;
	
	p1 = (Process*)a;
	p2 = (Process*)b;
	
	return (p1->arg1 - p2->arg1);
}

//Ordena Processos de Prioridade pelo valor da prioridade ( 0 -> 7 )
int sort_Priorities(const void *a, const void *b) {
	Process *p1;
	Process *p2;
	
	p1 = (Process*)a;
	p2 = (Process*)b;
	
	return (p1->arg1 - p2->arg1);
}

//Interrompe Processo passado por parâmetro
void stopProcess(Process p) {
	printf(" Interrompendo %s\n",  p.name);
	kill(p.pid, SIGSTOP);
}

//Retoma Processo passado por parâmetro
void continueProcess(Process p) {
	printf(" Executando %s\n",  p.name);
	kill(p.pid, SIGCONT);
}

//Escalonador Real Time
void RealTime(int i) {
	int finished;
	
	//Caso o Processo sendo executado no momento não seja o passado por parâmetro, interrompe a execução do processo atual
	//Executa processo de índice passado por parametro
	if (realTime[i].pid != processoAtual.pid) {
		if(processoAtual.pid != -1)
			stopProcess(processoAtual);
		processoAtual = realTime[i];
		continueProcess(processoAtual);
	}
	
	//Utiliza opção WNOHANG para que pai não espere término do filho
	//Verifica se filho terminou e retorna valor para finished
	finished = waitpid(processoAtual.pid, 0, WNOHANG);
	
	//Se terminou, atualiza pid do processo e o remove da Fila
	if (finished <= 0)
		return;

	printf(" %s Encerrou!\n",  processoAtual.name);
	processoAtual.method = processoAtual.pid = -1;
	remove_RealTime(i);
}


//Escalonador Prioridades
void Priority() {
	int finished;
	
	//Verifica se o Processo sendo executado é Prioridades ou se é o Processo que está para ser executado
	if ((processoAtual.method != PRIORITIES) || (processoAtual.method == PRIORITIES && processoAtual.arg1 != priorities[0].arg1)) {
		//Verifica se processo sendo executado terminou
		if (processoAtual.pid != -1)
			//Se não terminou, interrompe o Processo
			stopProcess(processoAtual);
		processoAtual = priorities[0]; //Torna Processo Prioridade no processo atual
		continueProcess(processoAtual); //Executa o Processo
	}
	
	//Utiliza opção WNOHANG para que pai não espere término do filho
	//Verifica se filho terminou e retorna valor para finished
	finished = waitpid(processoAtual.pid, 0, WNOHANG);
	
	//Se terminou, atualiza pid do processo e o remove da Fila
	if (finished <= 0)
		return;

	printf(" %s Encerrou!\n",  processoAtual.name);
	processoAtual.method = processoAtual.pid = -1;
	remove_Priorities(0);
}


//Escalonador Round Robin
void RoundRobin() {
	int trigger = 50;
	float msec = 0;
	int finished;
	int i = 0;
	
	//Verifica se o Processo sendo executado é Round Robin
	if (processoAtual.pid != roundRobin[0].pid) {
		if(processoAtual.pid != -1) //Verifica se o Processo atual terminou
			stopProcess(processoAtual); // Interrompe Processo atual	
		processoAtual = roundRobin[0]; // Torna o Processo Round Robin o Processo Atual
	
		//Inicia clock para calcular o quantum
		clock_t before = clock();
		
		do{	
			//Inicia a execução do Processo na primeira iteração	
			if(i == 0){
				continueProcess(processoAtual);
			}			
			clock_t difference = clock() - before;
			msec = difference * 1000 / CLOCKS_PER_SEC; // Calcula tempo passado
			i++;	
		}while(msec < trigger);	// Executa enquanto o Quantum não é atingido
		
		printf("QUANTUM atingido = %.1f segundos\n", msec/100);
	}

	//Utiliza opção WNOHANG para que pai não espere término do filho
	//Verifica se filho terminou e retorna valor para finished
	finished = waitpid(processoAtual.pid, 0, WNOHANG);

	//Se terminou, atualiza pid do Processo e o remove da Fila
	if (finished > 0) {
		printf(" %s Encerrou!\n",  processoAtual.name); fflush(stdout);
		processoAtual.method = processoAtual.pid = -1;
		remove_RoundRobin(0);
	}//Senão, remove da Fila e o reinsere no final da Fila
	else {
		remove_RoundRobin(0);
		roundRobin[countRoundRobinProc] = processoAtual;
		countRoundRobinProc++;
	}
}

//Remove Processo da Fila de Real Time
void remove_RealTime (int index) {
	int i;
	for (i = index; i<countRealTimeProc-1; i++) {
		realTime[i] = realTime[i+1];
	}
	countRealTimeProc--;
}

//Remove Processo da Fila de Prioridades
void remove_Priorities (int index) {
	int i;
	for (i = index; i<countPrioritiesProc-1; i++) {
		priorities[i] = priorities[i+1];
	}
	countPrioritiesProc--;
}

//Remove Processo da fila de RoundRobin
void remove_RoundRobin (int index) {
	int i;
	for (i = index; i<countRoundRobinProc-1; i++) {
		roundRobin[i] = roundRobin[i+1];
	}
	countRoundRobinProc--;
}


