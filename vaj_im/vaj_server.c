#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <mysql/mysql.h>
#include <iconv.h>
#include <errno.h>

#include "client.h"
#include "group.h"
#include "friend.h"
#include "user.h"

#define MYSQL_HOST "localhost"
#define MYSQL_USER "root"
#define MYSQL_PASS ""
#define MYSQL_DB "VAJIM"

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1536

typedef struct {
    int sockfd;
    MYSQL *conn;
} thread_params_t;

// 客户端列表
client_t *clients[MAX_CLIENTS];

// 锁定客户端列表
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// MySQL 连接配置
//MYSQL *conn;
//MYSQL_RES *res;
//MYSQL_ROW row;

// 查找客户端
client_t *find_client_by_username(const char *username) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != NULL) {
            printf("Checking client %d: %s\n", i, clients[i]->username);
            if (strcmp(clients[i]->username, username) == 0) {
                printf("Found user %s at index %d\n", username, i);
                pthread_mutex_unlock(&clients_mutex);
                return clients[i];
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    printf("User %s not found\n", username);
    return NULL;
}

// 声明 authenticate_user 函数
int authenticate_user(const char *username, const char *password,MYSQL *conn) {
    char query[256];
    snprintf(query, sizeof(query), "SELECT * FROM users WHERE username='%s' AND password=SHA2('%s', 256);", username, password);

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

// 数据库连接初始化
void init_mysql(MYSQL **conn) {
    *conn = mysql_init(NULL);
    if (*conn == NULL) {
        fprintf(stderr, "mysql_init() failed\n");
        exit(EXIT_FAILURE);
    }

    if (mysql_real_connect(*conn, "localhost", "root", "", "VAJIM", 3306, NULL, 0) == NULL) {
        fprintf(stderr, "mysql_real_connect() failed :ERROR: %s\n", mysql_error(conn));
        mysql_close(conn);
        exit(EXIT_FAILURE);
    }
}

void *handle_client(void *arg) {
    char buffer[BUFFER_SIZE];
    int client_sockfd = ((client_t*)arg)->sockfd;
    char *username = ((client_t*)arg)->username;
    int recv_size;
    char username_input[50]; 
    char password_input[50];
    MYSQL *conn=((client_t*)arg)->conn;
    int login_attempts = 0; // 记录登录尝试次数

    

    while (1) {


        recv_size = recv(client_sockfd, buffer, sizeof(buffer), 0);
        if (recv_size < 0) {
            perror("recv failed for credentials");
            close(client_sockfd);
            free(arg);
            return NULL;
        }
        buffer[recv_size] = '\0'; // 确保接收到的数据以 '\0' 结尾

        
        // 分离用户名和密码
        char *command = strtok(buffer, "\n");
        char *username_ptr = strtok(NULL, "\n");
        char *password_ptr = strtok(NULL, "\n");

        if (command == NULL || username_ptr == NULL || password_ptr == NULL) {
            char *error_msg = "Invalid input format";
            send(client_sockfd, error_msg, strlen(error_msg), 0);
            continue;
        }

        if (strcmp(command, "login") == 0) {
            // 验证用户登录
            if (!authenticate_user(username_ptr, password_ptr,conn)) {
                char *error_msg = "Invalid username or password";
                send(client_sockfd, error_msg, strlen(error_msg), 0);
                printf("Failed login attempt for username: %s\n", username_ptr);
                login_attempts++; // 增加登录尝试次数

                if (login_attempts >= 3) {
                    printf("User %s failed to login after 3 attempts. Closing connection.\n", username_ptr);
                    close(client_sockfd);
                    free(arg);
                    return NULL; // 退出函数，关闭连接
                }
            } else {
                snprintf(username, sizeof(username), "%s", username_ptr);
                printf("User %s logged in successfully.\n", username);

                char login_success_msg[BUFFER_SIZE];
                snprintf(login_success_msg, sizeof(login_success_msg), "Login successful. Welcome, %s!!", username);
                send(client_sockfd, login_success_msg, strlen(login_success_msg), 0);
                // 在用户登录成功后更新好友状态并通知好友
                update_friend_status(username, "online",conn);
                notify_friends_status(username, "online",conn);
                // 用户验证成功后，发送离线消息
                send_offline_messages(client_sockfd, username,conn);

                break; // 登录成功，退出循环
            }
        } else if (strcmp(command, "register") == 0) {
            // 注册新用户
            if (user_exists(username_ptr,conn)) {
                char *error_msg = "Username already exists";
                send(client_sockfd, error_msg, strlen(error_msg), 0);
                printf("Registration attempt with existing username: %s\n", username_ptr);
            } else {
                if (register_user(username_ptr, password_ptr,conn)) {
                    char *success_msg = "Registration successful";
                    send(client_sockfd, success_msg, strlen(success_msg), 0);
                    printf("User %s registered successfully.\n", username_ptr);
                } else {
                    char *error_msg = "Registration failed";
                    send(client_sockfd, error_msg, strlen(error_msg), 0);
                    printf("Failed to register user: %s\n", username_ptr);
                }
            }
        } else {
            char *error_msg = "Unknown command";
            send(client_sockfd, error_msg, strlen(error_msg), 0);
        }
    }
    // 添加到客户端列表
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == NULL) {
            clients[i] = (client_t*)arg;
            printf("Added user %s to clients at index %d\n", username, i);
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    // 继续处理消息收发...
    while (1) {
        int recv_size = recv(client_sockfd, buffer, sizeof(buffer), 0);
        if (recv_size <= 0) {
            // 客户端断开连接
            printf("Client %s disconnected.\n", username);
            break;
        }

        buffer[recv_size] = '\0'; // 确保字符串以 '\0' 结尾
        printf("Received message from %s: %s\n", username, buffer);
        
        // 将接收到的消息转换为 UTF-8 编码
        char temp_buffer[BUFFER_SIZE];
        size_t in_size = recv_size;
        size_t out_size = sizeof(temp_buffer);
        char *in_buf = buffer;
        char *out_buf = temp_buffer;

        iconv_t cd = iconv_open("UTF-8", "ISO-8859-1");
        if (cd == (iconv_t)-1) {
            perror("iconv_open");
            // 处理错误
        }

        if (iconv(cd, &in_buf, &in_size, &out_buf, &out_size) == (size_t)-1) {
            perror("iconv");
            // 处理错误
        }

        iconv_close(cd);

        // 确保 temp_buffer 以 '\0' 结尾
        *out_buf = '\0';

        // 将转换后的结果复制回 buffer
        strncpy(buffer, temp_buffer, sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';

        printf("Received message from %s: %s\n", username, buffer);

        char *command = strtok(buffer, " ");
        char *target = strtok(NULL, " ");
        char *message = strtok(NULL, "");

        if (strcmp(command, "exit") == 0) {
            char *exit_msg = "You have exited the chat.";
            send(client_sockfd, exit_msg, strlen(exit_msg), 0);
            printf("User %s has exited the chat.\n", username);
            break;  // 退出接收消息循环
        } else if (strcmp(command, "logout") == 0) {
            char *logout_msg = "You have been logged out.";
            send(client_sockfd, logout_msg, strlen(logout_msg), 0);
            printf("User %s has logged out.\n", username);
            // 在用户退出时更新好友状态并通知好友
            update_friend_status(username, "offline",conn);
            notify_friends_status(username, "offline",conn);
            break;  // 退出接收消息循环
        } else if (strcmp(command, "create_group") == 0) {
            if (target) {
                create_group(target, (client_t*)arg,conn);
                char response[BUFFER_SIZE];
                snprintf(response, sizeof(response), "create_group:%s", target);
                send(client_sockfd, response, strlen(response), 0);
                printf("Group %s created by %s.\n", target, username);
            }
        }else if (strcmp(command, "change_group_name") == 0) {
            if (target && message) {
                change_group_name(target, message, (client_t*)arg,conn);
                char response[BUFFER_SIZE];
                snprintf(response, sizeof(response), "change_group_name: %s to %s ", target, message);
                send(client_sockfd, response, strlen(response), 0);
                printf("Group name changed from %s to %s by %s.\n", target, message, username);
            }
        } else if (strcmp(command, "remove_member") == 0) {
            if (target && message) {
                remove_member_from_group(target, message, (client_t*)arg,conn);
                char response[BUFFER_SIZE];
                snprintf(response, sizeof(response), "User %s removed from group %s successfully.", message, target);
                send(client_sockfd, response, strlen(response), 0);
                printf("User %s removed from group %s by %s.\n", message, target, username);

                // 额外发送一个消息给被删除的用户
                client_t *removed_user = find_client_by_username(message);
                if (removed_user) {
                    char notify_msg[BUFFER_SIZE];
                    snprintf(notify_msg, sizeof(notify_msg), "remove_group_member:%s", target);
                    send(removed_user->sockfd, notify_msg, strlen(notify_msg), 0);
                }
            }
        } else if (strcmp(command, "join_group") == 0) {
            if (target) {
                group_t *group = find_group_by_name(target,conn);
                if (group) {
                    join_group(group, (client_t*)arg,conn);
                    char response[BUFFER_SIZE];
                    snprintf(response, sizeof(response), "Joined group %s successfully.", target);
                    send(client_sockfd, response, strlen(response), 0);
                    printf("User %s joined group %s.\n", username, target);
                } else {
                    char *error_msg = "Group not found";
                    send(client_sockfd, error_msg, strlen(error_msg), 0);
                    printf("Group %s not found for user %s.\n", target, username);
                }
            }
        } else if (strcmp(command, "leave_group") == 0) {
            if (target) {
                group_t *group = find_group_by_name(target,conn);
                if (group) {
                    leave_group(group, (client_t*)arg,conn);
                    char response[BUFFER_SIZE];
                    snprintf(response, sizeof(response), "Left group %s successfully.", target);
                    send(client_sockfd, response, strlen(response), 0);
                    printf("User %s left group %s.\n", username, target);
                } else {
                    char *error_msg = "Group not found";
                    send(client_sockfd, error_msg, strlen(error_msg), 0);
                    printf("Group %s not found for user %s.\n", target, username);
                }
            }
        } else if (strcmp(command, "delete_group") == 0) {
            if (target) {
                delete_group(target, (client_t*)arg,conn);
                char response[BUFFER_SIZE];
                snprintf(response, sizeof(response), "delete_group:%s", target);
                send(client_sockfd, response, strlen(response), 0);
                printf("Group %s deleted by %s.\n", target, username);
            }
        } else if (strcmp(command, "group_message") == 0) {
            if (target && message) {
                send_group_message(target, username, message,conn);
                char response[BUFFER_SIZE];
                snprintf(response, sizeof(response), "group_message: %s successfully.", target);
                send(client_sockfd, response, strlen(response), 0);
                printf("User %s sent message to group %s.\n", username, target);
            }
        } else if (strcmp(command, "invite_user") == 0) {
            if (target && message) {
                if (user_exists(message,conn)) {
                        invite_user_to_group(target,message,conn);
                        char response[BUFFER_SIZE];
                        snprintf(response, sizeof(response), "User %s invited to group %s successfully add to the database.", message, target);
                        send(client_sockfd, response, strlen(response), 0);
                } else {
                        char error_msg[BUFFER_SIZE];
                        snprintf(error_msg, sizeof(error_msg), "User %s not found.", message);
                        send(client_sockfd, error_msg, strlen(error_msg), 0);
                }
                 
            }else {
                    char error_msg[BUFFER_SIZE];
                    snprintf(error_msg, sizeof(error_msg), "Invalid invite_user command format.");
                    send(client_sockfd, error_msg, strlen(error_msg), 0);
            }
        } else if (strcmp(command, "list_group_invitations") == 0) {
            list_invitations(client_sockfd, username,conn);
        } else if (strcmp(command, "handle_invitation") == 0) {
            if (target && message) {
                if (strcmp(message, "accept") == 0) {
                    accept_group_invitation(target, username,conn);
                    char response[BUFFER_SIZE];
                    snprintf(response, sizeof(response), "You have accepted the invitation to join group %s.", target);
                    //send(client_sockfd, response, strlen(response), 0);
                } else if (strcmp(message, "decline") == 0) {
                    char query[256];
                    snprintf(query, sizeof(query), "DELETE FROM group_invitations WHERE group_name = '%s' AND username = '%s';", target, username);
                    if (mysql_query(conn, query)) {
                        fprintf(stderr, "DELETE failed: %s\n", mysql_error(conn));
                        return NULL;
                    }
                    char response[BUFFER_SIZE];
                    snprintf(response, sizeof(response), "You have declined the invitation to join group %s.", target);
                    send(client_sockfd, response, strlen(response), 0);
                } else {
                    char error_msg[BUFFER_SIZE];
                    snprintf(error_msg, sizeof(error_msg), "Invalid handle_invitation command format.");
                    send(client_sockfd, error_msg, strlen(error_msg), 0);
                }
            } else {
                char error_msg[BUFFER_SIZE];
                snprintf(error_msg, sizeof(error_msg), "Invalid handle_invitation command format.");
                send(client_sockfd, error_msg, strlen(error_msg), 0);
            }
        }else if (strcmp(command, "add_friend") == 0) {
            if (target) {
                if (user_exists(target,conn)) {
                    add_friend_request(username, target,conn);
                    char response[BUFFER_SIZE];
                    snprintf(response, sizeof(response), "Friend request sent to %s successfully.", target);
                    send(client_sockfd, response, strlen(response), 0);
                } else {
                    char error_msg[BUFFER_SIZE];
                    snprintf(error_msg, sizeof(error_msg), "User %s not found.", target);
                    send(client_sockfd, error_msg, strlen(error_msg), 0);
                }
            } else {
                char error_msg[BUFFER_SIZE];
                snprintf(error_msg, sizeof(error_msg), "Invalid add_friend command format.");
                send(client_sockfd, error_msg, strlen(error_msg), 0);
            }
        } else if (strcmp(command, "remove_friend") == 0) {
            if (target) {
                if (is_friend(username, target,conn)) {
                    remove_friend(username, target,conn);
                    char response[BUFFER_SIZE];
                    snprintf(response, sizeof(response), "User %s removed from friends successfully.", target);
                    send(client_sockfd, response, strlen(response), 0);
                } else {
                    char error_msg[BUFFER_SIZE];
                    snprintf(error_msg, sizeof(error_msg), "User %s is not your friend.", target);
                    send(client_sockfd, error_msg, strlen(error_msg), 0);
                }
            } else {
                char error_msg[BUFFER_SIZE];
                snprintf(error_msg, sizeof(error_msg), "Invalid remove_friend command format.");
                send(client_sockfd, error_msg, strlen(error_msg), 0);
            }
        }else if (strcmp(command, "list_friend_requests") == 0) {
            list_friend_requests(client_sockfd, username,conn);
        } else if (strcmp(command, "handle_friend_request") == 0) {
            if (target && message) {
                handle_friend_request(client_sockfd, username, target, message,conn);
            } else {
                char error_msg[BUFFER_SIZE];
                snprintf(error_msg, sizeof(error_msg), "Invalid handle_friend_request command format.");
                send(client_sockfd, error_msg, strlen(error_msg), 0);
            }
        }else if (strcmp(command, "private_message") == 0) {
            if (target && message) {
                send_private_message(client_sockfd, username, target, message,conn);
            } else {
                char error_msg[BUFFER_SIZE];
                snprintf(error_msg, sizeof(error_msg), "Invalid send_private_message command format.");
                send(client_sockfd, error_msg, strlen(error_msg), 0);
            }
        }else if (strcmp(command, "add_friend_nickname") == 0) {
            if (target && message) {
                add_friend_nickname(username, target, message,conn);
                char response[BUFFER_SIZE];
                snprintf(response, sizeof(response), "Nickname for friend %s added successfully.", target);
                send(client_sockfd, response, strlen(response), 0);
            } else {
                char error_msg[BUFFER_SIZE];
                snprintf(error_msg, sizeof(error_msg), "Invalid add_friend_nickname command format.");
                send(client_sockfd, error_msg, strlen(error_msg), 0);
            }
        } else if (strcmp(command, "update_friend_nickname") == 0) {
            if (target && message) {
                update_friend_nickname(username, target, message,conn);
                char response[BUFFER_SIZE];
                snprintf(response, sizeof(response), "Nickname for friend %s updated successfully.", target);
                send(client_sockfd, response, strlen(response), 0);
            } else {
                char error_msg[BUFFER_SIZE];
                snprintf(error_msg, sizeof(error_msg), "Invalid update_friend_nickname command format.");
                send(client_sockfd, error_msg, strlen(error_msg), 0);
            }
        } else if (strcmp(command, "get_friend_list") == 0) {
            char friend_list_with_status[BUFFER_SIZE];
            get_friend_list_with_status(username, friend_list_with_status, sizeof(friend_list_with_status),conn);
            
            // 将换行符替换为逗号
            for (int i = 0; friend_list_with_status[i] != '\0'; i++) {
                if (friend_list_with_status[i] == '\n') {
                    friend_list_with_status[i] = ',';
                }
            }
            char message[BUFFER_SIZE];
            snprintf(message, sizeof(message), "get_friend_list\n%s", friend_list_with_status);

            printf("%s", message);
            send(client_sockfd, message, strlen(message), 0);
            printf("Sent friend list to %s.\n", username);
        } else if (strcmp(command, "get_group_list") == 0) {
            char group_list[BUFFER_SIZE];
            get_group_list(username, group_list, sizeof(group_list),conn);
            
            // 将换行符替换为逗号
            for (int i = 0; group_list[i] != '\0'; i++) {
                if (group_list[i] == '\n') {
                    group_list[i] = ',';
                }
            }

            char message[BUFFER_SIZE];
            snprintf(message, sizeof(message), "get_group_list\n%s", group_list);

            printf("%s", message);
            send(client_sockfd, message, strlen(message), 0);
            printf("Sent group list to %s.\n", username);
        } else if (strcmp(command, "send_file") == 0) {
            if (target && message) {
                // 查找接收方的客户端
                char *file_size_str = strtok(NULL, " ");
                if (file_size_str == NULL) {
                    char error_msg[BUFFER_SIZE];
                    snprintf(error_msg, sizeof(error_msg), "Invalid send_file command format.");
                    send(client_sockfd, error_msg, strlen(error_msg), 0);
                    continue;
                }
                long file_size = atol(file_size_str);

                client_t *receiver_client = find_client_by_username(target);
                if (receiver_client != NULL) {
                    int receiver_sockfd = receiver_client->sockfd;

                    // 转发文件信息给接收方
                    char file_info[BUFFER_SIZE];
                    snprintf(file_info, sizeof(file_info), "FILE %s %ld", message, file_size);
                    send(receiver_sockfd, file_info, strlen(file_info), 0);

                    // 接收并转发文件数据
                    while ((recv_size = recv(client_sockfd, buffer, sizeof(buffer), 0)) > 0) {
                        send(receiver_sockfd, buffer, recv_size, 0);
                        if (recv_size < sizeof(buffer)) {
                            break;
                        }
                    }

                    printf("File %s sent to %s successfully.\n", message, target);
                } else {
                    char error_msg[BUFFER_SIZE];
                    snprintf(error_msg, sizeof(error_msg), "User %s not found.", target);
                    send(client_sockfd, error_msg, strlen(error_msg), 0);
                }
            } else {
                char error_msg[BUFFER_SIZE];
                snprintf(error_msg, sizeof(error_msg), "Invalid send_file command format.");
                send(client_sockfd, error_msg, strlen(error_msg), 0);
            }
        } else if (strcmp(command, "logout") == 0) {
            char *logout_msg = "You have been logged out.";
            send(client_sockfd, logout_msg, strlen(logout_msg), 0);
            printf("User %s has logged out.\n", username);
            // 在用户退出时更新好友状态并通知好友
            break;  // 退出接收消息循环
        }
    }
    update_friend_status(username, "offline",conn);
    notify_friends_status(username, "offline",conn);
    // 从客户端列表中移除
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != NULL && clients[i]->sockfd == client_sockfd) {
            clients[i] = NULL;
            break;
        }
    }


    pthread_mutex_unlock(&clients_mutex);
    mysql_close(conn);
    close(client_sockfd);
    free(arg);
    return NULL;
}

int main() {
    int sockfd, newsockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t clilen;
    pthread_t tid;

    // 初始化 MySQL 连接
    //init_mysql();

    // 创建套接字
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(12345); // 替换为服务器的端口号

    // 绑定套接字
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Socket bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // 监听连接
    if (listen(sockfd, 5) < 0) {
        perror("Socket listen failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port 12345\n");

    while (1) {
        clilen = sizeof(client_addr);
        newsockfd = accept(sockfd, (struct sockaddr*)&client_addr, &clilen);
        if (newsockfd < 0) {
            perror("Socket accept failed");
            continue;
        }

        // 为每个客户端创建独立的MySQL连接
        MYSQL *conn;
        init_mysql(&conn);


        // 分配客户端结构
        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->sockfd = newsockfd;
        cli->address = client_addr;
        cli->conn = conn;

        // 创建处理客户端的线程
        if (pthread_create(&tid, NULL, handle_client, (void*)cli) != 0) {
            perror("Failed to create thread");
            free(cli);
        }
    }

    // 关闭套接字
    close(sockfd);

    return 0;
}