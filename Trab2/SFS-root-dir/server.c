#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/dir.h>
#include <stddef.h>
#include <fcntl.h>
#include <fts.h>

#include <unistd.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER  250
#define BUFSIZE 1024


char* getFileInformation(char* path);
char* read_file(char* path, int *nrbytes, int offset);
int write_file(char* path, char* payload, int nrbytes, int offset, char* client, char* ownerPerm, char* otherPerm);
char* list_directories(char* path);
char* create_directory(char* path, char* name, char* client, char* ownerPerm, char* otherPerm);
char* remove_directory(char* path, char* name, char* client);
char* getDirectory();
int filter_files(const struct dirent* nameList);
void error(char *msg);
int delete_everything(const char *dir);
int check_file (char* filename);

char* runCommand(char* command)
{
	char* msgResposta[250];
	char* params[10];

	int qtd_par = 0;
	for(int i = 0; (params[i] = strsep(&command, " ")) != NULL; i++, qtd_par++)
	{
		if(i == 11)
			return "Erro: Muitos parâmetros!\n";
	}


    printf("Parâmetro %d\n", qtd_par);
	//Lê arquivo
	if(!strcmp(params[0], "RD-REQ"))
	{
		if(qtd_par != 5)
			return "Erro: Quantidade de parâmetros inválida!\n";

		char* path = params[1];
		int nrbytes = atoi(params[3]);
		int offset = atoi(params[4]);
		char* payload;
		char bytes[20];
		char* pathComplete = (char*)malloc(BUFFER * sizeof(char));
		char pathSize[20];

		pathComplete[0] = '\0';
		
		payload = read_file(path, &nrbytes, offset);
		printf("Payload: %s\n", payload);

		if (payload == NULL)
		{
			printf("Erro: Não foi possível carregar o arquivo!\n");
			return NULL;
		}

		snprintf(pathSize, 20, "%lu", strlen(path));
		snprintf(bytes, 20, "%d", nrbytes);

		strcpy(pathComplete, "RD-REP ");
		strcat(pathComplete, path);
		strcat(pathComplete, " ");
		strcat(pathComplete, pathSize);
		strcat(pathComplete, " ");
		strcat(pathComplete, payload);
		strcat(pathComplete, " ");
		strcat(pathComplete, bytes);
		strcat(pathComplete, " ");
		strcat(pathComplete, params[4]);

		printf("Resposta: %s\n", pathComplete);

		return pathComplete;
	}
	//Escreve mensagem no arquivo
	if(!strcmp(params[0], "WR-REQ"))
	{
		if(qtd_par != 9)
			return "Erro: Quantidade de parâmetros inválida!\n";

		char* path = params[1];
		char* payload = params[3];
		int nrbytes = atoi(params[4]);
		int offset = atoi(params[5]);
		char* client = params[6];
		char* owner = params[7];
		char* others = params[8];

		char* pathComplete = (char*)malloc(BUFFER * sizeof(char));
		char pathSize[BUFFER];
		char bytes[BUFFER];

		pathComplete[0] = '\0';
		
		nrbytes = write_file(path, payload, nrbytes, offset, client, owner, others);
		printf("Quantidade de bytes escritos: %d\n", nrbytes);

		if (nrbytes == -1)
		{
			printf("Erro ao tentar escrever no arquivo!\n");
			return "Erro ao tentar escrever no arquivo!\n";
		}
		if(nrbytes == -3) {
			printf("Erro: Acesso negado!\n");
			return "Erro: Acesso negado!\n";
		}

		snprintf(pathSize, 20, "%lu", strlen(path));
		snprintf(bytes, 20, "%d", nrbytes);

		strcpy(pathComplete, "WR-REP ");
		strcat(pathComplete, path);
		strcat(pathComplete, " ");
		strcat(pathComplete, pathSize);
		strcat(pathComplete, " ");
		strcat(pathComplete, bytes);
		strcat(pathComplete, " ");
		strcat(pathComplete, params[4]);

		printf("Resposta: %s\n", pathComplete);

		return pathComplete;
	}
	//Retorna informações do arquivo
	if(!strcmp(params[0], "FI-REQ"))
	{
		char* path = params[1];

		char* rep = (char*)malloc(BUFFER * sizeof(char));

		strcpy(rep, "FI-REP ");
		strcat(rep, path);
		strcat(rep, " ");

		char* temp = getFileInformation(path);
		if(temp == NULL) {
			return "Erro: Não foi possível acessar as informações do arquivo!\n";
		}
		strcat(rep, temp);
		
		return rep;
	}
	//Cria novo subdiretório
	if(!strcmp(params[0], "DC-REQ"))
	{
		if(qtd_par != 7)
			return "Erro: Quantidade de parâmetros inválida!\n";

		char* path = params[1];
		char* name = params[3];
		char* client = params[4];
		char* owner = params[5];
		char* others = params[6];

		char* pathComplete = (char*)malloc(BUFFER * sizeof(char));
		char* answer;
		char  len[20];

		pathComplete[0] = '\0';
		
		answer = create_directory(path, name, client, owner, others);

		if(answer == NULL) {
			printf("Erro: Não foi possível criar o diretório!\n");
			return "Erro: Não foi possível criar o diretório!\n";
		}

		snprintf(len, 20, "%lu", strlen(answer));

		strcpy(pathComplete, "DC-REP ");
		strcat(pathComplete, answer);
		strcat(pathComplete, " ");
		strcat(pathComplete, len);
		
		return pathComplete;
	}
	//Remove subdiretório
	if(!strcmp(params[0], "DR-REQ"))
	{
		if(qtd_par != 6)
			return "Erro: Quantidade de parâmetros inválida!\n";

		char* path   = params[1];
		char* name   = params[3];
		char* client = params[5];

		char* pathComplete = (char*)malloc(BUFFER * sizeof(char));
		char* answer;
		char  len[20];

		pathComplete[0] = '\0';

		answer = remove_directory(path, name, client);

		printf("Resposta: %s\n", answer);

		if (answer == NULL)
		{
			snprintf(len, 20, "%d", 0);
		}
		else
		{
			snprintf(len, 20, "%lu", strlen(path));
		}

		printf("Tamanho do caminho do diretório: %s\n", len);
		
		strcpy(pathComplete, "DR-REP ");
		strcat(pathComplete, path);
		strcat(pathComplete, " ");
		strcat(pathComplete, len);

		return pathComplete;
	}
 	//Retorna nome dos subdiretórios
	if(!strcmp(params[0], "DL-REQ"))
	{
		char* path = params[1];

		char* rep = (char*)malloc(BUFFER * sizeof(char));

		strcpy(rep, "DL-REP ");
		strcat(rep, "\n");
		strcat(rep, list_directories(path));
		
		return rep;
	}
	
	printf("Comando não reconhecido!\n");
	return NULL;
}

