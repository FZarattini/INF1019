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
	char* params[10];
	char* msgResposta[200];

	int param_num = 0;
	for(int i = 0; (params[i] = strsep(&command, " ")) != NULL; i++, param_num++)
	{
		if(i == 11)
			
			return "Erro: Muitos parâmetros!\n";
	}


    printf("Parâmetro %d\n", param_num);
	if(!strcmp(params[0], "RD-REQ"))
	{
		if(param_num != 5)
			return "Erro: Quantidade de parâmetros inválida!\n";

		char* path = params[1];
		char* payload;
		int nrbytes = atoi(params[3]);
		int offset = atoi(params[4]);

		char* fullpath = (char*)malloc(BUFFER * sizeof(char));
		char lenPath[20];
		char bt[20];

		fullpath[0] = '\0';
		
		payload = read_file(path, &nrbytes, offset);
		printf("Payload: %s\n", payload);

		if (payload == NULL)
		{
			printf("Erro: Não foi possível carregar o arquivo!\n");
			return NULL;
		}

		snprintf(lenPath, 20, "%lu", strlen(path));
		snprintf(bt, 20, "%d", nrbytes);

		strcpy(fullpath, "RD-REP ");
		strcat(fullpath, path);
		strcat(fullpath, " ");
		strcat(fullpath, lenPath);
		strcat(fullpath, " ");
		strcat(fullpath, payload);
		strcat(fullpath, " ");
		strcat(fullpath, bt);
		strcat(fullpath, " ");
		strcat(fullpath, params[4]);

		printf("Resposta: %s\n", fullpath);

		return fullpath;
	}
	
	if(!strcmp(params[0], "WR-REQ"))
	{
		if(param_num != 9)
			return "Erro: Quantidade de parâmetros inválida!\n";

		char* path = params[1];
		char* payload = params[3];
		int nrbytes = atoi(params[4]);
		int offset = atoi(params[5]);
		char* client = params[6];
		char* owner = params[7];
		char* others = params[8];

		char* fullpath = (char*)malloc(BUFFER * sizeof(char));
		char lenPath[BUFFER];
		char bt[BUFFER];

		fullpath[0] = '\0';
		
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

		snprintf(lenPath, 20, "%lu", strlen(path));
		snprintf(bt, 20, "%d", nrbytes);

		strcpy(fullpath, "WR-REP ");
		strcat(fullpath, path);
		strcat(fullpath, " ");
		strcat(fullpath, lenPath);
		strcat(fullpath, " ");
		strcat(fullpath, bt);
		strcat(fullpath, " ");
		strcat(fullpath, params[4]);

		printf("Resposta: %s\n", fullpath);

		return fullpath;
	}
	
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
	
	if(!strcmp(params[0], "DC-REQ"))
	{
		if(param_num != 7)
			return "Erro: Quantidade de parâmetros inválida!\n";

		char* path = params[1];
		char* name = params[3];
		char* client = params[4];
		char* owner = params[5];
		char* others = params[6];

		char* fullpath = (char*)malloc(BUFFER * sizeof(char));
		char* answer;
		char  len[20];

		fullpath[0] = '\0';
		
		answer = create_directory(path, name, client, owner, others);

		if(answer == NULL) {
			printf("Erro: Não foi possível criar o diretório!\n");
			return "Erro: Não foi possível criar o diretório!\n";
		}

		snprintf(len, 20, "%lu", strlen(answer));

		strcpy(fullpath, "DC-REP ");
		strcat(fullpath, answer);
		strcat(fullpath, " ");
		strcat(fullpath, len);
		
		return fullpath;
	}

	if(!strcmp(params[0], "DR-REQ"))
	{
		if(param_num != 6)
			return "Erro: Quantidade de parâmetros inválida!\n";

		char* path   = params[1];
		char* name   = params[3];
		char* client = params[5];

		char* fullpath = (char*)malloc(BUFFER * sizeof(char));
		char* answer;
		char  len[20];

		fullpath[0] = '\0';

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
		
		strcpy(fullpath, "DR-REP ");
		strcat(fullpath, path);
		strcat(fullpath, " ");
		strcat(fullpath, len);

		return fullpath;
	}

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
	char pathWithDot[BUFFER];
	char* nameWithDot;
	char* name;
	char* aux;
	char* fileBuf = (char*)malloc(BUFSIZE * sizeof(char));
	int clientDescriptor;
	int rw;

	char dirPath[40]; dirPath[0] = '\0';


	while((aux = strsep(&pathdup, "/")) != NULL) name = aux;
	nameWithDot = (char*)malloc((strlen(name)+2)*sizeof(char));
	strcpy(nameWithDot, ".");
	strcat(nameWithDot, name);
	strcpy(pathWithDot, path);
	pathWithDot[strlen(pathWithDot) - strlen(name)] = '\0';

	strcpy(dirPath, pathWithDot);
	strcat(dirPath, ".directory");

	strcat(pathWithDot, nameWithDot);

	strcpy(fileBuf, client);
	strcat(fileBuf, " ");
	strcat(fileBuf, ownerPerm);
	strcat(fileBuf, " ");
	strcat(fileBuf, otherPerm);

	printf("path: %s / pathWithDot: %s / name: %s / dirPath: %s\n ", path, pathWithDot, name, dirPath);

	char* dirBufAux = (char*)malloc(BUFSIZE * sizeof(char));

	int dirDescriptor = open(dirPath, O_RDONLY);
	rw = pread(dirDescriptor, dirBufAux, 10, 0);
	if(rw > 0) {
		printf("Lendo arquivo de auth: %d / valor: %s\n", rw, dirBufAux);

		char* params[3];

		for(int i = 0; (params[i] = strsep(&dirBufAux, " ")) != NULL; i++);

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

		clientDescriptor = open(pathWithDot, O_RDWR | O_CREAT, 0666);
		rw = pwrite(clientDescriptor, fileBuf, strlen(fileBuf), 0);
		close(clientDescriptor);

		printf("Escrevendo arquivo de auth: %d\n", rw);
		free(fileBuf);
	}
	else
	{
		char* fileBufAux = (char*)malloc(BUFSIZE * sizeof(char));

		descriptor = open(path, O_WRONLY);
		clientDescriptor = open(pathWithDot, O_RDONLY);
		rw = pread(clientDescriptor, fileBufAux, 2*strlen(fileBuf), 0);
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
		remove(pathWithDot);

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

char* getFileInformation(char* path)
{
	//Client
	char* pathdup = strdup(path);
	char pathWithDot[BUFFER];
	char* nameWithDot;
	char* name;
	char* aux;
	int clientDescriptor;
	int rw;
	//int descriptor;

	while((aux = strsep(&pathdup, "/")) != NULL) name = aux;
	nameWithDot = (char*)malloc((strlen(name)+2)*sizeof(char));
	strcpy(nameWithDot, ".");
	strcat(nameWithDot, name);
	strcpy(pathWithDot, path);
	pathWithDot[strlen(pathWithDot) - strlen(name)] = '\0';
	strcat(pathWithDot, nameWithDot);

	char* fileBufAux = (char*)malloc(BUFSIZE * sizeof(char));

	printf("%d\n", strcmp(pathWithDot, "./newDir/.teste.txt"));

	if(pathWithDot[strlen(pathWithDot)] == '\n') {
		printf("bananas\n"); //Debug, apagar depois
	}
	

	clientDescriptor = open(pathWithDot, O_RDONLY);
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

char* create_directory(char* path, char* name, char* client, char* ownerPerm, char* otherPerm)
{
	struct stat st = {0};
	char* fullpath = (char*)malloc((strlen(path) + strlen(name) + 1) * sizeof(char));
	char* authPath;
	char* fileBuf = (char*)malloc(BUFSIZE * sizeof(char));
	mode_t permissao = S_IRWXU | S_IROTH | S_IWOTH | S_IXOTH;
	int descriptor;

	printf("create_directory -- path: %s, name: %s, fullpath: %s\n", path, name, fullpath);

	strcpy(fullpath, path);
	strcat(fullpath, "/");
	strcat(fullpath, name);

	authPath = strdup(fullpath);
	strcat(authPath, "/.directory");

	if (stat(fullpath, &st) == -1) 
	{
		if (mkdir(fullpath, permissao) != 0)
		{
			printf("Error in mkdir\n");
			return NULL;
		}
	}
	else 
	{
		printf("Stat error\n");
		return NULL;
	}

	strcpy(fileBuf, client);
	strcat(fileBuf, " ");
	strcat(fileBuf, ownerPerm);
	strcat(fileBuf, " ");
	strcat(fileBuf, otherPerm);

	printf("Buffer: %s\n", fileBuf);

	descriptor = open(authPath, O_WRONLY | O_CREAT, 0666);
	pwrite(descriptor, fileBuf, strlen(fileBuf), 0);
	close(descriptor);

	return fullpath;
}

char* remove_directory(char* path, char* name, char* client)
{
	char* fullpath = (char*)malloc((strlen(path) + strlen(name) + 1) * sizeof(char));

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

	printf("remove_directory -- path: %s, name: %s\n", path, name);

	strcpy(fullpath, path);
	strcat(fullpath, "/");
	strcat(fullpath, name);

	printf("fullpath: %s\n", fullpath);

	if (delete_everything(fullpath) != -1)
		return fullpath;
	
	printf("Erro remover diretorio!\n");
	return NULL;
}

char* list_directories(char* path)
{
	char* fullpath = getDirectory();
	int count;
	struct dirent** nameList;
	char* ret = malloc(BUFSIZE*(sizeof(char))); ret[0] = '\0';

	printf("list_directories -- path: %s\n", path);

	if(path[1] == '/')
		strcat(fullpath, &path[1]);

	printf("%s\n", fullpath);

	count = scandir(fullpath, &nameList, filter_files, alphasort);
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

int delete_everything(const char *dir) //talvez suma
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
