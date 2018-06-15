#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

//CPU BOUND
int main()
{
	int i = 1;
	while(i < 5)
	{
		sleep(1);
		i++;
	}
	return 0;
}
