
#include <interfaces/mDNSProvider.hpp>
#include <interfaces/HTTPServer.hpp>
#include <interfaces/Crypto.hpp>

#include <ZeroConf.hpp>

#include "json.hpp"

#include "protobuf/keyexchange.pb.h"

using namespace cspot_ng;

int main()
{
    mDNSProvided & mdns = *static_cast<mDNSProvided*>(nullptr); // Replace with actual implementation
    HTTPServer & server = *static_cast<HTTPServer*>(nullptr); // Replace with actual implementation
    Crypto & crypto = *static_cast<Crypto*>(nullptr); // Replace with actual implementation

    ZeroConf zeroconf(mdns, server, crypto);

    // Wait for authentication success
    while (!zeroconf.is_auth_success())
    {
        // Handle other tasks or sleep
    }

    while (!zeroconf.is_closed())
    {
        // Handle other tasks or sleep
    }

    return 0;
}