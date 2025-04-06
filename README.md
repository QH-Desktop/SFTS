# 安全文件传输系统

## **1. 功能需求**

本系统采用 **C/S 架构**，基于 **Linux 系统调用** 和 **商用密码（SM2/SM3/SM4）** 实现**安全文件传输**。

### **1.1 主要功能**

| **模块**              | **功能描述**                                                 |
| --------------------- | ------------------------------------------------------------ |
| **用户认证**          | - 支持用户注册、登录，身份鉴别（使用 **SM3** + 盐存储密码）- 服务器端进行访问控制，防止未授权访问 |
| **文件加密存储**      | - 客户端使用 **SM4** 加密后上传文件，服务器仅存储加密文件 - 服务器可解密并提供访问控制 |
| **安全文件传输**      | - **Socket** 进行 C/S 通信，支持并发传输 - **SM4** 加密数据流，防止窃听和篡改 - **SM2** 进行密钥交换 |
| **访问控制**          | - 服务器端根据用户身份 **控制文件权限**，防止未授权用户访问 - 仅允许用户下载自己上传的文件 |
| **日志审计**          | - 记录用户登录、文件上传/下载等日志（使用 **SM4** 加密存储）- 管理员可通过命令 `view_logs` 查询日志 |
| **命令行交互（CLI）** | - 纯命令行界面，去除 GUI，支持 `upload file.txt`、`download file.txt` 等命令 |

------

### **1.2 交互流程**

#### **（1）用户注册 & 登录**

```
客户端： 
$ ./client
> register alice password123  # 注册
> login alice password123     # 登录

服务器：
[INFO] User 'alice' registered.
[INFO] User 'alice' logged in successfully.
```

------

#### **（2）文件上传**

```
客户端：
> upload report.pdf
[Progress] Uploading... 50% ... 100%
[INFO] File 'report.pdf' uploaded successfully.

服务器：
[INFO] Received encrypted file 'report.pdf' from 'alice'.
```

------

#### **（3）文件下载**

```
客户端：
> download report.pdf
[Progress] Downloading... 50% ... 100%
[INFO] File 'report.pdf' downloaded successfully.

服务器：
[INFO] User 'alice' downloaded 'report.pdf'.
```

------

#### **（4）日志审计**

```
管理员：
> view_logs
[LOG] 2025-03-15 10:00:00 - User 'alice' logged in.
[LOG] 2025-03-15 10:10:00 - User 'alice' uploaded 'report.pdf'.
```

------

## **2. 非功能需求**

### **2.1 安全性**

 **身份鉴别**：用户密码仅存储 **SM3 + 盐** 计算的 Hash，不保存明文密码
 **数据加密**：文件传输 **SM4 加密**，服务器存储仍保持加密
 **密钥管理**：客户端和服务器使用 **SM2** 进行密钥交换
 **访问控制**：只有上传者可以下载自己的文件
 **日志加密**：所有日志使用 **SM4** 加密，防止篡改

### **2.2 性能**

**多线程并发**：服务器采用 **多线程** 处理多个客户端请求
 **数据传输优化**：支持**断点续传**（可选）

### **2.3 兼容性**

**操作系统**：支持 Linux，使用 **POSIX API** 实现
**数据库**：支持 **SQLite/MySQL** 进行用户管理和日志存储

