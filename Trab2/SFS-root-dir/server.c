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

#define BUFFER  100
#define BUFSIZE 1024


static char* fileRead(char* path, int *nrbytes, int offset);
static int fileWrite(char* path, char* payload, int nrbytes, int offset, char* client, char* ownerPerm, char* otherPerm);
static char* fileInfo(char* path);

static char* dirCreate(char* path, char* name, char* client, char* ownerPerm, char* otherPerm);
static char* dirRemove(char* path, char* name, char* client);
static char* dirList(char* path);

static char* getDirectory();
static int filesFilter(const struct dirent* nameList);
static void error(char *msg);
static int fileExist (char* filename);
static int recursive_delete(const char *dir);

char* runCommand(char* command)
{
	char* params[10];

	int param_num = 0;
	for(int i = 0; (params[i] = strsep(&command, " ")) != NULL; i++, param_num++)
	{
		if(i == 11)
			return "ERROR: to many parameters!";
	}

	//return NULL;

    printf("PARAM %d\n", param_num);
	if(!strcmp(params[0], "RD-REQ"))
	{
		if(param_num != 5)
			return "ERROR: wrong number of parameters";

		char* path = params[1];
		// int len = atoi(params[2]);
		char* payload;
		int nrbytes = atoi(params[3]);
		int offset = atoi(params[4]);

		char* fullpath = (char*)malloc(BUFFER * sizeof(char));
		char lenPath[20];
		char bt[20];

		fullpath[0] = '\0';
		
		payload = fileRead(path, &nrbytes, offset);
		printf("Payload: %s\n", payload);

		if (payload == NULL)
		{
			printf("Error reading file\n");
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

		printf("Response: %s\n", fullpath);

		// path(string), strlen(int), payload (string), nrbytes lidos(int), offset igual ao do R-REQ(int)
		return fullpath;
	}
	
	if(!strcmp(params[0], "WR-REQ"))
	{
		//return NULL;
		if(param_num != 9)
			return "ERROR: wrong number of parameters";

		char* path = params[1];
		// int len = atoi(params[2]);
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
		
		nrbytes = fileWrite(path, payload, nrbytes, offset, client, owner, others);
		printf("Bytes Written Response: %d\n", nrbytes);

		if (nrbytes == -1)
		{
			printf("Error writing in file\n");
			return "Error writing in file\n";
		}
		if(nrbytes == -3) {
			printf("ERROR: permission denied\n");
			return "ERROR: permission denied\n";
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

		printf("Response: %s\n", fullpath);
		
		// path (string), strlen(int), payload(string vazio), nrbytes escritos (int), offset igual ao do W-REQ(int)
		return fullpath;
	}
	
	if(!strcmp(params[0], "FI-REQ"))
	{
		char* path = params[1];

		char* rep = (char*)malloc(BUFFER * sizeof(char));

		strcpy(rep, "FI-REP ");
		strcat(rep, path);
		strcat(rep, " ");

		char* temp = fileInfo(path);
		if(temp == NULL) {
			return "ERROR: could not get file info";
		}
		strcat(rep, temp);
		
		// path (string), strlen(int), owner(int), permissions (2char), filelength (int)
		return rep;
	}
	
	if(!strcmp(params[0], "DC-REQ"))
	{
		if(param_num != 7)
			return "ERROR: wrong number of parameters\n";

		char* path = params[1];
		char* name = params[3];
		char* client = params[4];
		char* owner = params[5];
		char* others = params[6];

		char* fullpath = (char*)malloc(BUFFER * sizeof(char));
		char* answer;
		char  len[20];

		fullpath[0] = '\0';
		
		answer = dirCreate(path, name, client, owner, others);

		if(answer == NULL) {
			printf("Error creating directory\n");
			return "Error creating directory\n";
		}

		snprintf(len, 20, "%lu", strlen(answer));

		strcpy(fullpath, "DC-REP ");
		strcat(fullpath, answer);
		strcat(fullpath, " ");
		strcat(fullpath, len);
		
		// path(string), strlen(int)
		return fullpath;
	}

	if(!strcmp(params[0], "DR-REQ"))
	{
		if(param_num != 6)
			return "ERROR: wrong number of parameters";

		char* path   = params[1];
		char* name   = params[3];
		char* client = params[5];

		char* fullpath = (char*)malloc(BUFFER * sizeof(char));
		char* answer;
		char  len[20];

		fullpath[0] = '\0';

		answer = dirRemove(path, name, client);

		printf("Answer: %s\n", answer);

		if (answer == NULL)
		{
			snprintf(len, 20, "%d", 0);
		}
		else
		{
			snprintf(len, 20, "%lu", strlen(path));
		}

		printf("Tamanho do path: %s\n", len);
		
		strcpy(fullpath, "DR-REP ");
		strcat(fullpath, path);
		strcat(fullpath, " ");
		strcat(fullpath, len);

		// path(string), strlen(int)
		return fullpath;
	}

	if(!strcmp(params[0], "DL-REQ"))
	{
		char* path = params[1];

		char* rep = (char*)malloc(BUFFER * sizeof(char));

		strcpy(rep, "DL-REP ");
		strcat(rep, "\n");
		strcat(rep, dirList(path));
		
		
		// allfilenames (string), fstlstpositions (array[40] of struct {int, int}), nrnames (int)
		return rep;
	}
	
	printf("UNRECOGNIZED COMMAND!!\n");
	return NULL;
}

static void runServer(int port)
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
            reply = strdup("Error: could not understand command!");
        }
        printf("Reply: %s\n", reply);
        
        /* 
         * sendto: echo the input back to the client 
         */
        n = sendto(sockfd, reply, strlen(reply) + 1, 0, (struct sockaddr *) &clientaddr, clientlen);
        if (n < 0) 
            error("ERROR in sendto");
        printf("Answer sent\n");
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

    runServer(portno);

	return 0;
}

