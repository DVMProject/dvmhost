// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Key Management Facility
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Key Management Facility
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Natalie Moore
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
*
*/

#include "MutualAuthSocket.h"
#include "Defines.h"
#include "common/Log.h"
#include "common/network/json/json.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Buffer size to be used for transfers */
#define BUFSIZE 1024

MutualAuthSocket::MutualAuthSocket(const int port, std::string caPem, std::string certPem, std::string keyPem) :
 m_sock(),
 m_port(port),
 m_caPem(caPem),
 m_certPem(certPem),
 m_keyPem(keyPem)
{
    // stub
}

/// <summary>
/// Executes the main modem KMF processing loop.
/// </summary>
/// <returns>Zero if successful, otherwise error occurred.</returns>
int MutualAuthSocket::run()
{
    static char buffer[BUFSIZE];
    struct sockaddr_in sin;
    socklen_t sin_len;
    SSL_CTX *ctx;
    SSL *ssl;
    int listen_fd, net_fd, rc, len;

    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    if (!(ctx = getServerContext(m_caPem, m_certPem, m_keyPem))) {
        return -1;
    }

    if ((listen_fd = getSocket(m_port)) < 0) {
        SSL_CTX_free(ctx);
        ::fatal("Unable to get socket!");
    }

    while (!g_killed) {
        sin_len = sizeof(sin);
        if ((net_fd = accept(listen_fd,
                             (struct sockaddr *) &sin,
                             &sin_len)) < 0) {
            LogError(LOG_KMFSOCK, "Failure accepting connection!");
            continue;
        }

        if (!(ssl = SSL_new(ctx))) {
            LogError(LOG_KMFSOCK, "Could not get an SSL handle!");
            close(net_fd);
            continue;
        }

        SSL_set_fd(ssl, net_fd);

        if ((rc = SSL_accept(ssl)) != 1) {
            LogError(LOG_KMFSOCK, "Could not perform SSL handshake!");
            if (rc != 0) {
                SSL_shutdown(ssl);
            }
            SSL_free(ssl);
            continue;
        }

        /* Print success connection message on the server */
        LogDebug(LOG_KMFSOCK, "SSL handshake successful with %s:%d\n", inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));

        // Send the hello
        json::object response = json::object();
        int statusVal = NET_STATUS_HELLO;
        response["status"].set<int>(statusVal);
        json::value v = json::value(response);
        std::string json = std::string(v.serialize());
        SSL_write(ssl, json.c_str(), json.length());

        //TODO: add timeout here

        while ((len = SSL_read(ssl, buffer, BUFSIZE)) != 0) {
            if (len < 0) {
                LogError(LOG_KMFSOCK, "SSL_read failed!");
                break;
            } else {
                // receive hello
                json::value v;
                std::string temp(buffer, len);
                std::string err = json::parse(v, temp);
                if (!err.empty()) {
                    LogError(LOG_KMFSOCK, "Error parsing JSON!");
                    break;
                }

                if (!v.is<json::object>()) {
                    LogError(LOG_KMFSOCK, "Response was not a valid JSON object!");
                    break;
                }

                json::object obj = v.get<json::object>();

                if (obj["status"].get<int>() == NET_STATUS_HELLO) {
                    LogInfo("Successful hello from %s", inet_ntoa(sin.sin_addr));
                }

                break;
            }
        }

        SSL_shutdown(ssl);
        SSL_free(ssl);
    }

    close(listen_fd);
    return 0;
}

SSL_CTX *MutualAuthSocket::getServerContext(std::string caPem, std::string certPem, std::string keyPem) {
    SSL_CTX *context;

    if (!(context = SSL_CTX_new(SSLv23_server_method()))) {
        ::fatal("Could not get new SSL context (this should not happen, and is a fatal error).\n");
    }

    if (SSL_CTX_load_verify_locations(context, caPem.c_str(), NULL) != 1) {
        SSL_CTX_free(context);
        ::fatal("Could not load CA certificate (does it exist and is it in PEM format?)\n");
    }

    SSL_CTX_set_client_CA_list(context, SSL_load_client_CA_file(caPem.c_str()));

    if (SSL_CTX_use_certificate_file(context, certPem.c_str(), SSL_FILETYPE_PEM) != 1) {
        SSL_CTX_free(context);
        ::fatal("Could not load server certificate (does it exist and is it in PEM format?)\n");
    }

    if (SSL_CTX_use_PrivateKey_file(context, keyPem.c_str(), SSL_FILETYPE_PEM) != 1) {
        SSL_CTX_free(context);
        ::fatal("Could not load server key (does it exist and is it in PEM format?)\n");
    }

    if (SSL_CTX_check_private_key(context) != 1) {
        SSL_CTX_free(context);
        ::fatal("Server cert/key mismatch!\n");
    }

    SSL_CTX_set_mode(context, SSL_MODE_AUTO_RETRY);

    // we should verify the client (that is the point of mutual auth, after all)
    SSL_CTX_set_verify(context,
                       SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                       NULL);

    // verify the cert is signed by CA
    SSL_CTX_set_verify_depth(context, 1);

    return context;
}

int MutualAuthSocket::getSocket(int portNum) {
    struct sockaddr_in sin;
    int sock, val;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        ::fatal("Cannot create socket!\n");
    }

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
        close(sock);
        ::fatal("Could not set SO_REUSEADDR flag on socket!\n");
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(portNum);

    if (bind(sock, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
        close(sock);
        ::fatal("Could not bind socket!\n");
    }

    if (listen(sock, SOMAXCONN) < 0) {
        close(sock);
        ::fatal("Failed to listen on socket!\n");
    }

    return sock;
}