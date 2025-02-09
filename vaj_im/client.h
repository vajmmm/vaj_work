#ifndef CLIENT_H
#define CLIENT_H

#define MAX_CLIENTS 10

#include <mysql/mysql.h>
#include <netinet/in.h>


typedef struct {
    int sockfd;
    struct sockaddr_in address;
    char username[50];
    MYSQL *conn;
} client_t;

typedef struct {
    char Type[20];    // 消息类型
    char target[10];  // 消息内容
    char message[400];  // 消息长度
} vaj_message;

#endif // CLIENT_H  