static char* fileRead(char* path, int* nrbytes, int offset)
{
	char *payload = (char*)malloc(BUFSIZE * sizeof(char));
	int descriptor;
	int bytes;

	printf("fileRead -- path: %s, nrbytes: %d, offset: %d\n", path, *nrbytes, offset);

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

static int fileWrite(char* path, char* payload, int nrbytes, int offset, char* client, char* ownerPerm, char* otherPerm)
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



	//

	
	if (!fileExist(path))
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

		// fileBufAux[0] = '\0';

		// char dirPath[20];
		// strcpy(dirPath, path);
		// strcat(dirPath, ".directory");

		// int dirDescriptor = open(dirPath, O_WRONLY);
		// rw = pread(dirDescriptor, fileBufAux, 2*strlen(fileBuf), 0);
		// printf("Lendo arquivo de auth: %d / valor: %s\n", rw, fileBufAux);
		// close(dirDescriptor);

		// for(int i = 0; (params[i] = strsep(&fileBufAux, " ")) != NULL; i++);

		// if (params[2][0] == 'R') {
		// 	if(strcmp(params[0], client) != 0) {
		// 		close(descriptor);
		// 		return -3;
		// 	}
		// }


		// free(fileBuf);
		// free(fileBufAux);
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

	printf("fileWrite -- path: %s, payload: %s, nrbytes: %d, offset: %d\n", path, payload, nrbytes, offset);

	written = pwrite(descriptor, payload, nrbytes, offset);

	close(descriptor);

	printf("Bytes written: %d\n", written);
	
	return written;
}

static char* fileInfo(char* path)
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
		printf("bananas\n");
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
	snprintf(sz, 19, "%lld", st.st_size);
	
	strcat(ret, " ");
	strcat(ret, sz);
	
	close(clientDescriptor);

	printf("Lendo arquivo de auth: %d / valor: %s\n", rw, ret);

	return ret;
}

static char* dirCreate(char* path, char* name, char* client, char* ownerPerm, char* otherPerm)
{
	struct stat st = {0};
	char* fullpath = (char*)malloc((strlen(path) + strlen(name) + 1) * sizeof(char));
	char* authPath;
	char* fileBuf = (char*)malloc(BUFSIZE * sizeof(char));
	mode_t permissao = S_IRWXU | S_IROTH | S_IWOTH | S_IXOTH;
	int descriptor;

	printf("dirCreate -- path: %s, name: %s, fullpath: %s\n", path, name, fullpath);

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

static char* dirRemove(char* path, char* name, char* client)
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

	printf("dirRemove -- path: %s, name: %s\n", path, name);

	strcpy(fullpath, path);
	strcat(fullpath, "/");
	strcat(fullpath, name);

	printf("fullpath: %s\n", fullpath);

	if (recursive_delete(fullpath) == -1)
	{
		printf("Erro remover diretorio!\n");
		return NULL;
	}

	return fullpath;
}

static char* dirList(char* path)
{
	char* fullpath = getDirectory();
	int count;
	struct dirent** nameList;
	char* ret = malloc(BUFSIZE*(sizeof(char))); ret[0] = '\0';

	printf("dirList -- path: %s\n", path);

	if(path[1] == '/')
		strcat(fullpath, &path[1]);

	printf("%s\n", fullpath);

	count = scandir(fullpath, &nameList, filesFilter, alphasort);
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

static char* getDirectory()
{
	char* currentDir = (char*)malloc(BUFFER*sizeof(char)) ;

	if (getcwd(currentDir, BUFFER) == NULL )
	{
		printf("Error getting path\n"); 
		exit(0);
	}

	return currentDir;
}

static int filesFilter(const struct dirent* nameList)
{
	if ((strcmp(nameList->d_name, ".") == 0) || (strcmp(nameList->d_name, "..") == 0) || (nameList->d_name[0] == '.'))  
		return 0; 
	else
		return 1;
}

static void error(char *msg)
{
    perror(msg);
    exit(1);
}

static int fileExist (char* filename)
{
  struct stat buffer;   
  return stat(filename, &buffer) == 0;
}

static int recursive_delete(const char *dir)
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
    	printf("dentro do while\n");
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
