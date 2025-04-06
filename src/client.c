#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <termios.h>
#include <ctype.h>

#define BUFFER_SIZE 1024

void get_password(char *password) {
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    
    fgets(password, 50, stdin);
    password[strcspn(password, "\n")] = '\0';
    
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

int check_password(const char *password) {
    if (strlen(password) < 6) return 0;
    int has_digit = 0, has_alpha = 0;
    for (int i = 0; password[i]; i++) {
        if (isdigit(password[i])) has_digit = 1;
        if (isalpha(password[i])) has_alpha = 1;
    }
    return has_digit && has_alpha;
}

void show_main_menu() {
    printf("\n========== 主菜单 ==========\n");
    printf("1. 上传文件\n");
    printf("2. 下载文件\n");
    printf("3. 上传记录\n");
    printf("4. 下载记录\n");
    printf("5. 退出系统\n");
    printf("请选择操作 (1-5): ");
}

int connect_to_server() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
         perror("socket 创建失败");
         exit(1);
    }
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
         perror("连接失败");
         exit(1);
    }
    return sock;
}

int main() {
    char cmd[10], username[50], password[50];
    char buffer[BUFFER_SIZE];
    int logged_in = 0;
    
    while (!logged_in) {
        printf("请选择操作 (register/login): ");
        scanf("%s", cmd);
        getchar();
        
        printf("请输入用户名: ");
        fgets(username, 50, stdin);
        username[strcspn(username, "\n")] = '\0';
        
        int valid = 0;
        do {
            printf("请输入密码（至少6位，包含字母和数字）: ");
            get_password(password);
            
            if (strcmp(cmd, "register") == 0 && !check_password(password)) {
                printf("\n密码不符合要求！\n");
                valid = 0;
            } else {
                valid = 1;
            }
        } while (!valid);
        
        // 每次操作均创建新的 socket 连接
        int sock = connect_to_server();
        snprintf(buffer, BUFFER_SIZE, "%s %s %s", cmd, username, password);
        send(sock, buffer, strlen(buffer), 0);
        recv(sock, buffer, BUFFER_SIZE, 0);
        
        if (strncmp(buffer, "REGISTER_SUCCESS", 16) == 0) {
            printf("\n注册成功！\n");
            // 注册成功后通常需要重新登录，此处可根据需求提示用户登录
        } else if (strncmp(buffer, "USER_EXISTS", 11) == 0) {
            printf("用户名已存在！\n");
        } else if (strncmp(buffer, "LOGIN_SUCCESS", 13) == 0) {
            printf("\n登录成功！\n");
            logged_in = 1;
            
            // 主菜单循环
            int choice;
            do {
                show_main_menu();
                scanf("%d", &choice);
                getchar();
                
                switch (choice) {
                    case 1:
                    case 2:
                    case 3:
                    case 4:
                        printf("\n功能开发中，敬请期待！\n");
                        break;
                    case 5:
                        printf("确定要退出吗？(y/n): ");
                        char confirm;
                        scanf("%c", &confirm);
                        if (confirm == 'y' || confirm == 'Y') {
                            printf("感谢使用，再见！\n");
                            close(sock);
                            exit(0);
                        }
                        break;
                    default:
                        printf("无效选项，请重新输入！\n");
                }
            } while (1);
        } else if (strncmp(buffer, "LOGIN_FAILED", 12) == 0) {
            printf("密码错误！\n");
            close(sock);
            // 登录失败后循环请求，注意每次都新建连接
            while (1) {
                printf("请重新输入密码: ");
                get_password(password);
                
                int new_sock = connect_to_server();
                snprintf(buffer, BUFFER_SIZE, "login %s %s", username, password); // 保持用户名
                send(new_sock, buffer, strlen(buffer), 0);
                recv(new_sock, buffer, BUFFER_SIZE, 0);
                
                if (strncmp(buffer, "LOGIN_SUCCESS", 13) == 0) {
                    printf("\n登录成功！\n");
                    logged_in = 1;
                    sock = new_sock; // 后续使用该连接进入主菜单
                    break;
                } else {
                    printf("密码错误！\n");
                    close(new_sock);
                }
            }
            // 进入主菜单
            int choice;
            do {
                show_main_menu();
                scanf("%d", &choice);
                getchar();
                
                switch (choice) {
                    case 1:
                    case 2:
                    case 3:
                    case 4:
                        printf("\n功能开发中，敬请期待！\n");
                        break;
                    case 5:
                        printf("确定要退出吗？(y/n): ");
                        char confirm;
                        scanf("%c", &confirm);
                        if (confirm == 'y' || confirm == 'Y') {
                            printf("感谢使用，再见！\n");
                            close(sock);
                            exit(0);
                        }
                        break;
                    default:
                        printf("无效选项，请重新输入！\n");
                }
            } while (1);
        }
        close(sock);
    }
    return 0;
}

