#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>

#define PACE_USEC (100000)

#define xstr(a) str(a)
#define str(a) #a

static void do_main(int num)
{
	int fd;
	int pos;

	pos = (num * 6);

	srand(pos);

	printf("task: %d opening %s\n", num, xstr(DEV));

	fd = open(xstr(DEV), O_WRONLY);
	if(fd < 0) {
		perror("cannot open device\n");
		return;
	}

	do {
		char buffer[5];
		int val;
		int len;

		if (lseek(fd, pos, SEEK_SET) < 0) {
			perror("lseek error\n");
			return;
		}

		val = ((long long)rand() * 1000 / RAND_MAX);
		len = snprintf(buffer, 4, "%d", val);
		buffer[len] = '\0';
		if (write(fd,buffer,len) < 0) {
			perror("write error\n");
			return;
		}
		printf("task: %d => wrote [ %s ] on pos [ %d ]\n",num , buffer, pos);
		usleep(PACE_USEC);
	}
	while(1);
	close(fd);
}

int main(int argc, const char *argv[])
{
	int n_childs = 0;
	pid_t pid;
	
	printf("#################\nLCD test x86\n#################\n");
	if (argc > 1) {
		n_childs = atoi(argv[1]);
	}

	while (n_childs--) {
		pid = fork();
		if (!pid) {
			/* child */
			// usleep(100000 * n_childs); // per sfalsare i processi
			do_main(n_childs);
		} else if(pid < 0) {
			perror("error forking\n");
		}
	}
	do_main(0);
	wait(NULL);

	return 0;
}


