#include "messagebox.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>

int main (int argc, char* argv[])
{

	int file_desc, ret_val;
	char *msg = "Message passed by ioctl\n";
	int ret=0;

	file_desc = open(argv[1], 0);
	if (file_desc < 0) {
		printf("Can't open device file: %s\n", argv[1]);
		exit(-1);
	}
	ret = ioctl (file_desc, 0, atoi(argv[2])); 
	if (ret != 0)
	{
		printf("Permission Error\n");
	}

	close(file_desc);
	return ret;
}
