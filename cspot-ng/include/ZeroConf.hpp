#pragma once

#include <interfaces/mDNSProvider.hpp>
#include <interfaces/HTTPServer.hpp>
#include <interfaces/Crypto.hpp>

#include <LoginBlob.hpp>

namespace cspot_ng
{
    struct ZeroConf
    {
        ZeroConf(mDNSProvided & mdns, HTTPServer & server, Crypto & crypto)
            : mdns_(mdns)
            , server_(server)
            , device_name_("TESTNAME")
            , login_blob_(device_name_, crypto)
        {
            // ###############################
            // ##### MDNS Initialization #####
            // ###############################

            // Initialize the mDNS service
            mdns_.initialize();

            // Set the hostname
            mdns_.set_hostname(device_name_);

            // Register the mDNS service
            mdns_.register_service(
                device_name_,
                "_spotify-connect",
                "_tcp",
                "",
                0,
                {{"VERSION", "1.0"}, {"CPath", "/spotify_info"}, {"Stack", "SP"}});

            // #############################
            // ##### HTTP Server Setup #####
            // #############################

            // Initialize the HTTP server
            const uint16_t server_port = 7864;
            server_.initialize(server_port);

            // Register the HTTP GET handler for /spotify_info
            server_.register_get_handler(
                "/spotify_info",
                [&](const std::string& request) {
                    // Handle the GET request

                    HTTPServer::HTTPResponse response;
                    response.status = 200;
                    response.headers["Content-Type"] = "application/json";
                    response.body = login_blob_.get_info();
                    return response;
                });

            // Register the HTTP POST handler for /spotify_info
            server_.register_post_handler(
                "/spotify_info",
                [&](const std::string& request) {
                    // Feed the blob with the request data
                    auto info = HTTPServer::decode_query(request);

                    if(login_blob_.set_info(info))
                    {
                        // Notify auth success
                        auth_success_ = true;
                    }


                    // Prepare the response
                    nlohmann::json obj;
                    obj["status"] = 101;
                    obj["spotifyError"] = 0;
                    obj["statusString"] = "ERROR-OK";

                    // Send the response
                    HTTPServer::HTTPResponse response;
                    response.status = 200;
                    response.headers["Content-Type"] = "application/json";
                    response.body = obj.dump();
                    return response;
                });

            // Register the HTTP GET handler for /close
            server_.register_get_handler(
                "/close",
                [&](const std::string& request) {
                    // Handle the close request
                    closed_ = true;

                    // Send empty response
                    HTTPServer::HTTPResponse response;
                    response.status = 200;
                    response.body = "";
                    return response;
                });
        }

        bool is_auth_success() const
        {
            return auth_success_;
        }

        bool is_closed() const
        {
            return closed_;
        }

    private:
        mDNSProvided & mdns_;
        HTTPServer & server_;

        const std::string device_name_;
        LoginBlob login_blob_;

        bool auth_success_ = false;
        bool closed_ = false;
    };
};