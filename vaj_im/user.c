#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>

extern MYSQL *conn;
#define BUFFER_SIZE 1024

int register_user(const char *username, const char *password,MYSQL *conn) {
    char query[256];
    snprintf(query, sizeof(query), "INSERT INTO users (username, password) VALUES ('%s', SHA2('%s', 256));", username, password);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "INSERT failed: %s\n", mysql_error(conn));
        return 0;
    }

    return 1;
}

const char* get_user_status(const char *user1, const char *user2,MYSQL *conn);
void get_friend_list_with_status(const char *username, char *friend_list_with_status, size_t size,MYSQL *conn) {
    char query[256];
    snprintf(query, sizeof(query), "SELECT user1, user2, nickname1, nickname2 FROM friends WHERE user1='%s' OR user2='%s';", username, username);

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
    size_t offset = 0;
    while ((row = mysql_fetch_row(res)) != NULL) {
        const char *friend_username;
        const char *nickname;
        const char *status;

        if (strcmp(row[0], username) == 0) {
            friend_username = row[1];
            nickname = row[2];
            status = get_user_status(username, friend_username,conn);
        } else {
            friend_username = row[0];
            nickname = row[3];
            status = get_user_status(username, friend_username,conn);
        }
        

        char display_name[BUFFER_SIZE];
        if (nickname == NULL || strlen(nickname) == 0) {
            snprintf(display_name, sizeof(display_name), "%s", friend_username);
        } else {
            snprintf(display_name, sizeof(display_name), "%s(%s)", nickname, friend_username);
        }

        //printf("friends: %s\n", display_name);
        //printf("status: %s\n", status);
        int n = snprintf(buffer, sizeof(buffer), "%s (%s)\n", display_name, status);
        if (offset + n < size) {
            strcpy(friend_list_with_status + offset, buffer);
            offset += n;
        } else {
            break;
        }
    }

    mysql_free_result(res);
}

const char* get_user_status(const char *user1, const char *user2,MYSQL *conn) {
    printf("Checking status between %s and %s\n", user1, user2);
    static char status[16];
    char query[256];

    // 确保 user1 和 user2 的顺序一致
    const char *u1 = user1;
    const char *u2 = user2;
    if (strcmp(user1, user2) > 0) {
        u1 = user2;
        u2 = user1;
    }

    snprintf(query, sizeof(query), "SELECT user1, user2, status1, status2 FROM friends WHERE (user1='%s' AND user2='%s') OR (user1='%s' AND user2='%s') LIMIT 1;", u1, u2, u2, u1);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "SELECT failed: %s\n", mysql_error(conn));
        return "offline";
    }

    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL) {
        fprintf(stderr, "mysql_store_result() failed: %s\n", mysql_error(conn));
        return "offline";
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    if (row) {
        // 确保返回的行中包含正确的状态信息
        if (strcmp(row[0], user1) == 0 && strcmp(row[1], user2) == 0) {
            strncpy(status, row[3], sizeof(status) - 1);
        } else if (strcmp(row[0], user2) == 0 && strcmp(row[1], user1) == 0) {
            strncpy(status, row[2], sizeof(status) - 1);
        } else {
            strncpy(status, "offline", sizeof(status) - 1);
        }
        status[sizeof(status) - 1] = '\0';
    } else {
        strncpy(status, "offline", sizeof(status) - 1);
        status[sizeof(status) - 1] = '\0';
    }

    mysql_free_result(res);
    printf("The found status is: %s\n", status);
    return status;
}

void get_friend_list(const char *username, char *friend_list, size_t size,MYSQL *conn) {
    char query[512];
    snprintf(query, sizeof(query), 
        "SELECT CASE WHEN user1 = '%s' THEN user2 ELSE user1 END AS friend_name "
        "FROM friends WHERE user1 = '%s' OR user2 = '%s';", 
        username, username, username);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "SELECT failed: %s\n", mysql_error(conn));
        snprintf(friend_list, size, "Error retrieving friend list.");
        return;
    }

    MYSQL_RES *result = mysql_store_result(conn);
    if (result == NULL) {
        fprintf(stderr, "mysql_store_result failed: %s\n", mysql_error(conn));
        snprintf(friend_list, size, "Error retrieving friend list.");
        return;
    }

    MYSQL_ROW row;
    size_t len = 0;
    while ((row = mysql_fetch_row(result))) {
        len += snprintf(friend_list + len, size - len, "%s,", row[0]);
        if (len >= size) break;
    }

    // 添加结束标志
    if (len < size) {
        snprintf(friend_list + len, size - len, "END_OF_FRIEND_LIST");
    }

    mysql_free_result(result);
}


void get_group_list(const char *username, char *group_list, size_t size,MYSQL *conn) {
    char query[256];
    snprintf(query, sizeof(query), "SELECT group_name FROM group_members WHERE username='%s';", username);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "SELECT failed: %s\n", mysql_error(conn));
        snprintf(group_list, size, "Error retrieving group list.");
        return;
    }

    MYSQL_RES *result = mysql_store_result(conn);
    if (result == NULL) {
        fprintf(stderr, "mysql_store_result failed: %s\n", mysql_error(conn));
        snprintf(group_list, size, "Error retrieving group list.");
        return;
    }

    MYSQL_ROW row;
    size_t len = 0;
    while ((row = mysql_fetch_row(result))) {
        len += snprintf(group_list + len, size - len, "%s,", row[0]);
        if (len >= size) break;
    }

    // 添加结束标志
    if (len < size) {
        snprintf(group_list + len, size - len, "END_OF_GROUP_LIST");
    }

    mysql_free_result(result);
}