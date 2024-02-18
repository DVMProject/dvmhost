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
#if !defined(__KMF_H__)
#define __KMF_H__

#include "Defines.h"
#include "common/Timer.h"
#include "common/yaml/Yaml.h"
#include "KmfMain.h"
#include "MutualAuthSocket.h"

#include <string>
#include <unordered_map>
#include <functional>

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the core KMF service logic.
// ---------------------------------------------------------------------------

class Kmf {
public:
    /// <summary>Initializes a new instance of the Kmf class.</summary>
    Kmf(const std::string& confFile);
    /// <summary>Finalizes a instance of the Kmf class.</summary>
    ~Kmf();

    /// <summary>Parses the config file and stores values into private vars.</summary>
    bool readParams();

    /// <summary>Executes the main modem host processing loop.</summary>
    int run();

private:
    const std::string& m_confFile;
    yaml::Node m_conf;

    int m_port;
    std::string m_caPem;
    std::string m_certPem;
    std::string m_keyPem;
    int m_masterKeyType;    // 0: "": invalid;
                            // 1: "configFile": file-based (insecure; for dev);
                            // 2: "sc": smart card (planed);
                            // 3: "hsm": YubiHSM (planned);

};

#endif // __HOST_H__
