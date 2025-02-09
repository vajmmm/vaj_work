#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>
#include "friend.h"
#include "client.h" // 确保包含 client.h 以使用 find_client_by_username 函数

#define BUFFER_SIZE 1024 // 确保定义 BUFFER_SIZE

//extern MYSQL *conn;

client_t *find_client_by_username(const char *username);


void add_friend_request(const char *requester, const char *requestee,MYSQL *conn) {
    char query[256];
    snprintf(query, sizeof(query), "INSERT INTO friend_requests (requester, requestee) VALUES ('%s', '%s');", requester, requestee);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "INSERT failed: %s\n", mysql_error(conn));
        return;
    }

    printf("Friend request from %s to %s added successfully.\n", requester, requestee);
}

void handle_friend_request(int client_sockfd, const char *username, const char *requester, const char *action,MYSQL *conn) {
    if (strcmp(action, "accept") == 0) {
        add_friend(username, requester,conn);
        char query[256];
        snprintf(query, sizeof(query), "DELETE FROM friend_requests WHERE requester = '%s' AND requestee = '%s';", requester, username);
        if (mysql_query(conn, query)) {
            fprintf(stderr, "DELETE failed: %s\n", mysql_error(conn));
            return;
        }
        char response[BUFFER_SIZE];
        snprintf(response, sizeof(response), "You have accepted the friend request from %s.", requester);
        send(client_sockfd, response, strlen(response), 0);

        client_t *requestee_client = find_client_by_username(requester);
        if (requestee_client->sockfd != -1) {
            snprintf(response, sizeof(response), "add_friend:%s", requester);
            send(requestee_client->sockfd, response, strlen(response), 0);
        }

    } else if (strcmp(action, "decline") == 0) {
        char query[256];
        snprintf(query, sizeof(query), "DELETE FROM friend_requests WHERE requester = '%s' AND requestee = '%s';", requester, username);
        if (mysql_query(conn, query)) {
            fprintf(stderr, "DELETE failed: %s\n", mysql_error(conn));
            return;
        }
        char response[BUFFER_SIZE];
        snprintf(response, sizeof(response), "You have declined the friend request from %s.", requester);
        send(client_sockfd, response, strlen(response), 0);
    } else {
        char error_msg[BUFFER_SIZE];
        snprintf(error_msg, sizeof(error_msg), "Invalid handle_friend_request command format.");
        send(client_sockfd, error_msg, strlen(error_msg), 0);
    }
}

void list_friend_requests(int client_sockfd, const char *username,MYSQL *conn) {
    char query[256];
    snprintf(query, sizeof(query), "SELECT requester, request_time FROM friend_requests WHERE requestee = '%s';", username);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "SELECT failed: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL) {
        fprintf(stderr, "mysql_store_result() failed: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_ROW row;
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "list_friend_requests\n"); // 添加标志
    send(client_sockfd, buffer, strlen(buffer), 0);

    while ((row = mysql_fetch_row(res)) != NULL) {
        snprintf(buffer, sizeof(buffer), "Friend request from %s at %s\n", row[0], row[1]);
        send(client_sockfd, buffer, strlen(buffer), 0);
    }

    mysql_free_result(res);
}

void add_friend(const char *user1, const char *user2,MYSQL *conn) {
    // 确保 user1 和 user2 的顺序一致
    const char *u1 = user1;
    const char *u2 = user2;
    if (strcmp(user1, user2) > 0) {
        u1 = user2;
        u2 = user1;
    }

    char query[256];
    snprintf(query, sizeof(query), "INSERT INTO friends (user1, user2) VALUES ('%s', '%s');", u1, u2);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "INSERT failed: %s\n", mysql_error(conn));
        return;
    }

    printf("User %s and %s are now friends.\n", u1, u2);
}



void remove_friend(const char *user1, const char *user2,MYSQL *conn) {
    // 确保 user1 和 user2 的顺序一致
    const char *u1 = user1;
    const char *u2 = user2;
    if (strcmp(user1, user2) > 0) {
        u1 = user2;
        u2 = user1;
    }

    char query[256];
    snprintf(query, sizeof(query), "DELETE FROM friends WHERE user1 = '%s' AND user2 = '%s';", u1, u2);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "DELETE failed: %s\n", mysql_error(conn));
        return;
    }

    printf("User %s and %s are no longer friends.\n", u1, u2);

    // 通知被删除的好友
    char message[256];
    client_t *client1 = find_client_by_username(user2);
    if (client1 != NULL) {
        snprintf(message, sizeof(message), "remove_friend:%s", user1);
        send(client1->sockfd, message, strlen(message), 0);
    }
}

