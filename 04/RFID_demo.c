////////////////////////////////////////////////////
//
//  Copyright(C), 2005 GEC Tech. Co., Ltd.
//
//  文件: uart/RFID/RFID.c
//  描述: 使用RFID读卡器读取RFID卡片信息
//
//  作者: Vincent Lin (林世霖)  微信公众号：秘籍酷
//  技术微店: http://weidian.com/?userid=260920190
//  技术交流: 260492823（QQ群）
//
////////////////////////////////////////////////////

#include <stdio.h>
#include <assert.h>
#include <fcntl.h> 
#include <unistd.h>
#include <termios.h> 
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include <pthread.h>

#include "ISO14443A.h"
#include "head.h"

int fifo_in, fifo_out; // 管道
int fd_in, fd_out; // 串口

void init_tty(int fd)
{    
	//声明设置串口的结构体
	struct termios config;
	bzero(&config, sizeof(config));

	// 设置无奇偶校验
	// 设置数据位为8位
	// 设置为非规范模式（对比与控制终端）
	cfmakeraw(&config);

	//设置波特率
	cfsetispeed(&config, B9600);
	cfsetospeed(&config, B9600);

	// CLOCAL和CREAD分别用于本地连接和接受使能
	// 首先要通过位掩码的方式激活这两个选项。    
	config.c_cflag |= CLOCAL | CREAD;

	// 一位停止位
	config.c_cflag &= ~CSTOPB;

	// 可设置接收字符和等待时间，无特殊要求可以将其设置为0
	config.c_cc[VTIME] = 0;
	config.c_cc[VMIN] = 1;

	// 用于清空输入/输出缓冲区
	tcflush (fd, TCIFLUSH);
	tcflush (fd, TCOFLUSH);

	//完成配置后，可以使用以下函数激活串口设置
	tcsetattr(fd, TCSANOW, &config);
}

void request_card(int fd)
{
	init_REQUEST();
	char recvinfo[128];

	char a[] = "|/-\\";
	for(int i=0; ; i++)
	{
		// 向串口发送指令
		tcflush (fd, TCIFLUSH);
		write(fd, PiccRequest_IDLE, PiccRequest_IDLE[0]);

		usleep(10*1000);

		bzero(recvinfo, 128);
		read(fd, recvinfo, 128);

		//应答帧状态部分为0 则请求成功
		if(recvinfo[2] == 0x00)	
		{
			break;
		}
		else
		{
			fprintf(stderr, "等待卡片%c\r", a[i%4]);
			usleep(200*1000);
		}
	}
}

void usage(int argc, char **argv)
{
	if(argc != 3)
	{
		fprintf(stderr, "用法: %s <入库串口> <出库串口>\n", argv[0]);
		exit(0);
	}
}

int get_id(int fd)
{
	// 刷新串口缓冲区
	tcflush (fd, TCIFLUSH);
	tcflush (fd, TCOFLUSH);

	// 初始化获取ID指令并发送给读卡器
	init_ANTICOLL();
	write(fd, PiccAnticoll1, PiccAnticoll1[0]);

	usleep(10*1000);

	// 获取读卡器的返回值
	char info[256];
	bzero(info, 256);
	read(fd, info, 128);

	// 应答帧状态部分为0 则成功
	uint32_t id = 0;
	if(info[2] == 0x00) 
	{
		memcpy(&id, &info[4], info[3]);

		if(id == 0)
		{
			return -1;
		}
	}
	else
	{
		return -1;
	}
	return id;
}

bool flag = true;

void refresh(int sig)
{
	// 卡片离开1秒后
	flag = true;
}

// 处理入库
void *routine(void *arg)
{
	uint32_t id;
	while(1)
	{
		// 检测附近是否有卡片... ...
		request_card(fd_in);

		// =======================================

		// 获取附近卡片的卡号... ...
		id = get_id(fd_in);
		if(id == 0 || id == 0xFFFFFFFF)
		{
			continue;
		}

		// flag为真意味着：卡片刚放上去
		if(flag)
		{
			// 将卡号发送给数据库程序
			write(fifo_in, &id, sizeof(id));
			flag = false;
		}
		alarm(1);
	}
	pthread_exit(NULL);
}

int main(int argc, char **argv) // RFID_demo /dev/ttySACx /dev/ttySACy
{
	usage(argc, argv);

	signal(SIGALRM, refresh);

	// 初始化串口
	fd_in  = open(argv[1]/*/dev/ttySACx*/, O_RDWR | O_NOCTTY);
	fd_out = open(argv[2]/*/dev/ttySACx*/, O_RDWR | O_NOCTTY);
	if(fd_in == -1 || fd_out == -1)
	{
		printf("打开串口: %s\n", strerror(errno));
		exit(0);
	}
	init_tty(fd_in);
	init_tty(fd_out);


	// 打开管道
	fifo_in  = open(RFID_SQLite_in,  O_RDWR);	
	fifo_out = open(RFID_SQLite_out, O_RDWR);	
	if(fifo_in == -1 || fifo_out == -1)
	{
		perror("打开管道失败");
		exit(0);
	}

	// 创建线程，专门读取入库卡号
	pthread_t tid;
	pthread_create(&tid, NULL, routine, NULL);

	// 主线程，专门读取出库卡号
	uint32_t id;
	while(1)
	{
		// 检测附近是否有卡片... ...
		request_card(fd_out);

		// =======================================

		// 获取附近卡片的卡号... ...
		id = get_id(fd_out);
		if(id == 0 || id == 0xFFFFFFFF)
		{
			continue;
		}

		// flag为真意味着：卡片刚放上去
		if(flag)
		{
			// 将卡号发送给数据库程序
			write(fifo_out, &id, sizeof(id));
			flag = false;
		}
		alarm(1);
	}

	close(fd_in);
	close(fd_out);
	close(fifo_in);
	close(fifo_out);

	pthread_exit(NULL);
}
