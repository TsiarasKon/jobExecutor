#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <error.h>
#include <errno.h>

char *fifo = "./pipes/Worker5_0";

void	main(int argc, char *argv[]){
	int fd, i, nwrite;
	char msgbuf[BUFSIZ];

	if (argc<2) { printf("Usage: sendamessage ... \n"); exit(1); }

	if ( mkfifo(fifo, 0666) == -1 ){
		if ( errno!=EEXIST ) { perror("receiver: mkfifo"); exit(6); };
		}
	if ( (fd=open(fifo, O_WRONLY| O_NONBLOCK)) < 0)
		{ perror("fife open error"); exit(1); }

	for (i=1; i<argc; i++){
		strcpy(msgbuf, argv[i]);
		if ((nwrite=write(fd, msgbuf, BUFSIZ)) == -1)
			{ perror("Error in Writing"); exit(2); }

		}
	exit(0);
}
		
