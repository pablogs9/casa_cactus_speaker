#pragma once

#include <map>
#include <string>

namespace cspot_ng
{
    struct MDNSProvider
    {
        virtual void initialize() = 0;

        virtual void set_hostname(const std::string& hostname) = 0;

        virtual void register_service(
            const std::string& name,
            const std::string& type,
            const std::string& proto,
            const std::string& host,
            u_int16_t port,
            const std::map<std::string, std::string>& properties) = 0;
    };

};