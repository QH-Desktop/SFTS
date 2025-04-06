#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sqlite3.h>
#include <openssl/evp.h>
#include <pthread.h>

#define SALT_SIZE 16
#define BUFFER_SIZE 1024
#define PORT 8080

sqlite3 *db;
pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;

// 修复 SM3 哈希函数
void sm3_hash(const char *input, size_t inlen, unsigned char *output, unsigned char *salt) {
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    const EVP_MD *md = EVP_sm3(); // 类型更正为 EVP_MD
    
    EVP_DigestInit_ex(ctx, md, NULL);
    EVP_DigestUpdate(ctx, salt, SALT_SIZE);
    EVP_DigestUpdate(ctx, input, inlen);
    EVP_DigestFinal_ex(ctx, output, NULL);
    EVP_MD_CTX_free(ctx);
}

// 初始化数据库
int init_database() {
    char *sql = "CREATE TABLE IF NOT EXISTS users ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "username TEXT UNIQUE NOT NULL,"
                "salt BLOB NOT NULL,"
                "password_hash BLOB NOT NULL);";

    char *err_msg = NULL;
    pthread_mutex_lock(&db_mutex);
    int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    pthread_mutex_unlock(&db_mutex);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return -1;
    }
    return 0;
}

// 注册用户
int register_user(const char *username, const char *password) {
    unsigned char salt[SALT_SIZE];
    unsigned char hash[EVP_MAX_MD_SIZE];
    
    // 生成随机盐
    RAND_bytes(salt, SALT_SIZE);
    sm3_hash(password, strlen(password), hash, salt);

    pthread_mutex_lock(&db_mutex);
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO users (username, salt, password_hash) VALUES (?, ?, ?)";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Prepare failed: %s\n", sqlite3_errmsg(db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 2, salt, SALT_SIZE, SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 3, hash, EVP_MD_size(EVP_sm3()), SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    if (rc == SQLITE_CONSTRAINT) {
        fprintf(stderr, "Username already exists\n");
        return -2; // 用户名已存在
    } else if (rc != SQLITE_DONE) {
        fprintf(stderr, "Execution failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    return 0;
}

// 登录验证
int login_user(const char *username, const char *password) {
    pthread_mutex_lock(&db_mutex);
    sqlite3_stmt *stmt;
    const char *sql = "SELECT salt, password_hash FROM users WHERE username = ?";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Prepare failed: %s\n", sqlite3_errmsg(db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);

    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1; // 用户不存在
    }

    const void *salt = sqlite3_column_blob(stmt, 0);
    const void *stored_hash = sqlite3_column_blob(stmt, 1);
    int hash_len = sqlite3_column_bytes(stmt, 1);

    unsigned char computed_hash[EVP_MAX_MD_SIZE];
    sm3_hash(password, strlen(password), computed_hash, (unsigned char*)salt);

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return memcmp(computed_hash, stored_hash, hash_len) == 0 ? 0 : -1;
}

// 客户端处理线程
void *handle_client(void *arg) {
    int client_fd = *(int*)arg;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    if ((bytes_read = read(client_fd, buffer, BUFFER_SIZE)) > 0) {
        buffer[bytes_read] = '\0';
        char cmd[20], username[50], password[50];
        sscanf(buffer, "%s %s %s", cmd, username, password);

        if (strcmp(cmd, "register") == 0) {
            int ret = register_user(username, password);
            if (ret == 0) {
                send(client_fd, "REGISTER_SUCCESS", 16, 0);
            } else if (ret == -2) {
                send(client_fd, "USER_EXISTS", 11, 0);
            } else {
                send(client_fd, "REGISTER_FAILED", 15, 0);
            }
        } else if (strcmp(cmd, "login") == 0) {
            int ret = login_user(username, password);
            if (ret == 0) {
                send(client_fd, "LOGIN_SUCCESS", 13, 0);
            } else {
                send(client_fd, "LOGIN_FAILED", 12, 0);
            }
        }
    }

    close(client_fd);
    free(arg);
    return NULL;
}

int main(int argc, char *argv[]) {
    // 初始化数据库
    if (sqlite3_open("users.db", &db) != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    if (init_database() != 0) {
        sqlite3_close(db);
        return 1;
    }

    // 创建TCP Socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        sqlite3_close(db);
        return 1;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = INADDR_ANY
    };

    // 修复 bind() 调用
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        sqlite3_close(db);
        return 1;
    }

    // 修复 listen() 调用
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        sqlite3_close(db);
        return 1;
    }

    printf("Server started on port %d\n", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int *client_fd = malloc(sizeof(int));
        *client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        
        if (*client_fd < 0) {
            perror("accept");
            free(client_fd);
            continue;
        }

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, client_fd) != 0) {
            perror("pthread_create");
            close(*client_fd);
            free(client_fd);
        }
        pthread_detach(tid);
    }

    close(server_fd);
    sqlite3_close(db);
    return 0;
}
