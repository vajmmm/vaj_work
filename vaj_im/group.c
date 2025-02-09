#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include "group.h"

#define BUFFER_SIZE 1024

extern MYSQL *conn;  // 从其他文件中引入 MySQL 连接

client_t *find_client_by_username(const char *username);

int user_exists(const char *username,MYSQL *conn) {
    char query[256];
    snprintf(query, sizeof(query), "SELECT * FROM users WHERE username='%s';", username);

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

// 添加用户
void add_user(const char *username, const char *password,MYSQL *conn) {
    char query[256];
    snprintf(query, sizeof(query), "INSERT INTO users (username, password) VALUES ('%s', SHA2('%s', 256));", username, password);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "INSERT failed: %s\n", mysql_error(conn));
        return;
    }

    printf("User %s added successfully.\n", username);
}

// 创建群组
void create_group(const char *group_name, client_t *client,MYSQL *conn) {
    char query[256];
    snprintf(query, sizeof(query), "INSERT INTO user_groups (name, owner) VALUES ('%s', '%s');", group_name, client->username);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "INSERT failed: %s\n", mysql_error(conn));
        return;
    }

    printf("Group %s created successfully.\n", group_name);
    // 创建者自动加入群组
    snprintf(query, sizeof(query), "INSERT INTO group_members (group_name, username) VALUES ('%s', '%s');", group_name, client->username);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "INSERT into group_members failed: %s\n", mysql_error(conn));
        return;
    }

    printf("User %s joined group %s successfully.\n", client->username, group_name);
}

// 删除群组
void delete_group(const char *group_name, client_t *client,MYSQL *conn) {
    char query[256];
    snprintf(query, sizeof(query), "SELECT owner FROM user_groups WHERE name='%s';", group_name);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "SELECT failed: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL) {
        fprintf(stderr, "mysql_store_result() failed: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    if (row == NULL || strcmp(row[0], client->username) != 0) {
        fprintf(stderr, "You are not the owner of the group %s.\n", group_name);
        mysql_free_result(res);
        return;
    }
    mysql_free_result(res);

    // 开始事务
    if (mysql_query(conn, "START TRANSACTION")) {
        fprintf(stderr, "START TRANSACTION failed: %s\n", mysql_error(conn));
        return;
    }

    // 禁用外键检查
    if (mysql_query(conn, "SET FOREIGN_KEY_CHECKS=0")) {
        fprintf(stderr, "Disabling foreign key checks failed: %s\n", mysql_error(conn));
        mysql_query(conn, "ROLLBACK");
        return;
    }

    snprintf(query, sizeof(query), "DELETE FROM group_members WHERE group_name='%s';", group_name);
    if (mysql_query(conn, query)) {
        fprintf(stderr, "DELETE from group_members failed: %s\n", mysql_error(conn));
        mysql_query(conn, "ROLLBACK");
        return;
    }

    snprintf(query, sizeof(query), "DELETE FROM user_groups WHERE name='%s';", group_name);
    if (mysql_query(conn, query)) {
        fprintf(stderr, "DELETE from user_groups failed: %s\n", mysql_error(conn));
        mysql_query(conn, "ROLLBACK");
        return;
    }

    // 启用外键检查
    if (mysql_query(conn, "SET FOREIGN_KEY_CHECKS=1")) {
        fprintf(stderr, "Enabling foreign key checks failed: %s\n", mysql_error(conn));
        mysql_query(conn, "ROLLBACK");
        return;
    }

    // 提交事务
    if (mysql_query(conn, "COMMIT")) {
        fprintf(stderr, "COMMIT failed: %s\n", mysql_error(conn));
        return;
    }

    printf("Group %s deleted successfully.\n", group_name);
}

// 查找群组
group_t *find_group_by_name(const char *group_name,MYSQL *conn) {
    char query[256];
    snprintf(query, sizeof(query), "SELECT * FROM user_groups WHERE name = '%s';", group_name);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "SELECT failed: %s\n", mysql_error(conn));
        return NULL;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL) {
        fprintf(stderr, "mysql_store_result() failed\n");
        return NULL;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    if (row == NULL) {
        return NULL;  // 群组不存在
    }

    group_t *group = malloc(sizeof(group_t));
    snprintf(group->name, sizeof(group->name), "%s", row[1]);
    mysql_free_result(res);
    return group;
}