char* read_file(char* path, int* nrbytes, int offset)
{
	char *payload = (char*)malloc(BUFSIZE * sizeof(char));
	int descriptor;
	int bytes;

	printf("read_file -- path: %s, nrbytes: %d, offset: %d\n", path, *nrbytes, offset);

	descriptor = open(path, O_RDONLY);

	bytes = pread(descriptor, payload, *nrbytes, offset);

	printf("Bytes: %d\n", bytes);

	close(descriptor);

	*nrbytes = bytes;

	if(bytes == 0) {
		return strdup("NULL");
	}

	if(bytes == -1)
	{
		return NULL;
	}
	
	return payload;
}

int write_file(char* path, char* payload, int nrbytes, int offset, char* client, char* ownerPerm, char* otherPerm)
{
	struct stat buf;
	int descriptor;
	int written;
	int size;

	//Client
	char* pathdup = strdup(path);
	char execPath[BUFFER];
	char* execName;
	char* name;
	char* aux;
	char* fBuffer = (char*)malloc(BUFSIZE * sizeof(char));
	int clientDescriptor;
	int rw;

	char dirPath[40]; dirPath[0] = '\0';


	while((aux = strsep(&pathdup, "/")) != NULL) name = aux;
	execName = (char*)malloc((strlen(name)+2)*sizeof(char));
	strcpy(execName, ".");
	strcat(execName, name);
	strcpy(execPath, path);
	execPath[strlen(execPath) - strlen(name)] = '\0';

	strcpy(dirPath, execPath);
	strcat(dirPath, ".directory");

	strcat(execPath, execName);

	strcpy(fBuffer, client);
	strcat(fBuffer, " ");
	strcat(fBuffer, ownerPerm);
	strcat(fBuffer, " ");
	strcat(fBuffer, otherPerm);

	printf("path: %s / execPath: %s / name: %s / dirPath: %s\n ", path, execPath, name, dirPath);

	char* dBuffer = (char*)malloc(BUFSIZE * sizeof(char));

	int dirDescriptor = open(dirPath, O_RDONLY);
	rw = pread(dirDescriptor, dBuffer, 10, 0);
	if(rw > 0) {
		printf("Lendo arquivo de auth: %d / valor: %s\n", rw, dBuffer);

		char* params[3];

		for(int i = 0; (params[i] = strsep(&dBuffer, " ")) != NULL; i++);

		if (params[2][0] == 'R') {
			if(strcmp(params[0], client) != 0) {
				return -3;
			}
		}

	}
	close(dirDescriptor);
	
	if (!check_file(path))
	{
		descriptor = open(path, O_WRONLY | O_CREAT, 0666);

		clientDescriptor = open(execPath, O_RDWR | O_CREAT, 0666);
		rw = pwrite(clientDescriptor, fBuffer, strlen(fBuffer), 0);
		close(clientDescriptor);

		printf("Escrevendo em arquivo: %d\n", rw);
		free(fBuffer);
	}
	else
	{
		char* fileBufAux = (char*)malloc(BUFSIZE * sizeof(char));

		descriptor = open(path, O_WRONLY);
		clientDescriptor = open(execPath, O_RDONLY);
		rw = pread(clientDescriptor, fileBufAux, 2*strlen(fBuffer), 0);
		printf("Lendo arquivo de auth: %d / valor: %s\n", rw, fileBufAux);

		close(clientDescriptor);

		char* params[3];
		for(int i = 0; (params[i] = strsep(&fileBufAux, " ")) != NULL; i++);

		if (params[2][0] == 'R') {
			if(strcmp(params[0], client) != 0) {
				close(descriptor);
				return -3;
			}
		}
	}

	if (nrbytes == 0)
	{
		if (remove(path) == -1)
		{
			printf("Error deleting file: %s\n", path);
		}
		remove(execPath);

		return 0;
	}

	stat(path, &buf);
	size = buf.st_size;

	offset = size < offset ? size : offset;

	printf("write_file -- path: %s, payload: %s, nrbytes: %d, offset: %d\n", path, payload, nrbytes, offset);

	written = pwrite(descriptor, payload, nrbytes, offset);

	close(descriptor);

	printf("Bytes written: %d\n", written);
	
	return written;
}

