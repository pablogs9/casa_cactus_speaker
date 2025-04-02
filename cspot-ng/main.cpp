#include <iostream>

#include <interfaces/MDNSProvider.hpp>
#include <interfaces/HTTPServer.hpp>
#include <interfaces/Crypto.hpp>

#include <ZeroConf.hpp>
#include <Context.hpp>

#include <linux/LinuxCrypto.hpp>
#include <linux/LinuxCrytoMbedTLS.hpp>
#include <linux/LinuxTCPClient.hpp>
#include <linux/LinuxHTTPClient.hpp>
#include <linux/LinuxHTTPServer.hpp>
#include <linux/LinuxMDNSProvider.hpp>


using namespace cspot_ng;

int main()
{
    LinuxMDNSProvider mdns;
    LinuxTCPClient tcp_client;
    LinuxHTTPServer http_server;
    LinuxHTTPClient http_client;
    LinuxCrytoMbedTLS crypto_zero_conf;

    ZeroConf zeroconf(mdns, http_server, crypto_zero_conf);

    // Wait for authentication success
    while (!zeroconf.is_auth_success())
    {
        // Handle other tasks or sleep
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "Waiting for authentication..." << std::endl;

    }

    LinuxCrytoMbedTLS context_crypto;
    Context context(zeroconf.get_blob(), http_client, tcp_client, context_crypto);

    if (!context.connect())
    {
        std::cerr << "Failed to connect to AP" << std::endl;
        return -1;
    }

    auto token = context.authenticate();

    if (token.empty())
    {
        std::cerr << "Authentication failed" << std::endl;
        return -1;
    }

    while (!zeroconf.is_closed())
    {
        // Handle other tasks or sleep
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "Waiting for close..." << std::endl;
    }

    return 0;
}