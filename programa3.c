#include <stdio.h>
#include <unistd.h>

int main()
{

	int b = 0;

	printf("Programa3 - IO bound\n");
		while(b < 4)
		{
			b++;		    
			sleep(1);
		    printf("Programa3 - IO Bound sendo executado a %d segundos\n", b);
		}
		printf("Programa3 - IO bound terminou\n");
	    return 0;
}
