#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>

#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/fb.h>
#include <linux/un.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include "sqlite3.h"
#include "head.h"


int fifo_in, fifo_out;

char *err;
char SQL[200];

// 每当得到一行（即一个记录）就会调用一次以下函数
// len: 列数目
// col_val:  每一列的值
// col_name: 每一列的标题
//
// 返回值: 0: 继续对剩下的记录调用该函数
//       非0: 终止调用该函数

bool first = true;

int callback(void *arg, int len, char **col_val, char **col_name)
{
	// 1，检测库中是否有相应的卡号
	if(arg != NULL)
	{
		(*(int *)arg) = time(NULL) - atol(col_val[2]);
		return 0;
	}

	// 2，显示当前数据库的信息
	// 针对第一行记录调用本函数的时候，就显示标题
	if(first)
	{
		printf("\n");
		for(int i=0; i<len; i++)
		{
			printf("%s\t\t", col_name[i]);
		}
		first = false;
		printf("\n=====================");
		printf("=====================\n");
	}

	for(int i=0; i<len; i++)
	{
		printf("%s\t", col_val[i]);
	}
	printf("\n");

	return 0;
}

int Sqlite3_exec(sqlite3 *db, char *SQL, int (*callback)(void *,int,char**,char**), void *arg, char **errmsg)
{
	int ret = sqlite3_exec(db, SQL, callback, arg, errmsg);
	if(ret != SQLITE_OK)
	{
		printf("执行[%s]失败: %s\n", SQL, *errmsg);
		exit(0);
	}
}

// 随机生成一个车牌号
char *license(void)
{
	return "粤B9MK48";
}


void beep(int times, float sec)
{
	int buz = open("/dev/beep", O_RDWR);
	if(buz <= 0)
	{
		perror("打开蜂鸣器失败");
		return;
	}

	for(int i=0; i<times; i++)
	{
		// 响
		ioctl(buz, 0, 1);
		usleep(sec*1000*1000);

		// 静
		ioctl(buz, 1, 1);
		usleep(sec*1000*1000);
	}

	close(buz);
}

// 处理入库
void *routine(void *arg)
{
	sqlite3* db = (sqlite3 *)arg;

	while(1)
	{
		uint32_t id;
		read(fifo_in, &id, sizeof(id));

		// 判断卡片的合法性
		bzero(SQL, 200);
		snprintf(SQL, 200, "SELECT * FROM carinfo WHERE 卡号='%u';", id);
		int n = 0;
		Sqlite3_exec(db, SQL, callback, (void *)&n, &err);
		if(n != 0)
		{
			printf("【此卡已进场】\n");
			beep(5, 0.05);
			continue;
		}
		else
		{
			printf("欢迎%s入场\n", license());
			beep(1, 0.3);
		}

		bzero(SQL, 200);
		snprintf(SQL, 200, "INSERT INTO carinfo VALUES('%u', '%s', '%ld');", id, license(), time(NULL));
		Sqlite3_exec(db, SQL, NULL, NULL, &err);

		// 查询当前数据库中的数据
		bzero(SQL, 200);
		snprintf(SQL, 200, "SELECT * FROM carinfo;");
		Sqlite3_exec(db, SQL, callback, NULL, &err);
		printf("=====================");
		printf("=====================\n");
		first = true;
	}
}

int main(int argc, char **argv)
{
	// 1，创建、打开一个数据库文件
	sqlite3 *db = NULL;
	int ret = sqlite3_open_v2("parking.db", &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
	if(ret != SQLITE_OK)
	{
		printf("创建数据库文件失败: %s\n", sqlite3_errmsg(db));
		exit(0);
	}


	// 2，创建表
	bzero(SQL, 200);
	snprintf(SQL, 200, "%s", "CREATE TABLE IF NOT EXISTS carinfo(卡号 TEXT, 车牌 TEXT, 入库时间 TEXT);");
	Sqlite3_exec(db, SQL, NULL, NULL, &err);


	// 打开管道
	fifo_in  = open(RFID_SQLite_in,  O_RDWR);
	fifo_out = open(RFID_SQLite_out, O_RDWR);


	// 3，创建线程，等待入库卡号
	pthread_t tid;
	pthread_create(&tid, NULL, routine, (void *)db);


	// 3，主线程，等待出库的卡号
	while(1)
	{
		uint32_t id;
		read(fifo_out, &id, sizeof(id));

		// 判断卡片是否合法
		bzero(SQL, 200);
		snprintf(SQL, 200, "SELECT * FROM carinfo WHERE 卡号='%u';", id);
		int n = 0;
		Sqlite3_exec(db, SQL, callback, (void *)&n, &err);
		if(n == 0)
		{
			printf("【此卡未进场】\n");
			beep(5, 0.05);
			continue;
		}
		else
		{
			printf("停车时长%d秒，收费%d元\n", n, 30);
			beep(1, 0.3);
		}

		// 出库
		bzero(SQL, 200);
		snprintf(SQL, 200, "DELETE FROM carinfo WHERE 卡号='%u';", id);
		Sqlite3_exec(db, SQL, NULL, NULL, &err);

		// 查询当前数据库中的数据
		bzero(SQL, 200);
		snprintf(SQL, 200, "SELECT * FROM carinfo;");
		Sqlite3_exec(db, SQL, callback, NULL, &err);
		printf("=====================");
		printf("=====================\n");
		first = true;
	}


	return 0;
}
