#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h> 
#include <glib.h>
#include <X11/Xlib.h>

#define BUFFER_SIZE 1536

typedef struct {
    char *data;
} UpdateData;

int sockfd;
GtkWidget *username_entry, *password_entry, *confirm_password_entry, *status_label;
GtkWidget *window, *grid;
GtkWidget *friend_list, *group_list, *message_list;
GtkWidget *friend_list_text_view;
GtkWidget *group_list_text_view;

char *global_friend_requests_data = NULL;
char *global_group_invitations_data = NULL;
char current_username[BUFFER_SIZE];

// 全局变量存储私聊窗口
GHashTable *private_chat_windows = NULL;
GHashTable *group_chat_windows = NULL;

// 声明 on_back_button_clicked 函数
void on_back_button_clicked(GtkWidget *widget, gpointer data);

void display_group_invitations(const char *group_invitations_data);
void display_friend_requests(const char *friend_requests_data);
// 声明新函数
void *receive_server_messages(void *arg);
void show_main_interface(const char *username);
void on_logout_button_clicked(GtkWidget *widget, gpointer data);
void update_friend_list(const char *friends);
void update_group_list(const char *groups);
gboolean update_group_list_idle(gpointer user_data);
gboolean update_friend_list_idle(gpointer user_data);
void on_view_friend_requests_button_clicked(GtkWidget *widget, gpointer data);
void on_accept_friend_request_clicked(GtkWidget *widget, gpointer data);
void on_reject_friend_request_clicked(GtkWidget *widget, gpointer data);
void on_edit_nickname_button_clicked(GtkWidget *widget, gpointer data);
const char* extract_real_username(const char *display_name);
void on_manage_group_button_clicked(GtkWidget *widget, gpointer data);
void on_view_group_requests_button_clicked(GtkWidget *widget, gpointer data);
void on_invite_to_group_button_clicked(GtkWidget *widget, gpointer data);
void on_accept_group_invitation_clicked(GtkWidget *widget, gpointer data);
void on_reject_group_invitation_clicked(GtkWidget *widget, gpointer data);
void on_remove_from_group_button_clicked(GtkWidget *widget, gpointer data);
void on_manage_group_members_button_clicked(GtkWidget *widget, gpointer data);
void on_send_private_message_button_clicked(GtkWidget *widget, gpointer data);
void on_send_group_message_button_clicked(GtkWidget *widget, gpointer data);
void on_edit_group_name_button_clicked(GtkWidget *widget, gpointer data);
void clear_interface();
void reset_state();
void show_login_window();

void send_file(const char *target, const char *file_path);
void on_send_file_button_clicked(GtkWidget *widget, gpointer data);
void create_chat_window(const char *target);

void on_user_clicked(GtkWidget *widget, gpointer data);
// 其他函数声明...
void send_command(int sockfd, const char *command, const char *target, const char *message);
void connect_to_server(const char *server_ip);
void on_login_button_clicked(GtkWidget *widget, gpointer data);
void on_submit_button_clicked(GtkWidget *widget, gpointer data);
void on_register_button_clicked(GtkWidget *widget, gpointer data);

void send_command(int sockfd, const char *command, const char *target, const char *message) {
    char buffer[BUFFER_SIZE];
    if (message && target) {
        snprintf(buffer, sizeof(buffer), "%s %s %s", command, target, message);
    } else if (target) {
        snprintf(buffer, sizeof(buffer), "%s %s", command, target);
    }else{
        snprintf(buffer, sizeof(buffer), "%s", command);

    }
    // 确保字符串以'\0'结尾
    buffer[sizeof(buffer) - 1] = '\0';

    send(sockfd, buffer, strlen(buffer), 0);
}



void connect_to_server(const char *server_ip) {
    struct sockaddr_in server_addr;

    // 创建套接字
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(12345); // 替换为服务器的端口号
    server_addr.sin_addr.s_addr = inet_addr(server_ip); // 替换为服务器的IP地址

    // 连接到服务器
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection to server failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    } else {
        printf("Connected to server successfully.\n");
    }
}

void on_send_file_button_clicked(GtkWidget *widget, gpointer data) {
    const char *target = (const char *)data;

    // 创建文件选择对话框
    GtkWidget *file_chooser = gtk_file_chooser_dialog_new("Select File",
                                                          GTK_WINDOW(window),
                                                          GTK_FILE_CHOOSER_ACTION_OPEN,
                                                          "_Cancel", GTK_RESPONSE_CANCEL,
                                                          "_Open", GTK_RESPONSE_ACCEPT,
                                                          NULL);

    if (gtk_dialog_run(GTK_DIALOG(file_chooser)) == GTK_RESPONSE_ACCEPT) {
        char *file_path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_chooser));
        if (file_path) {
            send_file(target, file_path);
            g_free(file_path);
        }
    }

    gtk_widget_destroy(file_chooser);
}

// 在创建聊天窗口时添加文件传输按钮
void create_chat_window(const char *target) {
    GtkWidget *dialog, *content_area, *scrolled_window, *text_view, *entry, *send_button, *file_button, *file_chooser;
    GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;

    dialog = gtk_dialog_new_with_buttons("Chat",
                                         GTK_WINDOW(window),
                                         flags,
                                         "_Close",
                                         GTK_RESPONSE_CLOSE,
                                         NULL);

    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    // 创建一个滚动窗口来显示聊天记录
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(content_area), scrolled_window);

    // 创建一个文本视图来显示聊天记录
    text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);

    // 创建一个输入框来输入消息
    entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Type your message here...");
    gtk_container_add(GTK_CONTAINER(content_area), entry);

    // 创建一个发送按钮
    send_button = gtk_button_new_with_label("Send");
    gtk_container_add(GTK_CONTAINER(content_area), send_button);

    // 创建一个文件选择按钮
    file_button = gtk_button_new_with_label("Send File");
    gtk_container_add(GTK_CONTAINER(content_area), file_button);

    // 创建一个文件选择器
    file_chooser = gtk_file_chooser_dialog_new("Select File",
                                               GTK_WINDOW(dialog),
                                               GTK_FILE_CHOOSER_ACTION_OPEN,
                                               "_Cancel", GTK_RESPONSE_CANCEL,
                                               "_Open", GTK_RESPONSE_ACCEPT,
                                               NULL);

    // 将目标和文件选择器作为参数传递给回调函数
    GtkWidget **data_array = g_new(GtkWidget *, 2);
    data_array[0] = (GtkWidget *)target;
    data_array[1] = file_chooser;

    g_signal_connect(send_button, "clicked", G_CALLBACK(on_send_private_message_button_clicked), data_array);
    g_signal_connect(file_button, "clicked", G_CALLBACK(on_send_file_button_clicked), data_array);

    gtk_widget_show_all(dialog);
    
    // 不再调用 gtk_dialog_run，而是监听关闭信号来销毁窗口
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);

    // 存储窗口和文本视图到全局变量中
    store_private_chat_window(target, dialog, text_view);
}

void send_file(const char *target, const char *file_path) {
    FILE *file = fopen(file_path, "rb");
    if (file == NULL) {
        perror("Failed to open file");
        return;
    }

    // 获取文件大小
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // 发送文件信息
    char file_info[BUFFER_SIZE];
    snprintf(file_info, sizeof(file_info), "send_file %s %s %ld", target, file_path, file_size);
    send(sockfd, file_info, strlen(file_info), 0);

    // 发送文件数据
    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        send(sockfd, buffer, bytes_read, 0);
    }

    fclose(file);
    printf("File %s sent to %s successfully.\n", file_path, target);
}

