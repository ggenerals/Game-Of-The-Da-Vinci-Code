#ifndef GEN_SOCKET_H
#define GEN_SOCKET_H

#pragma comment(lib, "ws2_32.lib")

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>

// 扩展位：预留未来功能标志
enum class GenSocketFlags : unsigned int {
    None = 0,
    EnableKeepAlive = 1 << 0,
    EnableNoDelay = 1 << 1,
    AllowReuseAddr = 1 << 2,
    // 预留扩展位 3-31
    Reserved_3 = 1 << 3,
    Reserved_4 = 1 << 4,
    Reserved_5 = 1 << 5,
    Reserved_6 = 1 << 6,
    Reserved_7 = 1 << 7,
    Reserved_8 = 1 << 8,
    Reserved_9 = 1 << 9,
    Reserved_10 = 1 << 10,
    Reserved_11 = 1 << 11,
    Reserved_12 = 1 << 12,
    Reserved_13 = 1 << 13,
    Reserved_14 = 1 << 14,
    Reserved_15 = 1 << 15,
    Reserved_16 = 1 << 16,
    Reserved_17 = 1 << 17,
    Reserved_18 = 1 << 18,
    Reserved_19 = 1 << 19,
    Reserved_20 = 1 << 20,
    Reserved_21 = 1 << 21,
    Reserved_22 = 1 << 22,
    Reserved_23 = 1 << 23,
    Reserved_24 = 1 << 24,
    Reserved_25 = 1 << 25,
    Reserved_26 = 1 << 26,
    Reserved_27 = 1 << 27,
    Reserved_28 = 1 << 28,
    Reserved_29 = 1 << 29,
    Reserved_30 = 1u << 30,
    Reserved_31 = (1u << 31)
};

inline GenSocketFlags operator|(GenSocketFlags a, GenSocketFlags b) {
    return static_cast<GenSocketFlags>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
}

inline GenSocketFlags operator&(GenSocketFlags a, GenSocketFlags b) {
    return static_cast<GenSocketFlags>(static_cast<unsigned int>(a) & static_cast<unsigned int>(b));
}

// 客户端连接信息类：捆绑IP连接、名字、详细说明
class GenClientConnection {
public:
    GenClientConnection() : socket_(INVALID_SOCKET), name_(""), description_("") {}
    
    GenClientConnection(SOCKET sock, const std::string& name, const std::string& desc)
        : socket_(sock), name_(name), description_(desc) {}
    
    // 拷贝构造
    GenClientConnection(const GenClientConnection& other)
        : socket_(other.socket_), name_(other.name_), description_(other.description_) {}
    
    // 移动构造
    GenClientConnection(GenClientConnection&& other) noexcept
        : socket_(other.socket_), name_(std::move(other.name_)), description_(std::move(other.description_)) {
        other.socket_ = INVALID_SOCKET;
    }
    
    // 拷贝赋值
    GenClientConnection& operator=(const GenClientConnection& other) {
        if (this != &other) {
            socket_ = other.socket_;
            name_ = other.name_;
            description_ = other.description_;
        }
        return *this;
    }
    
    // 移动赋值
    GenClientConnection& operator=(GenClientConnection&& other) noexcept {
        if (this != &other) {
            socket_ = other.socket_;
            name_ = std::move(other.name_);
            description_ = std::move(other.description_);
            other.socket_ = INVALID_SOCKET;
        }
        return *this;
    }
    
    ~GenClientConnection() {
        // 注意：此处不自动关闭socket，由管理器统一管理
    }
    
    SOCKET getSocket() const { return socket_; }
    void setSocket(SOCKET sock) { socket_ = sock; }
    
    std::string getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }
    
    std::string getDescription() const { return description_; }
    void setDescription(const std::string& desc) { description_ = desc; }
    
    bool isValid() const { return socket_ != INVALID_SOCKET; }
    
private:
    SOCKET socket_;
    std::string name_;
    std::string description_;
};

// Winsocket 初始化函数
inline bool GenSocket_Init() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    return (result == 0);
}

// Winsocket 清理函数
inline void GenSocket_Cleanup() {
    WSACleanup();
}

// 创建服务器Socket函数
inline SOCKET GenSocket_CreateServerSocket(unsigned short port, GenSocketFlags flags = GenSocketFlags::None) {
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        return INVALID_SOCKET;
    }
    
    // 设置地址重用
    if ((flags & GenSocketFlags::AllowReuseAddr) != GenSocketFlags::None) {
        int reuse = 1;
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    }
    
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(port);
    
    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(serverSocket);
        return INVALID_SOCKET;
    }
    
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(serverSocket);
        return INVALID_SOCKET;
    }
    
    return serverSocket;
}

// 接受客户端连接函数
inline SOCKET GenSocket_AcceptClient(SOCKET serverSocket, sockaddr_in* clientAddr = nullptr, int* addrLen = nullptr) {
    if (clientAddr && addrLen) {
        return accept(serverSocket, reinterpret_cast<sockaddr*>(clientAddr), addrLen);
    } else {
        return accept(serverSocket, nullptr, nullptr);
    }
}

