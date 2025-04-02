#pragma once

#include <interfaces/MDNSProvider.hpp>
#include <interfaces/HTTPServer.hpp>
#include <interfaces/Crypto.hpp>

#include <LoginBlob.hpp>

namespace cspot_ng
{
    struct ZeroConf
    {
        ZeroConf(MDNSProvider & mdns, HTTPServer & server, Crypto & crypto)
            : mdns_(mdns)
            , server_(server)
            , device_name_("CSpot player")
            , login_blob_(device_name_, crypto)
        {
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
                    std::cout << "GET /spotify_info" << std::endl;

                    // Handle the GET request
                    HTTPServer::HTTPResponse response;
                    response.status = 200;
                    response.headers["Content-Type"] = "application/json";
                    response.body = login_blob_.get_info();
                    std::cout << "Response: " << response.body << std::endl;
                    return response;
                });

            // Register the HTTP POST handler for /spotify_info
            server_.register_post_handler(
                "/spotify_info",
                [&](const std::string& request) {
                    std::cout << "POST /spotify_info" << std::endl;

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
                    std::cout << "GET /close" << std::endl;

                    // Handle the close request
                    closed_ = true;

                    // Send empty response
                    HTTPServer::HTTPResponse response;
                    response.status = 200;
                    response.body = "";
                    return response;
                });

            // ###############################
            // ##### MDNS Initialization #####
            // ###############################

            // Initialize the mDNS service
            mdns_.initialize();

            // Set the hostname
            mdns_.set_hostname("pgarrido");

            // Register the mDNS service
            mdns_.register_service(
                device_name_,
                "_spotify-connect",
                "_tcp",
                "",
                server_port,
                {{"VERSION", "1.0"}, {"CPath", "/spotify_info"}, {"Stack", "SP"}});

        }

        bool is_auth_success() const
        {
            return auth_success_;
        }

        bool is_closed() const
        {
            return closed_;
        }

        const LoginBlob & get_blob() const
        {
            return login_blob_;
        }

    private:
        MDNSProvider & mdns_;
        HTTPServer & server_;

        const std::string device_name_;
        LoginBlob login_blob_;

        bool auth_success_ = false;
        bool closed_ = false;
    };
};