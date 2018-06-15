#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

//CPU BOUND
int main()
{

	int i = 1;

	while(i < 10)
	{
		sleep(1);
		i++;
	}
	
	return 0;
}