// 连接到服务器函数
inline SOCKET GenSocket_ConnectToServer(const char* ip, unsigned short port, GenSocketFlags flags = GenSocketFlags::None) {
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        return INVALID_SOCKET;
    }
    
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    
    // 支持域名解析
    if (inet_pton(AF_INET, ip, &serverAddr.sin_addr) <= 0) {
        // 尝试DNS解析
        struct addrinfo hints{}, *result = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        
        if (getaddrinfo(ip, nullptr, &hints, &result) != 0 || result == nullptr) {
            closesocket(clientSocket);
            return INVALID_SOCKET;
        }
        
        serverAddr.sin_addr = reinterpret_cast<sockaddr_in*>(result->ai_addr)->sin_addr;
        freeaddrinfo(result);
    }
    
    if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(clientSocket);
        return INVALID_SOCKET;
    }
    
    // 设置选项
    if ((flags & GenSocketFlags::EnableNoDelay) != GenSocketFlags::None) {
        int noDelay = 1;
        setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));
    }
    
    if ((flags & GenSocketFlags::EnableKeepAlive) != GenSocketFlags::None) {
        int keepAlive = 1;
        setsockopt(clientSocket, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char*>(&keepAlive), sizeof(keepAlive));
    }
    
    return clientSocket;
}

// 发送数据函数
inline int GenSocket_Send(SOCKET sock, const char* data, int length, int flags = 0) {
    if (sock == INVALID_SOCKET || data == nullptr || length <= 0) {
        return SOCKET_ERROR;
    }
    return send(sock, data, length, flags);
}

// 发送字符串函数
inline int GenSocket_SendString(SOCKET sock, const std::string& str, int flags = 0) {
    return GenSocket_Send(sock, str.c_str(), static_cast<int>(str.length()), flags);
}

// 接收数据函数
inline int GenSocket_Receive(SOCKET sock, char* buffer, int bufferSize, int flags = 0) {
    if (sock == INVALID_SOCKET || buffer == nullptr || bufferSize <= 0) {
        return SOCKET_ERROR;
    }
    return recv(sock, buffer, bufferSize, flags);
}

// 接收数据到字符串函数
inline int GenSocket_ReceiveToString(SOCKET sock, std::string& outStr, int bufferSize = 4096, int flags = 0) {
    if (sock == INVALID_SOCKET) {
        return SOCKET_ERROR;
    }
    
    std::vector<char> buffer(bufferSize);
    int received = recv(sock, buffer.data(), bufferSize, flags);
    
    if (received > 0) {
        outStr.assign(buffer.data(), received);
    }
    
    return received;
}

// 关闭Socket函数
inline void GenSocket_Close(SOCKET sock) {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
    }
}

// 获取最后错误代码
inline int GenSocket_GetLastError() {
    return WSAGetLastError();
}

// 获取错误消息字符串
inline std::string GenSocket_GetErrorMessage(int errorCode = 0) {
    if (errorCode == 0) {
        errorCode = GenSocket_GetLastError();
    }
    
    char* messageBuffer = nullptr;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<char*>(&messageBuffer),
        0,
        nullptr
    );
    
    std::string message;
    if (messageBuffer) {
        message = messageBuffer;
        LocalFree(messageBuffer);
        // 移除末尾的换行符
        while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
            message.pop_back();
        }
    }
    
    return message;
}

// 设置Socket选项的辅助函数
inline bool GenSocket_SetOption(SOCKET sock, int level, int optname, const void* optval, int optlen) {
    if (sock == INVALID_SOCKET || optval == nullptr) {
        return false;
    }
    return setsockopt(sock, level, optname, reinterpret_cast<const char*>(optval), optlen) == 0;
}

// 获取Socket选项的辅助函数
inline bool GenSocket_GetOption(SOCKET sock, int level, int optname, void* optval, int* optlen) {
    if (sock == INVALID_SOCKET || optval == nullptr || optlen == nullptr) {
        return false;
    }
    return getsockopt(sock, level, optname, reinterpret_cast<char*>(optval), optlen) == 0;
}

// 检查Socket是否可读（带超时）
inline bool GenSocket_IsReadable(SOCKET sock, int timeoutMilliseconds = 0) {
    if (sock == INVALID_SOCKET) {
        return false;
    }
    
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(sock, &readSet);
    
    timeval timeout{};
    timeout.tv_sec = timeoutMilliseconds / 1000;
    timeout.tv_usec = (timeoutMilliseconds % 1000) * 1000;
    
    int result = select(0, &readSet, nullptr, nullptr, timeoutMilliseconds > 0 ? &timeout : nullptr);
    return result > 0 && FD_ISSET(sock, &readSet);
}

