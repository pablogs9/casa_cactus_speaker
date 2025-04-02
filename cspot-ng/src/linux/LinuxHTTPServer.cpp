#include <linux/LinuxHTTPServer.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <cstring>

namespace cspot_ng
{
    LinuxHTTPServer::LinuxHTTPServer()
        : m_socket(-1)
        , m_port(0)
        , m_running(false)
    {
    }

    LinuxHTTPServer::~LinuxHTTPServer()
    {
        if (m_running) {
            m_running = false;
            if (m_server_thread.joinable()) {
                m_server_thread.join();
            }
        }

        if (m_socket != -1) {
            ::close(m_socket);
        }
    }

    void LinuxHTTPServer::initialize(uint16_t port)
    {
        m_port = port;

        m_socket = ::socket(AF_INET, SOCK_STREAM, 0);
        if (m_socket < 0) {
            throw std::runtime_error("Failed to create server socket");
        }

        // Allow reuse of the address
        int opt = 1;
        if (::setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            ::close(m_socket);
            throw std::runtime_error("Failed to set socket options");
        }

        struct sockaddr_in server_addr;
        std::memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);

        if (::bind(m_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            ::close(m_socket);
            throw std::runtime_error("Failed to bind server socket");
        }

        if (::listen(m_socket, 5) < 0) {
            ::close(m_socket);
            throw std::runtime_error("Failed to listen on server socket");
        }

        m_running = true;
        m_server_thread = std::thread(&LinuxHTTPServer::server_thread_func, this);
    }

    void LinuxHTTPServer::register_get_handler(const std::string& path, RequestHandler handler)
    {
        std::lock_guard<std::mutex> lock(m_handlers_mutex);
        std::cout << "Registering GET handler for path: " << path << std::endl;
        m_get_handlers[path] = handler;
    }

    void LinuxHTTPServer::register_post_handler(const std::string& path, RequestHandler handler)
    {
        std::lock_guard<std::mutex> lock(m_handlers_mutex);
        std::cout << "Registering POST handler for path: " << path << std::endl;
        m_post_handlers[path] = handler;
    }

    void LinuxHTTPServer::server_thread_func()
    {
        while (m_running) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_socket = ::accept(m_socket, (struct sockaddr*)&client_addr, &client_len);

            if (client_socket < 0) {
                if (m_running) {
                    std::cerr << "Failed to accept client connection: " << strerror(errno) << std::endl;
                }
                continue;
            }

            // Handle client in a separate thread
            std::thread client_thread(&LinuxHTTPServer::handle_client, this, client_socket);
            client_thread.detach();
        }
    }

    void LinuxHTTPServer::handle_client(int client_socket)
    {
        std::cout << "Client connected: " << client_socket << std::endl;

        // Read request
        std::vector<char> buffer(4096);
        ssize_t bytes_read = ::recv(client_socket, buffer.data(), buffer.size(), 0);

        if (bytes_read <= 0) {
            ::close(client_socket);
            return;
        }

        std::string request_data(buffer.data(), bytes_read);
        std::istringstream request_stream(request_data);

        // Parse request line
        std::string request_line;
        std::getline(request_stream, request_line);

        // Remove carriage return if present
        if (!request_line.empty() && request_line.back() == '\r') {
            request_line.pop_back();
        }

        std::istringstream line_stream(request_line);
        std::string method, path, version;
        line_stream >> method >> path >> version;

        // Parse headers
        std::vector<std::string> header_lines;
        std::string header_line;
        while (std::getline(request_stream, header_line) && !header_line.empty()) {
            if (header_line.back() == '\r') {
                header_line.pop_back();
            }
            // Print size of header line
            if (header_line.size() == 0) {
                break;
            }
            header_lines.push_back(header_line);
        }


        auto headers = parse_headers(header_lines);

        // Parse body if present
        std::string body;
        if (headers.find("Content-Length") != headers.end()) {
            size_t content_length = std::stoul(headers["Content-Length"]);
            if (content_length > 0) {
                std::vector<char> body_buffer(content_length);
                request_stream.read(body_buffer.data(), content_length);
                body.assign(body_buffer.data(), content_length);
            }
        }

        // Process request
        auto response = process_request(method, path, headers, body);

        // Send response
        send_response(client_socket, response);

        // Close connection
        ::close(client_socket);
    }

    std::pair<std::string, std::string> LinuxHTTPServer::parse_request_line(const std::string& line)
    {
        size_t pos = line.find(':');
        if (pos == std::string::npos) {
            return {"", ""};
        }

        std::string name = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        // Trim leading/trailing spaces
        while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) {
            value.erase(0, 1);
        }

        return {name, value};
    }

    std::map<std::string, std::string> LinuxHTTPServer::parse_headers(const std::vector<std::string>& lines)
    {
        std::map<std::string, std::string> headers;
        for (const auto& line : lines) {
            auto [name, value] = parse_request_line(line);
            if (!name.empty()) {
                headers[name] = value;
            }
        }
        return headers;
    }

    HTTPServer::HTTPResponse LinuxHTTPServer::process_request(
        const std::string& method,
        const std::string& path,
        const std::map<std::string, std::string>& headers,
        const std::string& body)
    {
        std::lock_guard<std::mutex> lock(m_handlers_mutex);

        // Take into account path ? values
        size_t pos = path.find('?');
        std::string base_path = path.substr(0, pos);
        std::string query_string = (pos != std::string::npos) ? path.substr(pos + 1) : "";

        // Check for GET or POST handlers
        if (method == "GET") {
            auto it = m_get_handlers.find(base_path);
            if (it != m_get_handlers.end()) {
                return it->second(body);
            }
        } else if (method == "POST") {
            auto it = m_post_handlers.find(base_path);
            if (it != m_post_handlers.end()) {
                return it->second(body);
            }
        }
        else {
            std::cout << "Unknown method: " << method << std::endl;
        }

        // Default 404 response
        return {"Not Found", {{"Content-Type", "text/plain"}}, 404};
    }

    void LinuxHTTPServer::send_response(int client_socket, const HTTPResponse& response)
    {
        std::ostringstream response_stream;
        response_stream << "HTTP/1.1 " << response.status << " ";

        // Status text
        switch (response.status) {
            case 200: response_stream << "OK"; break;
            case 400: response_stream << "Bad Request"; break;
            case 404: response_stream << "Not Found"; break;
            case 500: response_stream << "Internal Server Error"; break;
            default: response_stream << "Unknown"; break;
        }

        response_stream << "\r\n";

        // Headers
        for (const auto& [name, value] : response.headers) {
            response_stream << name << ": " << value << "\r\n";
        }

        // Content-Length
        response_stream << "Content-Length: " << response.body.size() << "\r\n";

        // End of headers
        response_stream << "\r\n";

        // Body
        response_stream << response.body;

        std::string response_str = response_stream.str();
        ::send(client_socket, response_str.c_str(), response_str.size(), 0);
    }
}
