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
#if !defined(__MUTUALSOCKET_H__)
#define __MUTUALSOCKET_H__

#include <openssl/ssl.h>
#include <string>
#include "KmfMain.h"

#include "NetworkStatuses.h"


class MutualAuthSocket {
public:
    /// <summary>Initializes a new instance of the Kmf class.</summary>
    MutualAuthSocket(const int port, std::string caPem, std::string certPem, std::string keyPem);

    /// <summary>Executes the main modem host processing loop.</summary>
    int run();

private:
    int m_sock;
    int m_port;
    std::string m_caPem;
    std::string m_certPem;
    std::string m_keyPem;

    SSL_CTX *getServerContext(std::string caPem, std::string certPem, std::string keyPem);
    int getSocket(int portNum);
};

#endif