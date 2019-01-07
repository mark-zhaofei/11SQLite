#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "head.h"

void usage(int argc, char **argv)
{
	if(argc != 3)
	{
		printf("用法: %s <入库串口> <出库串口>\n", argv[0]);
		exit(0);
	}
}


int main(int argc, char **argv)
{
	usage(argc, argv);

	// 0，创建进程间通信的有名管道
	mkfifo(RFID_SQLite_in,  0777);
	mkfifo(RFID_SQLite_out, 0777);
	mkfifo(SQLite_V4L2, 0777);
	mkfifo(SQLite_ALSA, 0777);

	// 1，启动刷卡程序
	pid_t pid1 = fork();
	if(pid1 == 0)
	{
		execl("./RFID_demo.elf", "./RFID_demo.elf", argv[1], argv[2], NULL);
	}


	// 2，启动数据库程序
	pid_t pid2 = fork();
	if(pid2 == 0)
	{
		execl("./SQLite_demo.elf", "./SQLite_demo.elf", NULL);
	}


	while(1)
		pause();

	return 0;
}
