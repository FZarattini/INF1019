#include <stdio.h>
#include <unistd.h>

int main()
{

	int i = 0;

	printf("Programa4 - IO bound\n");
	
		while(i < 10)
		{
			sleep(1);
			printf("Programa4 Esperando IO\n");
			i++;
		}
		printf("Espera por IO terminou\n");
		return 0;
}
