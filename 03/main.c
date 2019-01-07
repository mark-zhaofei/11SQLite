////////////////////////////////////////////////
//
//  Copyright(C), 广州粤嵌通信科技股份有限公司
//
//  作者: Vincent Lin (林世霖)
//
//  微信公众号: 秘籍酷
//  技术交流群: 260492823（QQ群）
//  GitHub链接: https://github.com/vincent040
//
//  描述: 使用SQLite的C-API做经典SQL增删改查
//
////////////////////////////////////////////////

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
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include "sqlite3.h"

bool first = true;

int callback(void *counter, int col_num, char **col_val, char **col_name)
{
	if(counter != NULL)
	{
		(*(int*)counter)++;
		return 0;
	}

	if(first)
	{
		printf("\n");
		for(int i=0; i<col_num; i++)
		{
			printf("%s\t", col_name[i]);
		}
		first = false;
		printf("\n================================\n");
	}

	for(int i=0; i<col_num; i++)
	{
		printf("%s\t", col_val[i]);
	}
	printf("\n");

	return 0;
}

void Sqlite3_exec(sqlite3 *db, const char *SQL,
		 int (*callback)(void *, int, char **, char **), void *arg)
{
	char *err;
	if(sqlite3_exec(db, SQL, callback, arg, &err) != SQLITE_OK)
	{
		printf("执行[%s]失败:%s\n", SQL, err);
	}
}

char *license(void)
{
	return "粤B9MK48";
}

int main(int argc, char **argv)
{
	sqlite3 *db = NULL;

	//1，打开/创建一个数据库：parking.db
	int ret;
	ret = sqlite3_open_v2("parking.db", &db,
			SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
			NULL);
	if(ret != SQLITE_OK)
	{
		printf("[%d] error: %s\n", __LINE__, sqlite3_errmsg(db));
		exit(0);
	}

	// 2，在数据库中，创建一张表 carinfo
	//    并使得表中存放卡号、车牌号和入场时间
	char SQL[500];
	bzero(SQL, 500);
	snprintf(SQL, 500, "%s", "CREATE TABLE IF NOT EXISTS carinfo(卡号     TEXT,"
	                         "                                   车牌号码 TEXT,"
	                         "                                   入库时间 TEXT);");

	// 使用API执行以上SQL语句
	Sqlite3_exec(db, SQL, NULL, NULL);


	// 键盘输入卡号，模拟车辆出入库
	int id;
	time_t t;
	while(1)
	{
		printf("请输入卡号（正的代表入库，负的代表出库，0退出程序）:");

		while(scanf("%d", &id) != 1)
		{
			printf("请输入整数.\n");
			while(getchar() != '\n');
		}

		if(id == 0)
			break;

		// 模拟车辆入库
		bzero(SQL, 500);
		if(id > 0)
		{
			snprintf(SQL, 500, "INSERT INTO carinfo VALUES('%d', '%s', '%ld');",
								 id, license(), time(&t));
		}
		// 模拟车辆出库
		else
		{
			// 检测对应卡号是否已存在与数据库中
			int n = 0;
			snprintf(SQL, 500, "SELECT * FROM carinfo WHERE 卡号='%d';", -id);
			Sqlite3_exec(db, SQL, callback, (void *)&n);
			if(n == 0)
			{
				printf("卡号[%d]不存在.\n", -id);
				continue;
			}

			snprintf(SQL, 500, "DELETE FROM carinfo WHERE 卡号='%d';", -id);
		}

		Sqlite3_exec(db, SQL, NULL, NULL);

		// 查看车库信息
		bzero(SQL, 500);
		snprintf(SQL, 500, "SELECT * FROM carinfo;");
		Sqlite3_exec(db, SQL, callback, NULL);

		first = true;
	}

	// 关闭数据库文件
	sqlite3_close(db);
	return 0;
}
