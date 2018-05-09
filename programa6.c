#include <stdio.h>
#include <unistd.h>

//IO Bound
int main()
{

	int i = 0;

	printf("Programa6 - IO bound\n");
		while(i < 2)
		{		    
			sleep(1);
		    	printf("Programa6 Esperando IO\n");
			i++;
		}
		printf("Espera por IO terminou!\n");
	    return 0;
}
