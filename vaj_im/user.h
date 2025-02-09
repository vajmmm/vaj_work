#ifndef USER_H
#define USER_H

#include <mysql/mysql.h>

// 声明 register_user 函数
int register_user(const char *username, const char *password,MYSQL *conn);
void get_friend_list(const char *username, char *friend_list, size_t size,MYSQL *conn);
void get_group_list(const char *username, char *group_list, size_t size,MYSQL *conn);
const char* get_user_status(const char *user1, const char *user2,MYSQL *conn);
void get_friend_list_with_status(const char *username, char *friend_list_with_status, size_t size,MYSQL *conn);


#endif // USER_H