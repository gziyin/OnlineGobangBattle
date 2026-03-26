#include <stdio.h>
#include <string.h>
#include <mysql/mysql.h>
#define HOST "127.0.0.1"
#define PORT 3306
#define USER "root"
#define PASS "Guo050319!"
#define DB "gobang"

int main()
{
    //1.初始化句柄
    MYSQL * mysql = mysql_init(NULL);
    if(mysql == NULL)
    {
        printf("mysql_init error\n");
        mysql_close(mysql);
        return -1;
    }
    //2.连接数据库
    if(!mysql_real_connect(mysql, HOST, USER, PASS, DB, PORT, NULL, 0))
    {
        printf("mysql_real_connect error: %s\n", mysql_error(mysql));
        mysql_close(mysql);
        return -1;
    }
    //3.设置客户端字符集
    if(mysql_set_character_set(mysql, "utf8"))
    {
        printf("mysql_set_character_set error: %s\n", mysql_error(mysql));
        mysql_close(mysql);
        return -1;
    }
    //4.选择要操作的数据库 mysql_select_db(mysql, DB);
    //5.执行SQL语句
    // char * sql = "insert stu value(NULL,'zhangsan',18,90,12,67)";
    // char * sql = "update stu set ch=ch+1 where name='zhangsan'";
    // char * sql = "delete from stu where sn=1";
    char * sql = "select * from stu";
    if(mysql_query(mysql, sql))
    {
        printf("sql: %s\n", sql);
        printf("mysql_query error: %s\n", mysql_error(mysql));
        mysql_close(mysql);
        return -1;
    }
    //6.如果执行的是查询语句，就需要保存结果到本地
    MYSQL_RES * res = mysql_store_result(mysql);
    if(res == NULL)
    {
        printf("mysql_store_result error: %s\n", mysql_error(mysql));
        mysql_close(mysql);
        return -1;
    }
    //7.获取结果集中的结果条数
    int num_rows = mysql_num_rows(res);
    int num_cols = mysql_num_fields(res);
    printf("num_rows: %d, num_cols: %d\n", num_rows, num_cols);
    //8.遍历保存到本地结果集
    for (int i = 0; i < num_rows; i++)
    {
        MYSQL_ROW row = mysql_fetch_row(res);
        for (int j = 0; j < num_cols; j++)
        {
            printf("%s ", row[j]);
        }
        printf("\n");
    }
    //9.释放结果集
    mysql_free_result(res);
    //10.关闭连接，释放句柄
    mysql_close(mysql);
    return 0;
}