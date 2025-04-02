#pragma once

#include <interfaces/HTTPServer.hpp>
#include <thread>
#include <mutex>
#include <map>
#include <atomic>
#include <vector>
#include <functional>

namespace cspot_ng
{
    class LinuxHTTPServer : public HTTPServer
    {
    public:
        LinuxHTTPServer();
        ~LinuxHTTPServer();

        void initialize(uint16_t port) override;
        void register_get_handler(const std::string& path, RequestHandler handler) override;
        void register_post_handler(const std::string& path, RequestHandler handler) override;

    private:
        int m_socket;
        uint16_t m_port;
        std::atomic<bool> m_running;
        std::thread m_server_thread;
        std::map<std::string, RequestHandler> m_get_handlers;
        std::map<std::string, RequestHandler> m_post_handlers;
        std::mutex m_handlers_mutex;

        void server_thread_func();
        void handle_client(int client_socket);
        std::pair<std::string, std::string> parse_request_line(const std::string& line);
        std::map<std::string, std::string> parse_headers(const std::vector<std::string>& lines);
        HTTPResponse process_request(const std::string& method,
                                    const std::string& path,
                                    const std::map<std::string, std::string>& headers,
                                    const std::string& body);
        void send_response(int client_socket, const HTTPResponse& response);
    };
}
