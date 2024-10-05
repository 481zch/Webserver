#include <ssl/ssl.h>
#include "log/log.h"

SSLServer::SSLServer(const char *certFile, const char *keyFile): m_certFile(certFile), 
                                                                 m_keyFile(keyFile),
                                                                 m_ctx(nullptr)
{}

SSLServer::~SSLServer()
{
    if (m_ctx) {
        SSL_CTX_free(m_ctx);
    }
}

bool SSLServer::init() 
{
        // 初始化 OpenSSL 库
        SSL_load_error_strings();
        OpenSSL_add_ssl_algorithms();

        // 创建新的 SSL_CTX
        m_ctx = SSL_CTX_new(TLS_server_method());
        if (!m_ctx) {
            LOG_ERROR("Failed to create SSL_CTX");
            ERR_print_errors_fp(stderr);
            return false;
        }

        // 加载证书文件
        if (SSL_CTX_use_certificate_file(m_ctx, m_certFile, SSL_FILETYPE_PEM) <= 0) {
            LOG_ERROR("Failed to load certificate: %s", m_certFile);
            ERR_print_errors_fp(stderr);
            SSL_CTX_free(m_ctx);
            return false;
        }

        // 加载密钥文件
        if (SSL_CTX_use_PrivateKey_file(m_ctx, m_keyFile, SSL_FILETYPE_PEM) <= 0) {
            LOG_ERROR("Failed to load private key: %s", m_keyFile);
            ERR_print_errors_fp(stderr);
            SSL_CTX_free(m_ctx);
            return false;
        }

        // 验证证书和密钥匹配
        if (!SSL_CTX_check_private_key(m_ctx)) {
            LOG_ERROR("Private key does not match the certificate public key");
            SSL_CTX_free(m_ctx);
            return false;
        }

        LOG_INFO("SSL server initialized with cert: %s and key: %s", m_certFile, m_keyFile);
        return true;
}

bool SSLServer::SSLGetConnection(int clientSocket, SSL *&ssl)
{
    // 创建新的 SSL 连接对象
    ssl = SSL_new(m_ctx);
    if (!ssl) {
        LOG_ERROR("Failed to create SSL object");
        ERR_print_errors_fp(stderr);
        return false;
    }

    // 将客户端的 socket 文件描述符绑定到 SSL 对象
    SSL_set_fd(ssl, clientSocket);

    // 执行 SSL 握手
    int ret = SSL_accept(ssl);
    if (ret <= 0) {
        int err = SSL_get_error(ssl, ret);
        LOG_ERROR("SSL handshake failed with error code: %d", err);
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        ssl = nullptr;
        return false;
    }

    LOG_INFO("SSL handshake successful for client socket: %d", clientSocket);
    return true;
}