void on_login_button_clicked(GtkWidget *widget, gpointer data) {
    const char *username = gtk_entry_get_text(GTK_ENTRY(username_entry));
    const char *password = gtk_entry_get_text(GTK_ENTRY(password_entry));

    // 发送用户名和密码到服务器进行验证
    // 发送用户名和密码到服务器进行验证
    char credentials[BUFFER_SIZE];
    snprintf(credentials, sizeof(credentials), "login\n%s\n%s", username, password);
    send(sockfd, credentials, strlen(credentials), 0);

    // 接收登录结果
    char buffer[BUFFER_SIZE];
    int recv_size = recv(sockfd, buffer, sizeof(buffer), 0);
    printf("Received message from %s: %s\n", username, buffer);
    if (recv_size > 0) {
        buffer[recv_size] = '\0'; // 确保字符串以'\0'结尾

        // 检查登录结果
        if (strcmp(buffer, "Invalid username or password") != 0) {
            gtk_label_set_text(GTK_LABEL(status_label), "Login successful!");
            strncpy(current_username, username, BUFFER_SIZE - 1);
            current_username[BUFFER_SIZE - 1] = '\0';
            show_main_interface(username); // 显示主界面

        } else {
            gtk_label_set_text(GTK_LABEL(status_label), "Login failed! Invalid credentials.");
        }
    } else {
        gtk_label_set_text(GTK_LABEL(status_label), "Failed to receive response from server.");
    }
}

void on_submit_button_clicked(GtkWidget *widget, gpointer data) {
    const char *username = gtk_entry_get_text(GTK_ENTRY(username_entry));
    const char *password = gtk_entry_get_text(GTK_ENTRY(password_entry));
    const char *confirm_password = gtk_entry_get_text(GTK_ENTRY(confirm_password_entry));

    if (strcmp(password, confirm_password) != 0) {
        gtk_label_set_text(GTK_LABEL(status_label), "Passwords do not match!");
        gtk_widget_show_all(widget);  // 确保界面刷新
        return;
    }

    // 发送用户名和密码到服务器进行注册
    char credentials[BUFFER_SIZE];
    snprintf(credentials, sizeof(credentials), "register\n%s\n%s", username, password);
    send(sockfd, credentials, strlen(credentials), 0);

    // 接收注册结果
    char buffer[BUFFER_SIZE];
    int recv_size = recv(sockfd, buffer, sizeof(buffer), 0);
    if (recv_size > 0) {
        buffer[recv_size] = '\0'; // 确保字符串以'\0'结尾

        // 检查注册结果
        if (strcmp(buffer, "Registration successful") == 0) {
            gtk_label_set_text(GTK_LABEL(status_label), "Registration successful! You can now login.");
        } else {
            gtk_label_set_text(GTK_LABEL(status_label), "Registration failed!");
        }
    } else {
        gtk_label_set_text(GTK_LABEL(status_label), "Failed to receive response from server.");
    }

    // 强制刷新界面
    gtk_widget_show_all(GTK_WIDGET(widget));  // 刷新界面，确保所有控件都可见
}



gboolean update_friend_list_idle(gpointer user_data) {
    UpdateData *update_data = (UpdateData *)user_data;
    update_friend_list(update_data->data);
    // 删除 g_free(update_data->data); 保证不立即释放数据
    g_free(update_data); // 只释放 UpdateData 结构体
    return FALSE; // 只执行一次
}

gboolean update_group_list_idle(gpointer user_data) {
    UpdateData *update_data = (UpdateData *)user_data;
    update_group_list(update_data->data);
    // 删除 g_free(update_data->data); 保证不立即释放数据
    g_free(update_data); // 只释放 UpdateData 结构体
    return FALSE; // 只执行一次
}


void on_register_button_clicked(GtkWidget *widget, gpointer data) {
    // 切换到注册界面
    GtkWidget *window = gtk_widget_get_toplevel(widget);
    GList *children, *iter;

    // 移除现有的所有子组件
    children = gtk_container_get_children(GTK_CONTAINER(window));
    for (iter = children; iter != NULL; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);

    // 创建新的grid布局
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
    gtk_container_add(GTK_CONTAINER(window), grid);

    // 创建注册所需的输入框
    username_entry = gtk_entry_new();
    password_entry = gtk_entry_new();
    confirm_password_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(username_entry), "Username");
    gtk_entry_set_placeholder_text(GTK_ENTRY(password_entry), "Password");
    gtk_entry_set_placeholder_text(GTK_ENTRY(confirm_password_entry), "Confirm Password");
    gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);
    gtk_entry_set_visibility(GTK_ENTRY(confirm_password_entry), FALSE);

    GtkWidget *submit_button = gtk_button_new_with_label("Submit");
    GtkWidget *back_button = gtk_button_new_with_label("Back");

    // 设置输入框的固定宽度
    int input_width = 320;
    gtk_widget_set_size_request(username_entry, input_width, -1);
    gtk_widget_set_size_request(password_entry, input_width, -1);
    gtk_widget_set_size_request(confirm_password_entry, input_width, -1);

    // 创建状态标签
    status_label = gtk_label_new("");

    // 将组件添加到grid布局
    gtk_grid_attach(GTK_GRID(grid), username_entry, 0, 0, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), password_entry, 0, 1, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), confirm_password_entry, 0, 2, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), submit_button, 0, 3, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), back_button, 0, 4, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), status_label, 0, 5, 2, 1);

    // 连接按钮的点击事件
    g_signal_connect(submit_button, "clicked", G_CALLBACK(on_submit_button_clicked), NULL);

    // 返回按钮
    g_signal_connect(back_button, "clicked", G_CALLBACK(on_back_button_clicked), NULL);

    // 刷新界面
    gtk_widget_show_all(window);
}

void on_add_friend_button_clicked(GtkWidget *widget, gpointer data) {
    const char *friend_username = gtk_entry_get_text(GTK_ENTRY(data));
    if (strlen(friend_username) > 0) {
        send_command(sockfd, "add_friend", friend_username, NULL);
    }
    gtk_widget_destroy(gtk_widget_get_toplevel(widget)); // 关闭对话框
}

void on_remove_friend_button_clicked(GtkWidget *widget, gpointer data) {
    const char *friend_username = gtk_entry_get_text(GTK_ENTRY(data));
    if (strlen(friend_username) > 0) {
        send_command(sockfd, "remove_friend", friend_username, NULL);
        sleep(1);
        // 发送请求刷新好友列表
        send_command(sockfd, "get_friend_list", NULL, NULL);
    }
    gtk_widget_destroy(gtk_widget_get_toplevel(widget)); // 关闭对话框
}