//Retorna as informações do arquivo pedido
char* getFileInformation(char* path)
{
	//Client
	char* pathdup = strdup(path);
	char execPath[BUFFER];
	char* execName;
	char* name;
	char* aux;
	int clientDescriptor;
	int rw;
	//int descriptor;

	while((aux = strsep(&pathdup, "/")) != NULL) name = aux;
	execName = (char*)malloc((strlen(name)+2)*sizeof(char));
	strcpy(execName, ".");
	strcat(execName, name);
	strcpy(execPath, path);
	execPath[strlen(execPath) - strlen(name)] = '\0';
	strcat(execPath, execName);

	char* fileBufAux = (char*)malloc(BUFSIZE * sizeof(char));

	printf("%d\n", strcmp(execPath, "./newDir/.teste.txt"));

	if(execPath[strlen(execPath)] == '\n') {
		printf("bananas\n"); //Debug, apagar depois
	}
	

	clientDescriptor = open(execPath, O_RDONLY);
	rw = pread(clientDescriptor, fileBufAux, 10, 0);
	close(clientDescriptor);

	if(rw == -1) {
		return NULL;
	}

	char* ret = strsep(&fileBufAux, "\n");

	struct stat st;
	stat(path, &st);

	char sz[20];
	snprintf(sz, 19, "%ld", st.st_size);
	
	strcat(ret, " ");
	strcat(ret, sz);
	
	close(clientDescriptor);

	printf("Lendo arquivo de auth: %d / valor: %s\n", rw, ret);

	return ret;
}

