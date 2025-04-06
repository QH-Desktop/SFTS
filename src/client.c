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
    printf("\n========== ���˵� ==========\n");
    printf("1. �ϴ��ļ�\n");
    printf("2. �����ļ�\n");
    printf("3. �ϴ���¼\n");
    printf("4. ���ؼ�¼\n");
    printf("5. �˳�ϵͳ\n");
    printf("��ѡ����� (1-5): ");
}

int connect_to_server() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
         perror("socket ����ʧ��");
         exit(1);
    }
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
         perror("����ʧ��");
         exit(1);
    }
    return sock;
}

int main() {
    char cmd[10], username[50], password[50];
    char buffer[BUFFER_SIZE];
    int logged_in = 0;
    
    while (!logged_in) {
        printf("��ѡ����� (register/login): ");
        scanf("%s", cmd);
        getchar();
        
        printf("�������û���: ");
        fgets(username, 50, stdin);
        username[strcspn(username, "\n")] = '\0';
        
        int valid = 0;
        do {
            printf("���������루����6λ��������ĸ�����֣�: ");
            get_password(password);
            
            if (strcmp(cmd, "register") == 0 && !check_password(password)) {
                printf("\n���벻����Ҫ��\n");
                valid = 0;
            } else {
                valid = 1;
            }
        } while (!valid);
        
        // ÿ�β����������µ� socket ����
        int sock = connect_to_server();
        snprintf(buffer, BUFFER_SIZE, "%s %s %s", cmd, username, password);
        send(sock, buffer, strlen(buffer), 0);
        recv(sock, buffer, BUFFER_SIZE, 0);
        
        if (strncmp(buffer, "REGISTER_SUCCESS", 16) == 0) {
            printf("\nע��ɹ���\n");
            // ע��ɹ���ͨ����Ҫ���µ�¼���˴��ɸ���������ʾ�û���¼
        } else if (strncmp(buffer, "USER_EXISTS", 11) == 0) {
            printf("�û����Ѵ��ڣ�\n");
        } else if (strncmp(buffer, "LOGIN_SUCCESS", 13) == 0) {
            printf("\n��¼�ɹ���\n");
            logged_in = 1;
            
            // ���˵�ѭ��
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
                        printf("\n���ܿ����У������ڴ���\n");
                        break;
                    case 5:
                        printf("ȷ��Ҫ�˳���(y/n): ");
                        char confirm;
                        scanf("%c", &confirm);
                        if (confirm == 'y' || confirm == 'Y') {
                            printf("��лʹ�ã��ټ���\n");
                            close(sock);
                            exit(0);
                        }
                        break;
                    default:
                        printf("��Чѡ����������룡\n");
                }
            } while (1);
        } else if (strncmp(buffer, "LOGIN_FAILED", 12) == 0) {
            printf("�������\n");
            close(sock);
            // ��¼ʧ�ܺ�ѭ������ע��ÿ�ζ��½�����
            while (1) {
                printf("��������������: ");
                get_password(password);
                
                int new_sock = connect_to_server();
                snprintf(buffer, BUFFER_SIZE, "login %s %s", username, password); // �����û���
                send(new_sock, buffer, strlen(buffer), 0);
                recv(new_sock, buffer, BUFFER_SIZE, 0);
                
                if (strncmp(buffer, "LOGIN_SUCCESS", 13) == 0) {
                    printf("\n��¼�ɹ���\n");
                    logged_in = 1;
                    sock = new_sock; // ����ʹ�ø����ӽ������˵�
                    break;
                } else {
                    printf("�������\n");
                    close(new_sock);
                }
            }
            // �������˵�
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
                        printf("\n���ܿ����У������ڴ���\n");
                        break;
                    case 5:
                        printf("ȷ��Ҫ�˳���(y/n): ");
                        char confirm;
                        scanf("%c", &confirm);
                        if (confirm == 'y' || confirm == 'Y') {
                            printf("��лʹ�ã��ټ���\n");
                            close(sock);
                            exit(0);
                        }
                        break;
                    default:
                        printf("��Чѡ����������룡\n");
                }
            } while (1);
        }
        close(sock);
    }
    return 0;
}