void on_manage_friend_button_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog, *content_area;
    GtkWidget *entry, *add_button, *remove_button;
    GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;

    dialog = gtk_dialog_new_with_buttons("Manage Friends",
                                         GTK_WINDOW(window),
                                         flags,
                                         NULL);

    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Friend Username");

    add_button = gtk_button_new_with_label("Add Friend");
    remove_button = gtk_button_new_with_label("Remove Friend");

    gtk_container_add(GTK_CONTAINER(content_area), entry);
    gtk_container_add(GTK_CONTAINER(content_area), add_button);
    gtk_container_add(GTK_CONTAINER(content_area), remove_button);

    g_signal_connect(add_button, "clicked", G_CALLBACK(on_add_friend_button_clicked), entry);
    g_signal_connect(remove_button, "clicked", G_CALLBACK(on_remove_friend_button_clicked), entry);

    gtk_widget_show_all(dialog);
}
void on_back_button_clicked(GtkWidget *widget, gpointer data) {
    // 切换回登录界面
    GList *children, *iter;

    // 移除现有的所有子组件
    children = gtk_container_get_children(GTK_CONTAINER(window));
    for (iter = children; iter != NULL; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);

    // 重新创建登录界面
    grid = gtk_grid_new();
    gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
    gtk_container_add(GTK_CONTAINER(window), grid);

    username_entry = gtk_entry_new();
    password_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(username_entry), "Username");
    gtk_entry_set_placeholder_text(GTK_ENTRY(password_entry), "Password");
    gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE); // 隐藏密码

    // 设置输入框的宽度为固定值，避免使用动态获取窗口宽度
    int input_width = 320; // 设置固定宽度
    gtk_widget_set_size_request(username_entry, input_width, -1);
    gtk_widget_set_size_request(password_entry, input_width, -1);

    // 创建按钮
    GtkWidget *login_button = gtk_button_new_with_label("Login");
    GtkWidget *register_button = gtk_button_new_with_label("Register");

    // 创建状态标签
    status_label = gtk_label_new("");

    // 将组件添加到网格中
    gtk_grid_attach(GTK_GRID(grid), username_entry, 0, 0, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), password_entry, 0, 1, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), login_button, 0, 2, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), register_button, 0, 3, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), status_label, 0, 4, 2, 1);

    // 连接按钮的点击事件
    g_signal_connect(login_button, "clicked", G_CALLBACK(on_login_button_clicked), NULL);
    g_signal_connect(register_button, "clicked", G_CALLBACK(on_register_button_clicked), NULL);

    // 连接关闭信号
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // 运行 GTK 主循环
    gtk_widget_show_all(window);
}

void on_create_group_button_clicked(GtkWidget *widget, gpointer data) {
    const char *group_name = gtk_entry_get_text(GTK_ENTRY(data));
    if (strlen(group_name) > 0) {
        send_command(sockfd, "create_group", group_name, NULL);
    }
}

void on_delete_group_button_clicked(GtkWidget *widget, gpointer data) {
    const char *group_name = gtk_entry_get_text(GTK_ENTRY(data));
    if (strlen(group_name) > 0) {
        send_command(sockfd, "delete_group", group_name, NULL);
    }
}

void on_edit_group_name_button_clicked(GtkWidget *widget, gpointer data) {
    const char *group_name = (const char *)data;
    GtkWidget *dialog, *content_area, *entry;
    GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;

    dialog = gtk_dialog_new_with_buttons("Edit Group Name",
                                         NULL,
                                         flags,
                                         ("_OK"),
                                         GTK_RESPONSE_ACCEPT,
                                         ("_Cancel"),
                                         GTK_RESPONSE_REJECT,
                                         NULL);

    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), group_name);
    gtk_container_add(GTK_CONTAINER(content_area), entry);
    gtk_widget_show_all(dialog);

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response == GTK_RESPONSE_ACCEPT) {
        const char *new_group_name = gtk_entry_get_text(GTK_ENTRY(entry));
        // 发送修改群组名的命令到服务器
        send_command(sockfd, "change_group_name", group_name, new_group_name);
    }

    gtk_widget_destroy(dialog);
}


void on_manage_group_button_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog, *content_area;
    GtkWidget *entry, *create_button, *delete_button;
    GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;

    dialog = gtk_dialog_new_with_buttons("Manage Groups",
                                         GTK_WINDOW(window),
                                         flags,
                                         "_Close",
                                         GTK_RESPONSE_CLOSE,
                                         NULL);

    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    entry = gtk_entry_new();
    gtk_container_add(GTK_CONTAINER(content_area), entry);

    create_button = gtk_button_new_with_label("Create Group");
    delete_button = gtk_button_new_with_label("Delete Group");

    gtk_container_add(GTK_CONTAINER(content_area), create_button);
    gtk_container_add(GTK_CONTAINER(content_area), delete_button);

    g_signal_connect(create_button, "clicked", G_CALLBACK(on_create_group_button_clicked), entry);
    g_signal_connect(delete_button, "clicked", G_CALLBACK(on_delete_group_button_clicked), entry);

    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}



void show_main_interface(const char *username) {
    // 移除现有的所有子组件
    GList *children, *iter;
    children = gtk_container_get_children(GTK_CONTAINER(window));
    for (iter = children; iter != NULL; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);

    // 创建新的grid布局
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
    gtk_container_add(GTK_CONTAINER(window), grid);

    // 创建好友列表、群组列表和消息列表
    friend_list = gtk_list_box_new();
    group_list = gtk_list_box_new();

    // 创建登出按钮
    GtkWidget *logout_button = gtk_button_new_with_label("Logout");

    // 创建管理好友的按钮
    GtkWidget *manage_friend_button = gtk_button_new_with_label("Manage Friends");
    
    // 创建管理群组的按钮
    GtkWidget *manage_group_button = gtk_button_new_with_label("Manage Groups");

    // 创建查看好友请求的按钮
    GtkWidget *view_friend_requests_button = gtk_button_new_with_label("Friend Requests");

    // 创建查看群组请求的按钮
    GtkWidget *view_group_requests_button = gtk_button_new_with_label("Group Requests");

    // 创建管理群组成员的按钮
    GtkWidget *manage_group_members_button = gtk_button_new_with_label("Group Members");

    // 将组件添加到grid布局
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Friends"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), friend_list, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Groups"), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), group_list, 1, 1, 1, 1);

    // 添加管理好友的按钮
    gtk_grid_attach(GTK_GRID(grid), manage_friend_button, 0, 2, 1, 1);

    // 添加管理群组的按钮
    gtk_grid_attach(GTK_GRID(grid), manage_group_button, 1, 2, 1, 1);

    // 添加查看好友请求的按钮
    gtk_grid_attach(GTK_GRID(grid), view_friend_requests_button, 0, 3, 1, 1);

    // 添加查看群组请求的按钮
    gtk_grid_attach(GTK_GRID(grid), view_group_requests_button, 1, 3, 1, 1);

    // 添加管理群组成员的按钮
    gtk_grid_attach(GTK_GRID(grid), manage_group_members_button, 1, 4, 1, 1);

    // 添加登出按钮
    gtk_grid_attach(GTK_GRID(grid), logout_button, 0, 5, 2, 1);

    // 连接登出按钮的点击事件
    g_signal_connect(logout_button, "clicked", G_CALLBACK(on_logout_button_clicked), NULL);

    // 连接管理好友按钮的点击事件
    g_signal_connect(manage_friend_button, "clicked", G_CALLBACK(on_manage_friend_button_clicked), NULL);

    // 连接管理群组按钮的点击事件
    g_signal_connect(manage_group_button, "clicked", G_CALLBACK(on_manage_group_button_clicked), NULL);

    // 连接查看好友请求按钮的点击事件，传入用户名
    g_signal_connect(view_friend_requests_button, "clicked", G_CALLBACK(on_view_friend_requests_button_clicked), NULL);

    // 连接查看群组请求按钮的点击事件
    g_signal_connect(view_group_requests_button, "clicked", G_CALLBACK(on_view_group_requests_button_clicked), NULL);

    // 连接管理群组成员按钮的点击事件
    g_signal_connect(manage_group_members_button, "clicked", G_CALLBACK(on_manage_group_members_button_clicked), NULL);

    // 刷新界面
    gtk_widget_show_all(window);

    // 发送请求获取好友和群组列表
    send_command(sockfd, "get_friend_list", username, NULL);
    sleep(1); // 等待一段时间，避免两个请求混在一起
    send_command(sockfd, "get_group_list", username, NULL);
}

