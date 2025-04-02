#include <linux/LinuxMDNSProvider.hpp>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <iostream>
#include <cstring>

namespace cspot_ng
{
    LinuxMDNSProvider::LinuxMDNSProvider()
        : m_threaded_poll(nullptr)
        , m_client(nullptr)
    {
    }

    LinuxMDNSProvider::~LinuxMDNSProvider()
    {
        if (m_client) {
            avahi_client_free(m_client);
        }

        if (m_threaded_poll) {
            avahi_threaded_poll_stop(m_threaded_poll);
            avahi_threaded_poll_free(m_threaded_poll);
        }
    }

    void LinuxMDNSProvider::initialize()
    {
        int error;

        m_threaded_poll = avahi_threaded_poll_new();
        if (!m_threaded_poll) {
            std::cerr << "Failed to create Avahi threaded poll object" << std::endl;
            return;
        }

        m_client = avahi_client_new(
            avahi_threaded_poll_get(m_threaded_poll),
            AVAHI_CLIENT_NO_FAIL,
            &LinuxMDNSProvider::client_callback,
            this,
            &error);

        if (!m_client) {
            if (error == AVAHI_ERR_NO_DAEMON) {
                std::cerr << "Failed to create Avahi client: Daemon not running" << std::endl;
                std::cerr << "Please ensure the Avahi daemon is installed and running." << std::endl;
                std::cerr << "You can start it with: 'sudo systemctl start avahi-daemon'" << std::endl;
            } else {
                std::cerr << "Failed to create Avahi client: " << avahi_strerror(error) << std::endl;
            }
            avahi_threaded_poll_free(m_threaded_poll);
            m_threaded_poll = nullptr;
            return;
        }

        avahi_threaded_poll_start(m_threaded_poll);
    }

    void LinuxMDNSProvider::set_hostname(const std::string& hostname)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::cout << "Setting hostname to: " << hostname << std::endl;
        m_hostname = hostname;
    }

    void LinuxMDNSProvider::register_service(
        const std::string& name,
        const std::string& type,
        const std::string& proto,
        const std::string& host,
        u_int16_t port,
        const std::map<std::string, std::string>& properties)
    {
        if (!m_client || avahi_client_get_state(m_client) != AVAHI_CLIENT_S_RUNNING) {
            std::cerr << "Client not running, cannot register service" << std::endl;
            return;
        }

        // Ensure type and protocol have proper format (with leading underscore)
        std::string formatted_type = type;
        std::string formatted_proto = proto;

        if (!type.empty() && type[0] != '_') {
            formatted_type = "_" + type;
        }

        if (!proto.empty() && proto[0] != '_') {
            formatted_proto = "_" + proto;
        }

        std::string service_type = formatted_type + "." + formatted_proto;
        std::string service_key = name + "." + service_type;

        std::cout << "Registering service with type: " << service_type << std::endl;

        avahi_threaded_poll_lock(m_threaded_poll);

        AvahiEntryGroup* group = nullptr;
        auto it = m_groups.find(service_key);
        if (it != m_groups.end()) {
            group = it->second;
        } else {
            group = avahi_entry_group_new(
                m_client,
                &LinuxMDNSProvider::entry_group_callback,
                this);

            if (!group) {
                std::cerr << "Failed to create entry group: "
                          << avahi_strerror(avahi_client_errno(m_client)) << std::endl;
                avahi_threaded_poll_unlock(m_threaded_poll);
                return;
            }

            m_groups[service_key] = group;
        }

        // Convert properties to Avahi TXT records
        AvahiStringList* txt_records = nullptr;
        for (const auto& prop : properties) {
            std::string txt_entry = prop.first + "=" + prop.second;
            txt_records = avahi_string_list_add(txt_records, txt_entry.c_str());
        }

        int ret = avahi_entry_group_add_service_strlst(
            group,
            AVAHI_IF_UNSPEC,
            AVAHI_PROTO_UNSPEC,
            AvahiPublishFlags(0),
            name.c_str(),
            service_type.c_str(),
            nullptr,  // Domain
            host.empty() ? nullptr : host.c_str(),
            port,
            txt_records);

        avahi_string_list_free(txt_records);

        if (ret < 0) {
            std::cerr << "Failed to add service: " << avahi_strerror(ret) << std::endl;
            avahi_threaded_poll_unlock(m_threaded_poll);
            return;
        }

        ret = avahi_entry_group_commit(group);
        if (ret < 0) {
            std::cerr << "Failed to commit entry group: " << avahi_strerror(ret) << std::endl;
        }

        std::cout << "Registered service: " << name << " of type: " << service_type << std::endl;
        std::cout << "Properties:" << std::endl;
        for (const auto& prop : properties) {
            std::cout << "  " << prop.first << ": " << prop.second << std::endl;
        }
        std::cout << "Host: " << (host.empty() ? "default" : host) << std::endl;
        std::cout << "Port: " << port << std::endl;
        std::cout << "Service key: " << service_key << std::endl;
        std::cout << "Service type: " << service_type << std::endl;
        std::cout << "Hostname: " << m_hostname << std::endl;
        std::cout << "------------------------" << std::endl;

        avahi_threaded_poll_unlock(m_threaded_poll);
    }

    void LinuxMDNSProvider::client_callback(
        AvahiClient* client,
        AvahiClientState state,
        void* userdata)
    {
        LinuxMDNSProvider* provider = static_cast<LinuxMDNSProvider*>(userdata);

        switch (state) {
            case AVAHI_CLIENT_S_RUNNING:
                // The server has started up successfully
                break;

            case AVAHI_CLIENT_FAILURE:
                std::cerr << "Client failure: " << avahi_strerror(avahi_client_errno(client)) << std::endl;
                avahi_threaded_poll_quit(provider->m_threaded_poll);
                break;

            case AVAHI_CLIENT_S_COLLISION:
            case AVAHI_CLIENT_S_REGISTERING:
                // Clear all registered services when the server is
                // registering or when there's a collision
                for (auto& group_pair : provider->m_groups) {
                    if (group_pair.second) {
                        avahi_entry_group_reset(group_pair.second);
                    }
                }
                break;

            case AVAHI_CLIENT_CONNECTING:
                // Do nothing
                break;
            default:
                std::cerr << "Unknown client state: " << state << std::endl;
                break;
        }
    }

    void LinuxMDNSProvider::entry_group_callback(
        AvahiEntryGroup* group,
        AvahiEntryGroupState state,
        void* userdata)
    {
        LinuxMDNSProvider* provider = static_cast<LinuxMDNSProvider*>(userdata);

        switch (state) {
            case AVAHI_ENTRY_GROUP_ESTABLISHED:
                // Service successfully established
                break;

            case AVAHI_ENTRY_GROUP_COLLISION:
                // Service name collision
                std::cerr << "Service name collision" << std::endl;
                break;

            case AVAHI_ENTRY_GROUP_FAILURE:
                std::cerr << "Entry group failure: "
                          << avahi_strerror(avahi_client_errno(provider->m_client)) << std::endl;
                break;

            case AVAHI_ENTRY_GROUP_UNCOMMITED:
            case AVAHI_ENTRY_GROUP_REGISTERING:
                // Do nothing
                break;
        }
    }
}
