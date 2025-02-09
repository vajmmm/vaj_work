#ifndef GROUP_H
#define GROUP_H

#include <mysql/mysql.h>
#include "client.h"

typedef struct {
    char name[50];
    char owner[50];
} group_t;

void create_group(const char *group_name, client_t *client,MYSQL *conn);
void delete_group(const char *group_name, client_t *client,MYSQL *conn);
group_t *find_group_by_name(const char *group_name,MYSQL *conn);
void join_group(group_t *group, client_t *client,MYSQL *conn);
void leave_group(group_t *group, client_t *client,MYSQL *conn);
void send_group_message(const char *group_name, const char *sender_username, const char *message,MYSQL *conn);
void add_user(const char *username, const char *password,MYSQL *conn);
void invite_user_to_group(const char *group_name, const char *username,MYSQL *conn); 
void accept_group_invitation(const char *group_name, const char *username,MYSQL *conn);
void handle_invite(int client_sockfd, const char *group_name, const char *invitee,MYSQL *conn);
void list_invitations(int client_sockfd, const char *username,MYSQL *conn);
int user_exists(const char *username,MYSQL *conn);
void change_group_name(const char *old_name, const char *new_name, client_t *client,MYSQL *conn); // 添加更改群组名函数
void remove_member_from_group(const char *group_name, const char *member_username, client_t *client,MYSQL *conn); // 添加删除成员函数


#endif // GROUP_H