void on_invite_to_group_button_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget **entries = (GtkWidget **)data;
    const char *group_name = gtk_entry_get_text(GTK_ENTRY(entries[0]));
    const char *user_name = gtk_entry_get_text(GTK_ENTRY(entries[1]));
    if (strlen(group_name) > 0 && strlen(user_name) > 0) {
        send_command(sockfd, "invite_user", group_name, user_name);
    }
}





void update_group_invitations(const char *data) {
    // 使用GString来存储群组邀请数据
    static GString *group_invitations_data = NULL;
    if (group_invitations_data) {
        g_string_free(group_invitations_data, TRUE);
    }
    group_invitations_data = g_string_new(data);
}

void on_view_group_requests_button_clicked(GtkWidget *widget, gpointer data) {
    // 发送请求获取群组邀请列表
    send_command(sockfd, "list_group_invitations", NULL, NULL);
    sleep(0.5); // 等待一段时间，避免两个请求混在一起
    display_group_invitations(global_group_invitations_data);
}

void on_manage_group_members_button_clicked(GtkWidget *widget, gpointer data) {
    // 创建一个新窗口来管理群组成员
    GtkWidget *dialog, *content_area;
    GtkWidget *entry_group, *entry_user, *invite_button, *remove_button;
    GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;

    dialog = gtk_dialog_new_with_buttons("Manage Group Members",
                                         GTK_WINDOW(window),
                                         flags,
                                         "_Close",
                                         GTK_RESPONSE_CLOSE,
                                         NULL);

    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    entry_group = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_group), "Group Name");
    gtk_container_add(GTK_CONTAINER(content_area), entry_group);

    entry_user = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_user), "User Name");
    gtk_container_add(GTK_CONTAINER(content_area), entry_user);

    invite_button = gtk_button_new_with_label("Invite to Group");
    remove_button = gtk_button_new_with_label("Remove from Group");

    gtk_container_add(GTK_CONTAINER(content_area), invite_button);
    gtk_container_add(GTK_CONTAINER(content_area), remove_button);

    // 将 entry_group 和 entry_user 作为参数传递给回调函数
    GtkWidget **entries = g_new(GtkWidget *, 2);
    entries[0] = entry_group;
    entries[1] = entry_user;
    g_signal_connect(invite_button, "clicked", G_CALLBACK(on_invite_to_group_button_clicked), entries);
    g_signal_connect(remove_button, "clicked", G_CALLBACK(on_remove_from_group_button_clicked), entries);

    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    g_free(entries);
}

// 接受群组邀请的回调函数
void on_accept_group_invitation_clicked(GtkWidget *widget, gpointer data) {
    printf("on_accept_group_invitation_clicked called\n");
    const char *invitation = (const char *)data;
    printf("data: %s\n", invitation);
    char group_name[BUFFER_SIZE];
    if (sscanf(invitation, "%s at", group_name) != 1) {
        fprintf(stderr, "Failed to parse group name from invitation: %s\n", invitation);
        return;
    }
    send_command(sockfd, "handle_invitation", group_name, "accept");
    // 发送请求刷新群组列表
    sleep(0.5);
    send_command(sockfd, "get_group_list", NULL, NULL);
    gtk_widget_destroy(gtk_widget_get_toplevel(widget)); // 关闭对话框
}

// 拒绝群组邀请的回调函数
void on_reject_group_invitation_clicked(GtkWidget *widget, gpointer data) {
    printf("on_reject_group_invitation_clicked called\n");
    const char *invitation = (const char *)data;
    printf("data: %s\n", invitation);
    char group_name[BUFFER_SIZE];
    if (sscanf(invitation, "%s at", group_name) != 1) {
        fprintf(stderr, "Failed to parse group name from invitation: %s\n", invitation);
        return;
    }
    send_command(sockfd, "handle_invitation", group_name, "decline");
    gtk_widget_destroy(gtk_widget_get_toplevel(widget)); // 关闭对话框
}


void on_remove_from_group_button_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget **entries = (GtkWidget **)data;
    GtkWidget *entry_group = entries[0];
    GtkWidget *entry_user = entries[1];

    const char *group_name = gtk_entry_get_text(GTK_ENTRY(entry_group));
    const char *user_name = gtk_entry_get_text(GTK_ENTRY(entry_user));

    if (strlen(group_name) > 0 && strlen(user_name) > 0) {
        // 发送删除用户请求
        send_command(sockfd, "remove_member", group_name, user_name);
    } else {
        g_warning("Group name or user name is empty");
    }
}

// 初始化群聊窗口存储
void init_group_chat_windows() {
    group_chat_windows = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
}

// 存储群聊窗口
void store_group_chat_window(const char *group_name, GtkWidget *dialog, GtkWidget *text_view) {
    if (!group_chat_windows) {
        init_group_chat_windows();
    }
    GtkWidget **data_array = g_new(GtkWidget *, 2);
    data_array[0] = dialog;
    data_array[1] = text_view;
    g_hash_table_insert(group_chat_windows, g_strdup(group_name), data_array);
}

// 查找群聊窗口
GtkWidget* find_or_create_group_chat_window(const char *group_name) {
    if (!group_chat_windows) {
        init_group_chat_windows();
    }
    GtkWidget **data_array = (GtkWidget **)g_hash_table_lookup(group_chat_windows, group_name);
    if (data_array) {
        return data_array[0];
    } else {
        // 创建新的群聊窗口
        on_group_clicked(NULL, (gpointer)group_name);
        data_array = (GtkWidget **)g_hash_table_lookup(group_chat_windows, group_name);
        return data_array ? data_array[0] : NULL;
    }
}

// 获取群聊窗口中的文本视图
GtkWidget* get_text_view_from_group_chat_window(GtkWidget *dialog) {
    if (!group_chat_windows) {
        return NULL;
    }
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, group_chat_windows);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        GtkWidget **data_array = (GtkWidget **)value;
        if (data_array[0] == dialog) {
            return data_array[1];
        }
    }
    return NULL;
}

void on_view_friend_requests_button_clicked(GtkWidget *widget, gpointer data) {
    // 发送请求获取好友请求列表
    send_command(sockfd, "list_friend_requests", NULL, NULL);
    sleep(0.5);
    printf("1 Friend requests: %s\n", global_friend_requests_data);
    display_friend_requests(global_friend_requests_data);
}

// 私聊聊天框的回调函数
void on_user_clicked(GtkWidget *widget, gpointer data) {
    printf("User clicked\n");
    const char *username = (const char *)data;

    // 创建一个新窗口来显示私聊聊天框
    GtkWidget *dialog, *content_area, *scrolled_window, *text_view, *entry, *send_button;
    GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;

    dialog = gtk_dialog_new_with_buttons("Private Chat",
                                         GTK_WINDOW(window),
                                         flags,
                                         "_Close",
                                         GTK_RESPONSE_CLOSE,
                                         NULL);

    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    // 创建一个滚动窗口来显示聊天记录
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(content_area), scrolled_window);

    // 创建一个文本视图来显示聊天记录
    text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);

    // 创建一个输入框来输入消息
    entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Type your message here...");
    gtk_container_add(GTK_CONTAINER(content_area), entry);

    // 创建一个发送按钮
    send_button = gtk_button_new_with_label("Send");
    gtk_container_add(GTK_CONTAINER(content_area), send_button);

    // 将用户名和文本视图作为参数传递给回调函数
    GtkWidget **data_array = g_new(GtkWidget *, 3);
    data_array[0] = (GtkWidget *)username;
    data_array[1] = text_view;
    data_array[2] = entry;

    g_signal_connect(send_button, "clicked", G_CALLBACK(on_send_private_message_button_clicked), data_array);

    gtk_widget_show_all(dialog);
    
    // 不再调用 gtk_dialog_run，而是监听关闭信号来销毁窗口
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);

    // 存储窗口和文本视图到全局变量中
    store_private_chat_window(username, dialog, text_view);
}