// 加入群组
void join_group(group_t *group, client_t *client,MYSQL *conn) {
    char query[256];
    snprintf(query, sizeof(query), "INSERT INTO group_members (group_name, username) VALUES ('%s', '%s');", group->name, client->username);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "INSERT failed: %s\n", mysql_error(conn));
        return;
    }

    printf("User %s joined group %s successfully.\n", client->username, group->name);
}

// 退出群组
void leave_group(group_t *group, client_t *client,MYSQL *conn) {
    char query[256];
    snprintf(query, sizeof(query), "DELETE FROM group_members WHERE group_name = '%s' AND username = '%s';", group->name, client->username);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "DELETE failed: %s\n", mysql_error(conn));
        return;
    }

    printf("User %s left group %s successfully.\n", client->username, group->name);
}

// 发送群消息
void send_group_message(const char *group_name, const char *sender_username, const char *message,MYSQL *conn) {
    char query[256];
    snprintf(query, sizeof(query), "SELECT username FROM group_members WHERE group_name = '%s';", group_name);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "SELECT failed: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL) {
        fprintf(stderr, "mysql_store_result() failed\n");
        return;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != NULL) {
        if (strcmp(row[0], sender_username) != 0) { // 排除发送者
            client_t *client = find_client_by_username(row[0]);
            if (client != NULL) {
                char full_message[BUFFER_SIZE];
                snprintf(full_message, sizeof(full_message), "send_group_messag:%s-%s-%s", group_name, sender_username, message);
                send(client->sockfd, full_message, strlen(full_message), 0);
            }
        }
    }

    mysql_free_result(res);
}

// 邀请用户加入群组
void invite_user_to_group(const char *group_name, const char *username,MYSQL *conn) {
    char query[256];
    snprintf(query, sizeof(query), "INSERT INTO group_invitations (group_name, username) VALUES ('%s', '%s');", group_name, username);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "INSERT failed: %s\n", mysql_error(conn));
        return;
    }

    printf("User %s invited to group %s successfully.\n", username, group_name);
}

// 用户同意加入群组
void accept_group_invitation(const char *group_name, const char *username,MYSQL *conn) {
    char query[256];
    snprintf(query, sizeof(query), "INSERT INTO group_members (group_name, username) SELECT group_name, username FROM group_invitations WHERE group_name = '%s' AND username = '%s';", group_name, username);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "INSERT failed: %s\n", mysql_error(conn));
        return;
    }

    snprintf(query, sizeof(query), "DELETE FROM group_invitations WHERE group_name = '%s' AND username = '%s';", group_name, username);
    if (mysql_query(conn, query)) {
        fprintf(stderr, "DELETE failed: %s\n", mysql_error(conn));
        return;
    }

    printf("User %s accepted invitation and joined group %s successfully.\n", username, group_name);
}



void handle_invite(int client_sockfd, const char *group_name, const char *invitee,MYSQL *conn) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "You have been invited to join group %s. Do you accept? (yes/no)", group_name);
    send(client_sockfd, buffer, strlen(buffer), 0);

    int recv_size = recv(client_sockfd, buffer, sizeof(buffer), 0);
    if (recv_size <= 0) {
        // 处理接收错误或连接关闭
        fprintf(stderr, "Failed to receive response or connection closed.\n");
        return;
    }

    buffer[recv_size] = '\0';
    if (strcmp(buffer, "yes") == 0) {
        // 用户同意加入群组
        snprintf(buffer, sizeof(buffer), "User %s has joined the group %s", invitee, group_name);
        accept_group_invitation(group_name, invitee,conn);
    } else {
        snprintf(buffer, sizeof(buffer), "User %s declined the invitation to join the group %s", invitee, group_name);
    }
    send(client_sockfd, buffer, strlen(buffer), 0);
}


