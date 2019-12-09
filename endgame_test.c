#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "mastermind_ioctl.h"

int main(){
	int status = -1;
	
	int fd = open("/dev/mastermind", O_RDWR);
	status = ioctl(fd, MMIND_ENDGAME, 0);
	
	if(status == -1)
		perror("Cannot end the game");
	
	return status;
}