int is_friend(const char *user1, const char *user2,MYSQL *conn) {
    // 确保 user1 和 user2 的顺序一致
    const char *u1 = user1;
    const char *u2 = user2;
    if (strcmp(user1, user2) > 0) {
        u1 = user2;
        u2 = user1;
    }

    char query[256];
    snprintf(query, sizeof(query), "SELECT * FROM friends WHERE user1 = '%s' AND user2 = '%s';", u1, u2);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "SELECT failed: %s\n", mysql_error(conn));
        return 0;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL) {
        fprintf(stderr, "mysql_store_result() failed: %s\n", mysql_error(conn));
        return 0;
    }

    int num_rows = mysql_num_rows(res);
    mysql_free_result(res);

    return num_rows > 0;
}

void update_friend_status(const char *username, const char *status,MYSQL *conn) {
    char query[256];
    snprintf(query, sizeof(query), "UPDATE friends SET status1 = IF(user1='%s', '%s', status1), status2 = IF(user2='%s', '%s', status2) WHERE user1='%s' OR user2='%s';", username, status, username, status, username, username);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "UPDATE failed: %s\n", mysql_error(conn));
        return;
    }

    printf("Updated status of user %s to %s.\n", username, status);
}

void notify_friends_status(const char *username, const char *status,MYSQL *conn) {
    char query[256];
    snprintf(query, sizeof(query), "SELECT user1, user2 FROM friends WHERE user1='%s' OR user2='%s';", username, username);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "SELECT failed: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL) {
        fprintf(stderr, "mysql_store_result() failed: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != NULL) {
        const char *friend_username = strcmp(row[0], username) == 0 ? row[1] : row[0];
        client_t *friend_client = find_client_by_username(friend_username);
        if (friend_client != NULL) {
            char buffer[BUFFER_SIZE];
            snprintf(buffer, sizeof(buffer), "%s: %s", status, username);
            send(friend_client->sockfd, buffer, strlen(buffer), 0);
        }
    }

    mysql_free_result(res);
}

// 发送消息
void send_private_message(int client_sockfd, const char *sender, const char *receiver, const char *message,MYSQL *conn) {
    printf("Sending private message from %s to %s: %s\n", sender, receiver, message);
    client_t *receiver_client = find_client_by_username(receiver);
    if (receiver_client != NULL) {
        // 用户在线，直接发送消息
        char buffer[BUFFER_SIZE];
        snprintf(buffer, sizeof(buffer), "private_message:%s-%s", sender, message);
        printf("private_message:%s-%s\n", sender, message);
        send(receiver_client->sockfd, buffer, strlen(buffer), 0);
    } else {
        // 用户不在线，存储离线消息
        char query[256];
        snprintf(query, sizeof(query), "INSERT INTO offline_messages (sender, receiver, message) VALUES ('%s', '%s', '%s');", sender, receiver, message);
        if (mysql_query(conn, query)) {
            fprintf(stderr, "INSERT failed: %s\n", mysql_error(conn));
        }
    }
}

void store_offline_message(const char *sender, const char *receiver, const char *message,MYSQL *conn) {
    char query[512];
    snprintf(query, sizeof(query), "INSERT INTO offline_messages (sender, receiver, message) VALUES ('%s', '%s', '%s');", sender, receiver, message);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "INSERT failed: %s\n", mysql_error(conn));
        return;
    }

    printf("Offline message from %s to %s stored successfully.\n", sender, receiver);
}