// 初始化私聊窗口存储
void init_private_chat_windows() {
    private_chat_windows = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
}

// 存储私聊窗口
void store_private_chat_window(const char *username, GtkWidget *dialog, GtkWidget *text_view) {
    if (!private_chat_windows) {
        init_private_chat_windows();
    }
    GtkWidget **data_array = g_new(GtkWidget *, 2);
    data_array[0] = dialog;
    data_array[1] = text_view;
    g_hash_table_insert(private_chat_windows, g_strdup(username), data_array);
}

void clear_interface() {
    // 清理界面上的所有组件
    gtk_container_foreach(GTK_CONTAINER(window), (GtkCallback)gtk_widget_destroy, NULL);
}

void reset_state() {
    // 重置客户端状态，例如清空用户名、密码等
    memset(current_username, 0, sizeof(current_username));
    // 其他需要重置的状态
}

void show_login_window() {
    // 创建并显示登录窗口
    GtkWidget *login_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(login_window), "Login");
    gtk_window_set_default_size(GTK_WINDOW(login_window), 300, 200);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(login_window), vbox);

    GtkWidget *username_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(username_entry), "Username");
    gtk_box_pack_start(GTK_BOX(vbox), username_entry, TRUE, TRUE, 0);

    GtkWidget *password_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(password_entry), "Password");
    gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), password_entry, TRUE, TRUE, 0);

    GtkWidget *login_button = gtk_button_new_with_label("Login");
    gtk_box_pack_start(GTK_BOX(vbox), login_button, TRUE, TRUE, 0);

    // 连接登录按钮的点击事件
    g_signal_connect(login_button, "clicked", G_CALLBACK(on_login_button_clicked), NULL);

    gtk_widget_show_all(login_window);
}

// 查找私聊窗口
GtkWidget* find_or_create_private_chat_window(const char *username) {
    if (!private_chat_windows) {
        init_private_chat_windows();
    }
    GtkWidget **data_array = (GtkWidget **)g_hash_table_lookup(private_chat_windows, username);
    if (data_array) {
        return data_array[0];
    } else {
        // 创建新的私聊窗口
        on_user_clicked(NULL, (gpointer)username);
        data_array = (GtkWidget **)g_hash_table_lookup(private_chat_windows, username);
        return data_array ? data_array[0] : NULL;
    }
}

// 获取私聊窗口中的文本视图
GtkWidget* get_text_view_from_private_chat_window(GtkWidget *dialog) {
    if (!private_chat_windows) {
        return NULL;
    }
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, private_chat_windows);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        GtkWidget **data_array = (GtkWidget **)value;
        if (data_array[0] == dialog) {
            return data_array[1];
        }
    }
    return NULL;
}

// 发送私聊消息的回调函数
void on_send_private_message_button_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget **data_array = (GtkWidget **)data;
    const char *username = (const char *)data_array[0];
    GtkWidget *text_view = data_array[1];
    GtkWidget *entry = data_array[2];

    const char *message = gtk_entry_get_text(GTK_ENTRY(entry));
    if (strlen(message) > 0) {
        // 发送私聊消息
        send_command(sockfd, "private_message", username, message);

        // 在聊天记录中显示发送的消息
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
        GtkTextIter end;
        gtk_text_buffer_get_end_iter(buffer, &end);
        gtk_text_buffer_insert(buffer, &end, "Me: ", -1);
        gtk_text_buffer_insert(buffer, &end, message, -1);
        gtk_text_buffer_insert(buffer, &end, "\n", -1);

        // 清空输入框
        gtk_entry_set_text(GTK_ENTRY(entry), "");
    }
}

void update_friend_list(const char *friend_list_data) {
    // 清空现有的好友列表
    GList *children, *iter;
    children = gtk_container_get_children(GTK_CONTAINER(friend_list));
    for (iter = children; iter != NULL; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);

    // 假设每个好友的名称和状态以逗号分隔
    GtkListBoxRow *row;
    char *friend_list_copy = g_strdup(friend_list_data);
    char *friend_info = strtok(friend_list_copy, ",");
    while (friend_info != NULL) {
        char friend_name[BUFFER_SIZE];
        char status[16];
        sscanf(friend_info, "%s (%[^)])", friend_name, status);

        // 创建标签并设置颜色
        GtkWidget *label = gtk_label_new(friend_name);
        GdkRGBA color;
        if (strcmp(status, "online") == 0) {
            gdk_rgba_parse(&color, "green");
        } else {
            gdk_rgba_parse(&color, "black");
        }
        gtk_widget_override_color(label, GTK_STATE_FLAG_NORMAL, &color);

        // 创建修改备注名字的按钮
        GtkWidget *edit_button = gtk_button_new_with_label("修改备注");
        g_signal_connect(edit_button, "clicked", G_CALLBACK(on_edit_nickname_button_clicked), g_strdup(friend_name));

        // 创建私聊按钮
        GtkWidget *chat_button = gtk_button_new_with_label("私聊");
        g_signal_connect(chat_button, "clicked", G_CALLBACK(on_user_clicked), g_strdup(friend_name));

        // 创建传输文件按钮
        GtkWidget *file_button = gtk_button_new_with_label("传输文件");
        g_signal_connect(file_button, "clicked", G_CALLBACK(on_send_file_button_clicked), g_strdup(friend_name));

        // 创建一个水平盒子，将标签和按钮添加到盒子中
        GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), edit_button, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), chat_button, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), file_button, FALSE, FALSE, 0);

        // 插入到列表中
        row = gtk_list_box_row_new();
        gtk_container_add(GTK_CONTAINER(row), hbox);
        gtk_list_box_insert(GTK_LIST_BOX(friend_list), row, -1);

        friend_info = strtok(NULL, ",");
    }
    g_free(friend_list_copy);

    // 刷新界面
    gtk_widget_show_all(friend_list);
}

void on_edit_nickname_button_clicked(GtkWidget *widget, gpointer data) {
    const char *friend_name = (const char *)data;

    // 提取真实用户名
    const char *real_username = extract_real_username(friend_name);

    // 创建一个对话框来输入新的备注名字
    GtkWidget *dialog = gtk_dialog_new_with_buttons("修改备注名字",
                                                    GTK_WINDOW(window),
                                                    GTK_DIALOG_MODAL,
                                                    "_取消", GTK_RESPONSE_CANCEL,
                                                    "_确定", GTK_RESPONSE_ACCEPT,
                                                    NULL);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), friend_name);
    gtk_container_add(GTK_CONTAINER(content_area), entry);

    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const char *new_nickname = gtk_entry_get_text(GTK_ENTRY(entry));
        // 发送修改备注名字的命令到服务器
        send_command(sockfd, "update_friend_nickname", real_username, new_nickname);
    }
    send_command(sockfd, "get_friend_list", NULL, NULL);
    gtk_widget_destroy(dialog);
}

