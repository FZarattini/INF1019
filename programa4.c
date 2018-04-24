#include <stdio.h>
#include <unistd.h>

int main()
{

	int b = 10;

	printf("Programa4 - IO bound\n");
	
		while(b >= 1)
		{
			printf("Faltam %d segundos para o Programa4 IO Bound terminar.\n", b);
			b--;
			sleep(1);
		}
		printf("Programa4 IO bound terminou\n");
		return 0;
}