void send_offline_messages(int client_sockfd, const char *username,MYSQL *conn) {
    char query[256];
    snprintf(query, sizeof(query), "SELECT sender, message, timestamp FROM offline_messages WHERE receiver = '%s';", username);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "SELECT failed: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL) {
        fprintf(stderr, "mysql_store_result() failed: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_ROW row;
    char buffer[BUFFER_SIZE];
    while ((row = mysql_fetch_row(res)) != NULL) {
        snprintf(buffer, sizeof(buffer), "Offline message from %s at %s: %s\n", row[0], row[2], row[1]);
        send(client_sockfd, buffer, strlen(buffer), 0);
    }

    mysql_free_result(res);

    // 删除已发送的离线消息
    snprintf(query, sizeof(query), "DELETE FROM offline_messages WHERE receiver = '%s';", username);
    if (mysql_query(conn, query)) {
        fprintf(stderr, "DELETE failed: %s\n", mysql_error(conn));
    }

    // 发送离线消息结束标记
    const char *end_of_offline_messages = "END_OF_OFFLINE_MESSAGES";
    send(client_sockfd, end_of_offline_messages, strlen(end_of_offline_messages), 0);
}

void add_friend_nickname(const char *user1, const char *user2, const char *nickname,MYSQL *conn) {
    // 确保 user1 和 user2 的顺序一致
    const char *u1 = user1;
    const char *u2 = user2;
    if (strcmp(user1, user2) > 0) {
        u1 = user2;
        u2 = user1;
    }

    // 查询表中是否存在记录
    char query[256];
    snprintf(query, sizeof(query), "SELECT * FROM friends WHERE user1='%s' AND user2='%s';", u1, u2);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "SELECT failed: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL) {
        fprintf(stderr, "mysql_store_result() failed: %s\n", mysql_error(conn));
        return;
    }

    int num_rows = mysql_num_rows(res);
    mysql_free_result(res);

    // 根据查询结果判断存储位置
    if (num_rows > 0) {
        // 记录已存在，更新备注名
        if (strcmp(user1, u1) == 0) {
            snprintf(query, sizeof(query), "UPDATE friends SET nickname1='%s' WHERE user1='%s' AND user2='%s';", nickname, u1, u2);
        } else {
            snprintf(query, sizeof(query), "UPDATE friends SET nickname2='%s' WHERE user1='%s' AND user2='%s';", nickname, u1, u2);
        }
    } else {
        // 记录不存在，插入新记录
        snprintf(query, sizeof(query), "INSERT INTO friends (user1, user2, nickname1) VALUES ('%s', '%s', '%s');", u1, u2, nickname);
    }

    if (mysql_query(conn, query)) {
        fprintf(stderr, "INSERT/UPDATE failed: %s\n", mysql_error(conn));
    } else {
        printf("Friend nickname added/updated successfully.\n");
    }
}

void update_friend_nickname(const char *user1, const char *user2, const char *nickname,MYSQL *conn) {
    // 确保 user1 和 user2 的顺序一致
    const char *u1 = user1;
    const char *u2 = user2;
    if (strcmp(user1, user2) > 0) {
        u1 = user2;
        u2 = user1;
    }

    // 查询表中是否存在记录
    char query[256];
    snprintf(query, sizeof(query), "SELECT * FROM friends WHERE user1='%s' AND user2='%s';", u1, u2);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "SELECT failed: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL) {
        fprintf(stderr, "mysql_store_result() failed: %s\n", mysql_error(conn));
        return;
    }

    int num_rows = mysql_num_rows(res);
    mysql_free_result(res);

    // 根据查询结果判断存储位置
    if (num_rows > 0) {
        // 记录已存在，更新备注名
        if (strcmp(user1, u1) == 0) {
            snprintf(query, sizeof(query), "UPDATE friends SET nickname1='%s' WHERE user1='%s' AND user2='%s';", nickname, u1, u2);
        } else {
            snprintf(query, sizeof(query), "UPDATE friends SET nickname2='%s' WHERE user1='%s' AND user2='%s';", nickname, u1, u2);
        }
    } else {
        fprintf(stderr, "No existing friend relationship found between %s and %s.\n", u1, u2);
        return;
    }

    if (mysql_query(conn, query)) {
        fprintf(stderr, "UPDATE failed: %s\n", mysql_error(conn));
    } else {
        printf("Friend nickname updated successfully.\n");
    }
}

void send_file(int client_sockfd, const char *receiver_username, const char *file_path,MYSQL *conn) {
    FILE *file = fopen(file_path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Failed to open file %s\n", file_path);
        return;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "FILE %s %ld", file_path, file_size);
    send(client_sockfd, buffer, strlen(buffer), 0);

    while (!feof(file)) {
        size_t bytes_read = fread(buffer, 1, sizeof(buffer), file);
        if (bytes_read > 0) {
            send(client_sockfd, buffer, bytes_read, 0);
        }
    }

    fclose(file);
    printf("File %s sent to %s successfully.\n", file_path, receiver_username);
}