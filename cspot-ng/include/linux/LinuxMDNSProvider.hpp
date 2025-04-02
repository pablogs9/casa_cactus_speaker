#pragma once

#include <interfaces/MDNSProvider.hpp>
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/thread-watch.h>
#include <memory>
#include <map>
#include <string>
#include <thread>
#include <mutex>

namespace cspot_ng
{
    class LinuxMDNSProvider : public MDNSProvider
    {
    public:
        LinuxMDNSProvider();
        ~LinuxMDNSProvider();

        void initialize() override;
        void set_hostname(const std::string& hostname) override;
        void register_service(
            const std::string& name,
            const std::string& type,
            const std::string& proto,
            const std::string& host,
            u_int16_t port,
            const std::map<std::string, std::string>& properties) override;

    private:
        AvahiThreadedPoll* m_threaded_poll;
        AvahiClient* m_client;
        std::map<std::string, AvahiEntryGroup*> m_groups;
        std::string m_hostname;
        std::mutex m_mutex;

        static void client_callback(
            AvahiClient* client,
            AvahiClientState state,
            void* userdata);

        static void entry_group_callback(
            AvahiEntryGroup* group,
            AvahiEntryGroupState state,
            void* userdata);
    };
}