// 检查Socket是否可写（带超时）
inline bool GenSocket_IsWritable(SOCKET sock, int timeoutMilliseconds = 0) {
    if (sock == INVALID_SOCKET) {
        return false;
    }
    
    fd_set writeSet;
    FD_ZERO(&writeSet);
    FD_SET(sock, &writeSet);
    
    timeval timeout{};
    timeout.tv_sec = timeoutMilliseconds / 1000;
    timeout.tv_usec = (timeoutMilliseconds % 1000) * 1000;
    
    int result = select(0, nullptr, &writeSet, nullptr, timeoutMilliseconds > 0 ? &timeout : nullptr);
    return result > 0 && FD_ISSET(sock, &writeSet);
}

// 获取本地地址信息
inline bool GenSocket_GetLocalAddress(SOCKET sock, sockaddr_in& outAddr) {
    if (sock == INVALID_SOCKET) {
        return false;
    }
    
    int addrLen = sizeof(outAddr);
    return getsockname(sock, reinterpret_cast<sockaddr*>(&outAddr), &addrLen) == 0;
}

// 获取远程地址信息
inline bool GenSocket_GetRemoteAddress(SOCKET sock, sockaddr_in& outAddr) {
    if (sock == INVALID_SOCKET) {
        return false;
    }
    
    int addrLen = sizeof(outAddr);
    return getpeername(sock, reinterpret_cast<sockaddr*>(&outAddr), &addrLen) == 0;
}

// 将sockaddr_in转换为IP字符串
inline std::string GenSocket_InetAddressToString(const sockaddr_in& addr) {
    char ipStr[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &addr.sin_addr, ipStr, INET_ADDRSTRLEN) == nullptr) {
        return "";
    }
    return std::string(ipStr);
}

// 服务端连接管理器类
class GenSocketServerManager {
public:
    GenSocketServerManager() : serverSocket_(INVALID_SOCKET), port_(0) {}
    
    ~GenSocketServerManager() {
        shutdown();
    }
    
    // 启动服务器
    bool start(unsigned short port, GenSocketFlags flags = GenSocketFlags::None) {
        if (serverSocket_ != INVALID_SOCKET) {
            return false; // 已经启动
        }
        
        serverSocket_ = GenSocket_CreateServerSocket(port, flags);
        if (serverSocket_ == INVALID_SOCKET) {
            return false;
        }
        
        port_ = port;
        return true;
    }
    
    // 停止服务器
    void shutdown() {
        // 关闭所有客户端连接
        for (auto& conn : clients_) {
            GenSocket_Close(conn.getSocket());
        }
        clients_.clear();
        
        if (serverSocket_ != INVALID_SOCKET) {
            GenSocket_Close(serverSocket_);
            serverSocket_ = INVALID_SOCKET;
        }
        port_ = 0;
    }
    
    // 接受新连接并添加到管理器
    std::shared_ptr<GenClientConnection> acceptNewConnection(const std::string& name = "", const std::string& description = "") {
        if (serverSocket_ == INVALID_SOCKET) {
            return nullptr;
        }
        
        sockaddr_in clientAddr{};
        int addrLen = sizeof(clientAddr);
        SOCKET clientSock = GenSocket_AcceptClient(serverSocket_, &clientAddr, &addrLen);
        
        if (clientSock == INVALID_SOCKET) {
            return nullptr;
        }
        
        auto connection = std::make_shared<GenClientConnection>(clientSock, name, description);
        clients_.push_back(*connection);
        
        // 返回最后一个元素的共享指针（需要重新获取因为vector可能重新分配）
        return std::make_shared<GenClientConnection>(clients_.back());
    }
    
    // 根据Socket查找连接
    GenClientConnection* findConnectionBySocket(SOCKET sock) {
        for (auto& conn : clients_) {
            if (conn.getSocket() == sock) {
                return &conn;
            }
        }
        return nullptr;
    }
    
    // 根据名称查找连接
    GenClientConnection* findConnectionByName(const std::string& name) {
        for (auto& conn : clients_) {
            if (conn.getName() == name) {
                return &conn;
            }
        }
        return nullptr;
    }
    
    // 移除连接（不关闭Socket）
    bool removeConnection(SOCKET sock) {
        for (auto it = clients_.begin(); it != clients_.end(); ++it) {
            if (it->getSocket() == sock) {
                clients_.erase(it);
                return true;
            }
        }
        return false;
    }
    
    // 关闭并移除连接
    bool closeAndRemoveConnection(SOCKET sock) {
        GenSocket_Close(sock);
        return removeConnection(sock);
    }
    
    // 获取所有连接
    std::vector<GenClientConnection>& getClients() { return clients_; }
    const std::vector<GenClientConnection>& getClients() const { return clients_; }
    
    size_t getClientCount() const { return clients_.size(); }
    
    bool isRunning() const { return serverSocket_ != INVALID_SOCKET; }
    unsigned short getPort() const { return port_; }
    SOCKET getServerSocket() const { return serverSocket_; }
    
private:
    SOCKET serverSocket_;
    unsigned short port_;
    std::vector<GenClientConnection> clients_;
};

// 扩展位：预留未来类或接口
// class GenSocketExtension1 { };
// class GenSocketExtension2 { };

#endif // GEN_SOCKET_H
