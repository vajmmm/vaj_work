#ifndef FRIEND_H
#define FRIEND_H

#include <mysql/mysql.h>

void add_friend_request(const char *requester, const char *requestee,MYSQL *conn);
void handle_friend_request(int client_sockfd, const char *username, const char *requester, const char *action,MYSQL *conn);
void list_friend_requests(int client_sockfd, const char *username,MYSQL *conn);
void add_friend(const char *user1, const char *user2,MYSQL *conn);
void remove_friend(const char *user1, const char *user2,MYSQL *conn);
int is_friend(const char *user1, const char *user2,MYSQL *conn);
void update_friend_status(const char *username, const char *status,MYSQL *conn);
void notify_friends_status(const char *username, const char *status,MYSQL *conn);
void send_private_message(int client_sockfd, const char *sender, const char *receiver, const char *message,MYSQL *conn);
void store_offline_message(const char *sender, const char *receiver, const char *message,MYSQL *conn);
void send_offline_messages(int client_sockfd, const char *username,MYSQL *conn);
void add_friend_nickname(const char *user1, const char *user2, const char *nickname,MYSQL *conn);
void update_friend_nickname(const char *user1, const char *user2, const char *nickname,MYSQL *conn);
void send_file(int client_sockfd, const char *receiver_username, const char *file_path,MYSQL *conn); // 添加文件传输函数


#endif // FRIEND_H