void list_invitations(int client_sockfd, const char *username,MYSQL *conn) {
    char query[256];
    snprintf(query, sizeof(query), "SELECT group_name, invitation_time FROM group_invitations WHERE username = '%s';", username);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "SELECT failed: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL) {
        fprintf(stderr, "mysql_store_result() failed\n");
        return;
    }

    MYSQL_ROW row;
    char buffer[BUFFER_SIZE];
    char all_invitations[1024] = "group_invitations:"; // 假设总长度不会超过1024字符

    while ((row = mysql_fetch_row(res)) != NULL) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), " %s at %s,", row[0], row[1]);
        strncat(all_invitations, buffer, sizeof(all_invitations) - strlen(all_invitations) - 1);
    }

    // 移除最后一个逗号
    size_t len = strlen(all_invitations);
    if (len > 0 && all_invitations[len - 1] == ',') {
        all_invitations[len - 1] = '\0';
    }

    send(client_sockfd, all_invitations, strlen(all_invitations), 0);
    printf("all_invitations: %s\n", all_invitations);
    mysql_free_result(res);
}

void change_group_name(const char *old_name, const char *new_name, client_t *client,MYSQL *conn) {
    char query[256];
    snprintf(query, sizeof(query), "SELECT owner FROM user_groups WHERE name='%s';", old_name);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "SELECT failed: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL) {
        fprintf(stderr, "mysql_store_result() failed: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    if (row == NULL || strcmp(row[0], client->username) != 0) {
        fprintf(stderr, "You are not the owner of the group %s.\n", old_name);
        mysql_free_result(res);
        return;
    }
    mysql_free_result(res);

    // 开始事务
    if (mysql_query(conn, "START TRANSACTION")) {
        fprintf(stderr, "START TRANSACTION failed: %s\n", mysql_error(conn));
        return;
    }

    // 禁用外键检查
    if (mysql_query(conn, "SET FOREIGN_KEY_CHECKS=0")) {
        fprintf(stderr, "Disabling foreign key checks failed: %s\n", mysql_error(conn));
        mysql_query(conn, "ROLLBACK");
        return;
    }

    snprintf(query, sizeof(query), "UPDATE user_groups SET name='%s' WHERE name='%s';", new_name, old_name);
    if (mysql_query(conn, query)) {
        fprintf(stderr, "UPDATE user_groups failed: %s\n", mysql_error(conn));
        mysql_query(conn, "ROLLBACK");
        return;
    }

    snprintf(query, sizeof(query), "UPDATE group_members SET group_name='%s' WHERE group_name='%s';", new_name, old_name);
    if (mysql_query(conn, query)) {
        fprintf(stderr, "UPDATE group_members failed: %s\n", mysql_error(conn));
        mysql_query(conn, "ROLLBACK");
        return;
    }

    // 启用外键检查
    if (mysql_query(conn, "SET FOREIGN_KEY_CHECKS=1")) {
        fprintf(stderr, "Enabling foreign key checks failed: %s\n", mysql_error(conn));
        mysql_query(conn, "ROLLBACK");
        return;
    }

    // 提交事务
    if (mysql_query(conn, "COMMIT")) {
        fprintf(stderr, "COMMIT failed: %s\n", mysql_error(conn));
        return;
    }

    printf("Group name changed from %s to %s successfully.\n", old_name, new_name);
}
// 从群组中删除成员
void remove_member_from_group(const char *group_name, const char *member_username, client_t *client,MYSQL *conn) {
    char query[256];
    snprintf(query, sizeof(query), "SELECT owner FROM user_groups WHERE name='%s';", group_name);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "SELECT failed: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL) {
        fprintf(stderr, "mysql_store_result() failed: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    if (row == NULL || strcmp(row[0], client->username) != 0) {
        fprintf(stderr, "You are not the owner of the group %s.\n", group_name);
        mysql_free_result(res);
        return;
    }
    mysql_free_result(res);

    snprintf(query, sizeof(query), "DELETE FROM group_members WHERE group_name='%s' AND username='%s';", group_name, member_username);
    if (mysql_query(conn, query)) {
        fprintf(stderr, "DELETE failed: %s\n", mysql_error(conn));
        return;
    }

    printf("User %s removed from group %s successfully.\n", member_username, group_name);
}