//Cria um subdiretório no diretório corrente
char* create_directory(char* path, char* name, char* client, char* ownerPerm, char* otherPerm)
{
	struct stat st = {0};
	char* pathComplete = (char*)malloc((strlen(path) + strlen(name) + 1) * sizeof(char));
	char* authPath;
	char* fBuffer = (char*)malloc(BUFSIZE * sizeof(char));
	mode_t permissao = S_IRWXU | S_IROTH | S_IWOTH | S_IXOTH;
	int descriptor;

	printf("create_directory -- path: %s, name: %s, pathComplete: %s\n", path, name, pathComplete);

	strcpy(pathComplete, path);
	strcat(pathComplete, "/");
	strcat(pathComplete, name);

	authPath = strdup(pathComplete);
	strcat(authPath, "/.directory");

	if (stat(pathComplete, &st) == -1) 
	{
		if (mkdir(pathComplete, permissao) != 0)
		{
			printf("Erro ao tentar criar diretório!\n");
			return NULL;
		}
	}
	else 
	{
		printf("Erro ao receber stats!\n");
		return NULL;
	}

	strcpy(fBuffer, client);
	strcat(fBuffer, " ");
	strcat(fBuffer, ownerPerm);
	strcat(fBuffer, " ");
	strcat(fBuffer, otherPerm);

	printf("Buffer: %s\n", fBuffer);

	descriptor = open(authPath, O_WRONLY | O_CREAT, 0666);
	pwrite(descriptor, fBuffer, strlen(fBuffer), 0);
	close(descriptor);

	return pathComplete;
}

//Remove um diretório
char* remove_directory(char* path, char* name, char* client)
{
	char* pathComplete = (char*)malloc((strlen(path) + strlen(name) + 1) * sizeof(char));

	//Client
	char* buf = (char*)malloc(BUFSIZE*sizeof(char));
	char* auth = (char*)malloc(BUFSIZE*sizeof(char));
	int clientDescriptor;
	int rw;

	strcpy(buf, path);
	strcat(buf, "/");
	strcat(buf, name);
	strcat(buf, "/");
	strcat(buf, ".directory");

	printf("buf: %s\n", buf);

	clientDescriptor = open(buf, O_RDONLY);
	rw = pread(clientDescriptor, auth, strlen(buf), 0);
	close(clientDescriptor);
	printf("Lendo arquivo de auth: %d / valor: %s\n", rw, auth);

	char* params[4];
	for(int i = 0; (params[i] = strsep(&auth, " ")) != NULL; i++);

	printf("client: %s\n", params[0]);

	if(strcmp(params[0], client) != 0) {
		free(buf);
		free(auth);
		return NULL;
	}
	free(buf);
	free(auth);
	//

	printf("Removendo diretório -- path: %s, nome: %s\n", path, name);

	strcpy(pathComplete, path);
	strcat(pathComplete, "/");
	strcat(pathComplete, name);

	printf("Path: %s\n", pathComplete);

	if (delete_everything(pathComplete) != -1)
		return pathComplete;
	
	printf("Erro ao tentar remover diretório!\n");
	return NULL;
}

//Lista os nomes dos diretórios a partir do diretório corrente
char* list_directories(char* path)
{
	char* pathComplete = getDirectory();
	int count;
	struct dirent** nameList;
	char* ret = malloc(BUFSIZE*(sizeof(char))); ret[0] = '\0';

	printf("list_directories -- path: %s\n", path);

	if(path[1] == '/')
		strcat(pathComplete, &path[1]);

	printf("%s\n", pathComplete);

	count = scandir(pathComplete, &nameList, filter_files, alphasort);
	//quantidade de arquivos no diretorio

	printf("%d\n", count);


	for(int i = 0; i < count; i ++) {
		printf("%s\n", nameList[i]->d_name);
		if (nameList[i]->d_type == DT_DIR) {
			strcat(ret, "    ");
		}
		strcat(ret, nameList[i]->d_name);
		strcat(ret, "\n");
	}
	return ret;
}