// 群聊聊天框的回调函数
void on_group_clicked(GtkWidget *widget, gpointer data) {
    printf("Group clicked\n");
    const char *group_name = (const char *)data;

    // 创建一个新窗口来显示群聊聊天框
    GtkWidget *dialog, *content_area, *scrolled_window, *text_view, *entry, *send_button;
    GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;

    dialog = gtk_dialog_new_with_buttons("Group Chat",
                                         GTK_WINDOW(window),
                                         flags,
                                         "_Close",
                                         GTK_RESPONSE_CLOSE,
                                         NULL);

    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    // 创建一个滚动窗口来显示聊天记录
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(content_area), scrolled_window);

    // 创建一个文本视图来显示聊天记录
    text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);

    // 创建一个输入框来输入消息
    entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Type your message here...");
    gtk_container_add(GTK_CONTAINER(content_area), entry);

    // 创建一个发送按钮
    send_button = gtk_button_new_with_label("Send");
    gtk_container_add(GTK_CONTAINER(content_area), send_button);

    // 将群组名称和文本视图作为参数传递给回调函数
    GtkWidget **data_array = g_new(GtkWidget *, 3);
    data_array[0] = (GtkWidget *)group_name;
    data_array[1] = text_view;
    data_array[2] = entry;

    g_signal_connect(send_button, "clicked", G_CALLBACK(on_send_group_message_button_clicked), data_array);

    gtk_widget_show_all(dialog);
    
    // 不再调用 gtk_dialog_run，而是监听关闭信号来销毁窗口
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);

    // 存储窗口和文本视图到全局变量中
    store_group_chat_window(group_name, dialog, text_view);
}

// 发送群聊消息的回调函数
void on_send_group_message_button_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget **data_array = (GtkWidget **)data;
    const char *group_name = (const char *)data_array[0];
    GtkWidget *text_view = data_array[1];
    GtkWidget *entry = data_array[2];

    const char *message = gtk_entry_get_text(GTK_ENTRY(entry));
    if (strlen(message) > 0) {
        // 发送群聊消息
        send_command(sockfd, "group_message", group_name, message);

        // 在聊天记录中显示发送的消息
        GtkTextBuffer *text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
        GtkTextIter end;
        gtk_text_buffer_get_end_iter(text_buffer, &end);
        gtk_text_buffer_insert(text_buffer, &end, "Me: ", -1);
        gtk_text_buffer_insert(text_buffer, &end, message, -1);
        gtk_text_buffer_insert(text_buffer, &end, "\n", -1);

        // 清空输入框
        gtk_entry_set_text(GTK_ENTRY(entry), "");
    }
}

void update_group_list(const char *group_list_data) {
    // 清空现有的群组列表
    GList *children, *iter;
    children = gtk_container_get_children(GTK_CONTAINER(group_list));
    for (iter = children; iter != NULL; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);

    // 假设每个群组的名称以逗号分隔
    GtkListBoxRow *row;
    char *group_list_copy = g_strdup(group_list_data);
    char *group_name = strtok(group_list_copy, ",");
    while (group_name != NULL) {
        // 跳过无效数据
        if (strcmp(group_name, "END_OF_GROUP_LIST") == 0) {
            break;
        }

        // 创建标签
        GtkWidget *label = gtk_label_new(group_name);

        // 创建群聊按钮
        GtkWidget *chat_button = gtk_button_new_with_label("群聊");
        g_signal_connect(chat_button, "clicked", G_CALLBACK(on_group_clicked), g_strdup(group_name));

        // 创建修改群组名按钮
        GtkWidget *edit_group_name_button = gtk_button_new_with_label("群组名");
        g_signal_connect(edit_group_name_button, "clicked", G_CALLBACK(on_edit_group_name_button_clicked), g_strdup(group_name));

        // 创建一个水平盒子，将标签和按钮添加到盒子中
        GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), chat_button, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), edit_group_name_button, FALSE, FALSE, 0);

        // 插入到列表中
        row = gtk_list_box_row_new();
        gtk_container_add(GTK_CONTAINER(row), hbox);
        gtk_list_box_insert(GTK_LIST_BOX(group_list), row, -1);

        group_name = strtok(NULL, ",");
    }
    g_free(group_list_copy);

    // 刷新界面
    gtk_widget_show_all(group_list);
}

void on_accept_friend_request_clicked(GtkWidget *widget, gpointer data) {
    
    const char *request = (const char *)data;
    char requester[BUFFER_SIZE];
    printf("request: %s\n", request);
    sscanf(request, "Friend request from %s at", requester);
    printf("requester: %s\n", requester);
    send_command(sockfd, "handle_friend_request", requester, "accept");
    // 发送请求刷新好友列表
    send_command(sockfd, "get_friend_list", NULL, NULL);
    gtk_widget_destroy(gtk_widget_get_toplevel(widget)); // 关闭对话框
}

void on_reject_friend_request_clicked(GtkWidget *widget, gpointer data) {
    const char *request = (const char *)data;
    char requester[BUFFER_SIZE];
    sscanf(request, "%s at", requester);
    send_command(sockfd, "handle_friend_request", requester, "reject");
    gtk_widget_destroy(gtk_widget_get_toplevel(widget)); // 关闭对话框
}

void handle_server_response(char *response) {
    
    char *command = strtok(response, "\n");
    char *data = strtok(NULL, "\n");

    if (strcmp(command, "friends_list") == 0) {
        printf("1\n");
        update_friend_list(data);
    } else if (strcmp(command, "groups_list") == 0) {
        printf("2\n");
        update_group_list(data);

    }
}



// 提取括号中的真实用户名
const char* extract_real_username(const char *display_name) {
    const char *start = strchr(display_name, '(');
    const char *end = strchr(display_name, ')');
    if (start && end && end > start) {
        size_t len = end - start - 1;
        char *real_username = g_strndup(start + 1, len);
        return real_username;
    }
    return display_name;
}

// 显示群组邀请的函数
void display_group_invitations(const char *group_invitations_data) {
    GtkWidget *dialog, *content_area, *list_box;
    GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;

    dialog = gtk_dialog_new_with_buttons("Group Invitations",
                                         GTK_WINDOW(window),
                                         flags,
                                         "_Close",
                                         GTK_RESPONSE_CLOSE,
                                         NULL);

    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    // 创建一个列表框来显示群组邀请
    list_box = gtk_list_box_new();
    gtk_container_add(GTK_CONTAINER(content_area), list_box);

    // 检查 group_invitations_data 是否为空
    if (group_invitations_data == NULL || strlen(group_invitations_data) == 0) {
        GtkWidget *label = gtk_label_new("No group invitations.");
        gtk_container_add(GTK_CONTAINER(content_area), label);
    } else {
        // 将群组邀请数据按逗号分割
        char **requests = g_strsplit(group_invitations_data, ",", -1);
        for (int i = 0; requests[i] != NULL; i++) {
            // 跳过空行和无效数据
            if (strlen(requests[i]) == 0 || strcmp(requests[i], "END_OF_GROUP_LIST") == 0) {
                continue;
            }
            GtkWidget *row = gtk_list_box_row_new();
            GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
            GtkWidget *label = gtk_label_new(requests[i]);
            GtkWidget *accept_button = gtk_button_new_with_label("Accept");
            GtkWidget *reject_button = gtk_button_new_with_label("Reject");

            gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
            gtk_box_pack_start(GTK_BOX(hbox), accept_button, FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(hbox), reject_button, FALSE, FALSE, 0);

            gtk_container_add(GTK_CONTAINER(row), hbox);
            gtk_list_box_insert(GTK_LIST_BOX(list_box), row, -1);

            // 连接按钮的点击事件
            g_signal_connect(accept_button, "clicked", G_CALLBACK(on_accept_group_invitation_clicked), g_strdup(requests[i]));
            g_signal_connect(reject_button, "clicked", G_CALLBACK(on_reject_group_invitation_clicked), g_strdup(requests[i]));
        }
        g_strfreev(requests);
    }

    gtk_widget_show_all(dialog);
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
}

