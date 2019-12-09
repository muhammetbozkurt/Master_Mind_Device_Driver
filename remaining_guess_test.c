#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "mastermind_ioctl.h"

int main(){
	int status = -1;
	int value;
	int fd = open("/dev/mastermind", O_RDWR);
	status = ioctl(fd, MMIND_REMAINING, &value);
	
	if(status == -1)
		perror("Cannot get remaining guesses!");
	else
		printf("Remaining guesses: %d\n", value);
	
	return status;
}