//Retorna o diretório corrente
char* getDirectory()
{
	char* currentDir = (char*)malloc(BUFFER*sizeof(char)) ;

	if (getcwd(currentDir, BUFFER) != NULL)
		return currentDir;

	printf("Error getting path\n"); 
	exit(0);
}

int filter_files(const struct dirent* nameList)
{
	if ((strcmp(nameList->d_name, ".") == 0) || (strcmp(nameList->d_name, "..") == 0) || (nameList->d_name[0] == '.'))  
		return 0; 
	else
		return 1;
}

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int check_file (char* filename)
{
  struct stat buffer;   
  return stat(filename, &buffer) == 0;
}

int delete_everything(const char *dir)
{
    int ret = 0;
    FTS *ftsp = NULL;
    FTSENT *curr;
    char *files[] = { (char *) dir, NULL };

    ftsp = fts_open(files, FTS_NOCHDIR | FTS_PHYSICAL | FTS_XDEV, NULL);
    if (!ftsp) {
        ret = -1;
        goto finish;
    }

    while ((curr = fts_read(ftsp))) {
        switch (curr->fts_info) {
        case FTS_NS:
        case FTS_DNR:
        case FTS_ERR:
            break;

        case FTS_DC:
        case FTS_DOT:
        case FTS_NSOK:
            break;

        case FTS_D:
            break;

        case FTS_DP:
        case FTS_F:
        case FTS_SL:
        case FTS_SLNONE:
        case FTS_DEFAULT:
            if (remove(curr->fts_accpath) < 0) {
                ret = -1;
            }
            break;
        }
    }

    finish:
    if (ftsp) {
        fts_close(ftsp);
    }

    return ret;
}

void server_execute(int port)
{
    int sockfd; /* socket */
    int portno; /* port to listen on */
    unsigned int clientlen; /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    struct hostent *hostp; /* client host info */
    char buf[BUFSIZE]; /* message buf */
    char *hostaddrp; /* dotted decimal host addr string */
    int optval; /* flag value for setsockopt */
    int n; /* message byte size */
    
    portno = port;
    
    /* 
     * socket: create the parent socket 
     */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets 
     * us rerun the server immediately after we kill it; 
     * otherwise we have to wait about 20 secs. 
     * Eliminates "ERROR on binding: Address already in use" error. 
     */
    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

    /*
     * build the server's Internet address
     */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)portno);

    /* 
     * bind: associate the parent socket with a port 
     */
    if (bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) 
        error("ERROR on binding");

    /* 
     * main loop: wait for a datagram, then echo it
     */
    clientlen = sizeof(clientaddr);
    while (1) 
    {
        /*
         * recvfrom: receive a UDP datagram from a client
         */
        bzero(buf, BUFSIZE);
        n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
        if (n < 0)
            error("ERROR in recvfrom");
        
        /* 
         * gethostbyaddr: determine who sent the datagram
         */
        hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, sizeof(clientaddr.sin_addr.s_addr), AF_INET);
        if (hostp == NULL)
            error("ERROR on gethostbyaddr");

        hostaddrp = inet_ntoa(clientaddr.sin_addr);
        if (hostaddrp == NULL)
            error("ERROR on inet_ntoa\n");
    
        printf("Server received datagram from %s (%s)\n",  hostp->h_name, hostaddrp);
        printf("Server received %lu/%d bytes: %s\n", strlen(buf), n, buf);
        
        if(buf[0] == '#')
        	continue;

        char* reply;
        if( !(reply = runCommand(strdup(buf)))) {
            reply = strdup("Error: Comando não reconhecido!\n");
        }
        printf("Reply: %s\n", reply);
        
        /* 
         * sendto: echo the input back to the client 
         */
        n = sendto(sockfd, reply, strlen(reply) + 1, 0, (struct sockaddr *) &clientaddr, clientlen);
        if (n < 0) 
            error("ERROR in sendto");
        printf("Resposta enviada!\n");
    }
}

int main(int argc, char **argv)
{
    /* 
     * check command line arguments 
     */
    if (argc != 2) 
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    int portno = atoi(argv[1]);

    server_execute(portno);

    return 0;
}