void display_friend_requests(const char *friend_requests_data) {
    GtkWidget *dialog, *content_area, *list_box;
    GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;

    dialog = gtk_dialog_new_with_buttons("Friend Requests",
                                         GTK_WINDOW(window),
                                         flags,
                                         "_Close",
                                         GTK_RESPONSE_CLOSE,
                                         NULL);

    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    // 创建一个列表框来显示好友请求
    list_box = gtk_list_box_new();
    gtk_container_add(GTK_CONTAINER(content_area), list_box);

    // 检查 friend_requests_data 是否为空
    if (friend_requests_data == NULL || strlen(friend_requests_data) == 0) {
        GtkWidget *label = gtk_label_new("No friend requests.");
        gtk_container_add(GTK_CONTAINER(content_area), label);
    } else {
        // 将好友请求数据按行分割
        char **requests = g_strsplit(friend_requests_data, "\n", -1);
        for (int i = 0; requests[i] != NULL; i++) {
            // 跳过空行
            if (strlen(requests[i]) == 0) {
                continue;
            }
            GtkWidget *row = gtk_list_box_row_new();
            GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
            GtkWidget *label = gtk_label_new(requests[i]);
            GtkWidget *accept_button = gtk_button_new_with_label("Accept");
            GtkWidget *reject_button = gtk_button_new_with_label("Reject");

            gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
            gtk_box_pack_start(GTK_BOX(hbox), accept_button, FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(hbox), reject_button, FALSE, FALSE, 0);

            gtk_container_add(GTK_CONTAINER(row), hbox);
            gtk_list_box_insert(GTK_LIST_BOX(list_box), row, -1);

            // 连接按钮的点击事件
            g_signal_connect(accept_button, "clicked", G_CALLBACK(on_accept_friend_request_clicked), g_strdup(requests[i]));
            g_signal_connect(reject_button, "clicked", G_CALLBACK(on_reject_friend_request_clicked), g_strdup(requests[i]));
        }
        g_strfreev(requests);
    }

    gtk_widget_show_all(dialog);
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
}

void *receive_server_messages(void *arg) {
    char buffer[BUFFER_SIZE];
    while (1) {
        int recv_size = recv(sockfd, buffer, sizeof(buffer), 0);
        if (recv_size > 0) {
            buffer[recv_size] = '\0';

            // 打印接收到的消息（调试）
            printf("Received message: %s\n", buffer);

            // 判断并处理不同的命令
            if (strstr(buffer, "get_friend_list") != NULL) {
                // 提取好友列表并更新界面
                char *friend_list_data = buffer + strlen("get_friend_list")+1; // 跳过命令部分
                printf("friend_list_data: %s\n", friend_list_data);

                // 检查消息是否以 "END_OF_GROUP_LIST" 结尾
                if (strstr(friend_list_data, "END_OF_GROUP_LIST") != NULL) {
                    printf("Message ends with END_OF_GROUP_LIST, ignoring...\n");
                } else {
                    UpdateData *update_data = g_malloc(sizeof(UpdateData));
                    update_data->data = g_strdup(friend_list_data);
                    g_idle_add(update_friend_list_idle, update_data);
                    printf("friend_list_update data is %s\n", update_data->data);
                }
            } 
            else if (strstr(buffer, "get_group_list") != NULL) {
                // 提取群组列表并更新界面
                char *group_list_data = buffer + strlen("get_group_list")+1; // 跳过命令部分
                printf("group_list_data: %s\n", group_list_data);
                UpdateData *update_data = g_malloc(sizeof(UpdateData));
                update_data->data = g_strdup(group_list_data);
                g_idle_add(update_group_list_idle, update_data);
                printf("group_list_update data is %s\n",update_data->data);
            }else if (strstr(buffer, "list_friend_requests") != NULL) {
                // 处理查看好友请求的响应
                char *friend_requests_data = buffer + strlen("list_friend_requests"); // 跳过命令部分
                printf("friend_requests_data: %s\n", friend_requests_data);
                global_friend_requests_data=friend_requests_data;
                
            }else if (strstr(buffer, "online: ") != NULL) {
                // 处理用户上线的消息
                char *username = buffer + strlen("online: "); // 跳过命令部分
                printf("User %s is online\n", username);

                // 更新好友列表，将在线的用户名修改为绿色
                update_friend_status_in_list(username, TRUE);
            } else if (strstr(buffer, "offline: ") != NULL) {
                // 处理用户离线的消息
                char *username = buffer + strlen("offline: "); // 跳过命令部分
                printf("User %s is offline\n", username);

                // 更新好友列表，将离线的用户名修改为红色
                update_friend_status_in_list(username, FALSE);
            } else if (strstr(buffer, "create_group:") != NULL) {
                // 处理创建群组的响应
                printf("Group created successfully.\n");
                // 发送请求刷新群组列表
                send_command(sockfd, "get_group_list", NULL, NULL);
            } else if (strstr(buffer, "delete_group:") != NULL) {
                // 处理创建群组的响应
                printf("Group deleted successfully.\n");
                // 发送请求刷新群组列表
                send_command(sockfd, "get_group_list", NULL, NULL);
            } else if (strstr(buffer, "group_invitations:") != NULL) {
                // 处理查看群组邀请的响应
                char *group_invitations_data = buffer + strlen("group_invitations:"); // 跳过命令部分
                printf("group_invitations_data: %s\n", group_invitations_data);

                global_group_invitations_data=group_invitations_data;
            } else if (strstr(buffer, "remove_group_member:") != NULL) {
                // 处理被删除群组成员的消息
                char *group_name = buffer + strlen("remove_group_member:");
                printf("You have been removed from group: %s\n", group_name);
                // 刷新群组列表
                send_command(sockfd, "get_group_list", NULL, NULL);
            } else if (strstr(buffer, "private_message:") == buffer) {
                // 处理私聊消息
                const char *message_data = buffer + strlen("private_message:");
                char sender[BUFFER_SIZE];
                char message[BUFFER_SIZE];
                sscanf(message_data, "%[^-]-%s", sender, message);

                // 查找或创建私聊窗口
                GtkWidget *dialog = find_or_create_private_chat_window(sender);
                GtkWidget *text_view = get_text_view_from_private_chat_window(dialog);

                // 在聊天记录中显示接收到的消息
                GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
                GtkTextIter end;
                gtk_text_buffer_get_end_iter(buffer, &end);
                gtk_text_buffer_insert(buffer, &end, sender, -1);
                gtk_text_buffer_insert(buffer, &end, ": ", -1);
                gtk_text_buffer_insert(buffer, &end, message, -1);
                gtk_text_buffer_insert(buffer, &end, "\n", -1);
            } else if (strstr(buffer, "send_group_messag:") == buffer) {
                // 处理群聊消息
                const char *message_data = buffer + strlen("send_group_messag:");
                char group_name[BUFFER_SIZE];
                char sender[BUFFER_SIZE];
                char message[BUFFER_SIZE];
                sscanf(message_data, "%[^-]-%[^-]-%s", group_name, sender, message);

                // 忽略自己发送的消息
                if (strcmp(sender, current_username) == 0) {
                    return;
                }

                // 查找或创建群聊窗口
                GtkWidget *dialog = find_or_create_group_chat_window(group_name);
                GtkWidget *text_view = get_text_view_from_group_chat_window(dialog);

                // 在聊天记录中显示接收到的消息
                GtkTextBuffer *text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
                GtkTextIter end;
                gtk_text_buffer_get_end_iter(text_buffer, &end);
                gtk_text_buffer_insert(text_buffer, &end, sender, -1);
                gtk_text_buffer_insert(text_buffer, &end, ": ", -1);
                gtk_text_buffer_insert(text_buffer, &end, message, -1);
                gtk_text_buffer_insert(text_buffer, &end, "\n", -1);
            } else if (strstr(buffer, "FILE") == buffer) {
                // 处理文件传输
                const char *file_info = buffer + strlen("FILE ");
                char file_name[BUFFER_SIZE];
                long file_size;
                sscanf(file_info, "%s %ld", file_name, &file_size);

                FILE *file = fopen(file_name, "wb");
                if (file == NULL) {
                    perror("Failed to open file");
                    return;
                }

                char file_buffer[BUFFER_SIZE];
                size_t bytes_received;
                long total_bytes_received = 0;
                while (total_bytes_received < file_size) {
                    bytes_received = recv(sockfd, file_buffer, sizeof(file_buffer), 0);
                    if (bytes_received <= 0) {
                        break;
                    }
                    fwrite(file_buffer, 1, bytes_received, file);
                    total_bytes_received += bytes_received;
                }

                fclose(file);
                printf("File %s received successfully.\n", file_name);
            } else if (strncmp(buffer, "change_group_name:", 18) == 0) {
                const char *group_info = buffer + 18;
                char old_group_name[BUFFER_SIZE], new_group_name[BUFFER_SIZE];
                sscanf(group_info, "%s to %s", old_group_name, new_group_name);

                // 调用函数更新群组列表
                send_command(sockfd, "get_group_list", NULL, NULL);
            }else if (strstr(buffer, "create_group:") != NULL) {
                // 处理创建群组的响应
                printf("Group created successfully.\n");
                // 发送请求刷新群组列表
                send_command(sockfd, "get_group_list", NULL, NULL);
            } else if (strstr(buffer, "add_friend:") != NULL) {
                // 处理添加好友的消息
                printf("New friend added: %s\n", buffer+11);
                // 发送请求刷新好友列表
                send_command(sockfd, "get_friend_list", NULL, NULL);
            } else if (strstr(buffer, "remove_friend") != NULL) {
                // 处理添加好友的消息
                printf("lost friend : %s\n", buffer+14);
                // 发送请求刷新好友列表
                sleep(0.5);
                send_command(sockfd, "get_friend_list", NULL, NULL);
            }
        }
    }
    return NULL;
}

