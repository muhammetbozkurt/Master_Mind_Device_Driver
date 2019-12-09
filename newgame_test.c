#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "mastermind_ioctl.h"

int main(int argc, char *argv[]){
	int status = -1;
	
	if(argc != 2) {
		perror("Invalid arguements. Try: ./newgame_tast [mmind_number]\n");
		return 0;
	}
	
	int fd = open("/dev/mastermind", O_RDWR);
	status = ioctl(fd, MMIND_NEWGAME, argv[1]);
	
	if(status == -1)
		perror("Cannot open a new game\n");
	
	return status;
}