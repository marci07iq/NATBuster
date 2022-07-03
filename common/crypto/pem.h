#pragma once

#include <memory>
#include <string>

#include <openssl/evp.h>
#include <openssl/pem.h>

namespace NATBuster::Common::Crypto {
    class PEMRaw;

    typedef std::shared_ptr<PEMRaw> PEM;

    class PEMRaw {
        
        PEMRaw();

        void save(std::string filename);

        ~PEMRaw();
    };
}