void update_friend_status_in_list(const char *username, gboolean is_online) {
    printf("update_friend_status_in_list\n");

    // 获取friend_list容器中的所有子控件
    GList *children = gtk_container_get_children(GTK_CONTAINER(friend_list));

    for (GList *iter = children; iter != NULL; iter = g_list_next(iter)) {
        GtkWidget *row = GTK_WIDGET(iter->data);

        // 获取row中的所有子控件
        GList *box_children = gtk_container_get_children(GTK_CONTAINER(row));
        for (GList *box_iter = box_children; box_iter != NULL; box_iter = g_list_next(box_iter)) {
            GtkWidget *child = GTK_WIDGET(box_iter->data);

            // 如果是水平盒子（GtkBox）
            if (GTK_IS_BOX(child)) {
                GList *label_children = gtk_container_get_children(GTK_CONTAINER(child));
                for (GList *label_iter = label_children; label_iter != NULL; label_iter = g_list_next(label_iter)) {
                    GtkWidget *label_child = GTK_WIDGET(label_iter->data);

                    // 如果是标签（GtkLabel）
                    if (GTK_IS_LABEL(label_child)) {
                        GtkLabel *label = GTK_LABEL(label_child);

                        // 获取标签的文本内容
                        const char *friend_name = gtk_label_get_text(label);
                        const char *real_username = extract_real_username(friend_name);

                        printf("Comparing '%s' with '%s'\n", real_username, username);

                        // 如果用户名匹配
                        if (strcmp(real_username, username) == 0) {
                            GdkRGBA color;

                            // 根据在线状态设置颜色
                            if (is_online) {
                                gdk_rgba_parse(&color, "green");
                            } else {
                                gdk_rgba_parse(&color, "black");
                            }

                            // 更新标签颜色
                            gtk_widget_override_color(GTK_WIDGET(label), GTK_STATE_FLAG_NORMAL, &color);
                            printf("Updated %s's status to %s\n", friend_name, is_online ? "online (green)" : "offline (black)");

                            // 强制刷新标签显示
                            gtk_widget_queue_draw(GTK_WIDGET(label));
                            break;  // 找到匹配项后停止遍历
                        }
                    }
                }
                g_list_free(label_children); // 释放获取到的子控件列表
            }
        }

        g_list_free(box_children); // 释放获取到的子控件列表
    }

    g_list_free(children); // 释放friend_list容器中的子控件列表
}



void on_logout_button_clicked(GtkWidget *widget, gpointer data) {
    // 发送登出命令到服务器
    send_command(sockfd, "logout", NULL, NULL);
    
    // 接收登出确认消息
    char buffer[BUFFER_SIZE];
    int recv_size = recv(sockfd, buffer, sizeof(buffer), 0);
    if (recv_size > 0) {
        buffer[recv_size] = '\0'; // 确保字符串以'\0'结尾
        if (strcmp(buffer, "You have been logged out.") == 0) {
            printf("Logout successful.\n");
            clear_interface();
            reset_state();
            show_login_window();
        } else {
            printf("Logout failed: %s\n", buffer);
        }
    } else {
        printf("Failed to receive logout confirmation from server.\n");
    }

    // 在收到服务器的响应后再进行界面切换，避免卡死
    //g_idle_add((GSourceFunc)on_back_button_clicked, widget);
}

int main(int argc, char *argv[]) {
    XInitThreads(); 
    gtk_init(&argc, &argv);

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server_ip>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *server_ip = argv[1];

    init_private_chat_windows();
    init_group_chat_windows();

    // 创建窗口
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Client Login");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);

    // 创建网格布局
    grid = gtk_grid_new();
    gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
    gtk_container_add(GTK_CONTAINER(window), grid);

    // 创建用户名和密码输入框
    username_entry = gtk_entry_new();
    password_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(username_entry), "Username");
    gtk_entry_set_placeholder_text(GTK_ENTRY(password_entry), "Password");
    gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE); // 隐藏密码

    // 设置输入框的宽度为固定值，避免使用动态获取窗口宽度
    int input_width = 320; // 设置固定宽度
    gtk_widget_set_size_request(username_entry, input_width, -1);
    gtk_widget_set_size_request(password_entry, input_width, -1);

    // 创建按钮
    GtkWidget *login_button = gtk_button_new_with_label("Login");
    GtkWidget *register_button = gtk_button_new_with_label("Register");

    // 创建状态标签
    status_label = gtk_label_new("");

    // 将组件添加到网格中
    gtk_grid_attach(GTK_GRID(grid), username_entry, 0, 0, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), password_entry, 0, 1, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), login_button, 0, 2, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), register_button, 0, 3, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), status_label, 0, 4, 2, 1);

    // 连接按钮的点击事件
    g_signal_connect(login_button, "clicked", G_CALLBACK(on_login_button_clicked), NULL);
    g_signal_connect(register_button, "clicked", G_CALLBACK(on_register_button_clicked), NULL);
    

    // 连接关闭信号
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // 连接到服务器
    connect_to_server(server_ip);

    // 创建接收服务器消息的线程
    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_server_messages, NULL);

    // 运行 GTK 主循环
    gtk_widget_show_all(window);
    gtk_main();

    // 关闭套接字
    close(sockfd);

    return 0;
}