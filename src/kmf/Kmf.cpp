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
#include "Defines.h"
#include "common/lookups/RSSIInterpolator.h"
#include "common/Log.h"
#include "common/StopWatch.h"
#include "common/Thread.h"
#include "common/ThreadFunc.h"
#include "common/Utils.h"
#include "Kmf.h"

#include <cstdio>
#include <algorithm>
#include <functional>
#include <memory>
#include <mutex>
#include <unistd.h>

// ---------------------------------------------------------------------------
//  Public Class Members
// ---------------------------------------------------------------------------

/// <summary>
/// Initializes a new instance of the Host class.
/// </summary>
/// <param name="confFile">Full-path to the configuration file.</param>
Kmf::Kmf(const std::string& confFile) :
    m_confFile(confFile),
    m_conf(),
    m_port(),
    m_caPem(),
    m_certPem(),
    m_keyPem()
{
    /* stub */
}

/// <summary>
/// Finalizes a instance of the Kmf class.
/// </summary>
Kmf::~Kmf() = default;

bool Kmf::readParams()
{
    yaml::Node kmfConf = m_conf["kmf"];

    m_port = kmfConf["port"].as<int>(8234);
    ::LogInfo("KMF Port: %d", m_port);

    if (m_port < 1 || m_port > 65535) {
        ::fatal("Invalid KMF port number (must be 1-65535): %d\n", m_port);
    }

    m_caPem = kmfConf["caPem"].as<std::string>("");
    ::LogInfo("Secure Socket CA: %s", m_caPem.c_str());
    if (strcmp(m_caPem.c_str(), "") == 0) {
        ::fatal("You did not specify a location for a CA PEM file. I will die now.");
    }
    
    m_certPem = kmfConf["certPem"].as<std::string>("");
    ::LogInfo("Secure Socket Cert: %s", m_certPem.c_str());
    if (strcmp(m_certPem.c_str(), "") == 0) {
        ::fatal("You did not specify a location for a certificate PEM file. I will die now.");
    }
    
    m_keyPem = kmfConf["keyPem"].as<std::string>("");
    ::LogInfo("Secure Socket Key: %s", m_keyPem.c_str());
    if (strcmp(m_keyPem.c_str(), "") == 0) {
        ::fatal("You did not specify a location for a key PEM file. I will die now.");
    }

    std::string masterKeyTypeS = kmfConf["masterKeyType"].as<std::string>("");
    ::LogInfo("Master Key Type: %s", masterKeyTypeS.c_str());
    if (strcmp(masterKeyTypeS.c_str(), "") == 0) {
        ::fatal("You did not specify a master key type. I will die now.");
    }
    if (strcmp(masterKeyTypeS.c_str(), "configFile") == 0) {
        LogWarning(LOG_KMF, "*** WARNING WARNING WARNING WARNING ***");
        LogWarning(LOG_KMF, "Config file-based master key is very insecure!");
        LogWarning(LOG_KMF, "Anybody with access to your config file will have access to ALL of your keys!");
        LogWarning(LOG_KMF, "This setting is intended for DEVELOPMENT ONLY.");
        LogWarning(LOG_KMF, "By continuing you accept these risks.");
        LogWarning(LOG_KMF, "*** WARNING WARNING WARNING WARNING ***");
        m_masterKeyType = 1;
    }
    
    return true;
}

/// <summary>
/// Executes the main modem KMF processing loop.
/// </summary>
/// <returns>Zero if successful, otherwise error occurred.</returns>
int Kmf::run()
{
    bool ret = false;
    try {
        ret = yaml::Parse(m_conf, m_confFile.c_str());
        if (!ret) {
            ::fatal("cannot read the configuration file, %s\n", m_confFile.c_str());
        }
    }
    catch (yaml::OperationException const& e) {
        ::fatal("cannot read the configuration file - %s (%s)", m_confFile.c_str(), e.message());
    }

    ::LogInfo(__BANNER__ "\r\n" __PROG_NAME__ " " __VER__ " (built " __BUILD__ ")\r\n" \
        "Copyright (c) 2024 Natalie Moore, Bryan Biedenkapp (N2PLL) and DVMProject (https://github.com/dvmproject) Authors.\r\n" \
        ">> Key Management Facility\r\n");

    // read base parameters from configuration
    ret = readParams();
    if (!ret)
        return EXIT_FAILURE;

    int masRet;
    MutualAuthSocket* mas = new MutualAuthSocket(m_port, m_caPem, m_certPem, m_keyPem);
    masRet = mas->run();
    delete mas;
    return masRet;
}