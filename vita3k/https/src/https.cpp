// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <boost/algorithm/string/trim.hpp>
#include <https/functions.h>

#ifdef WIN32
#include <io.h>
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET abs_socket;
#else
#include <netdb.h>
#include <sys/socket.h>
typedef int abs_socket;
#endif

#include <openssl/err.h>
#include <openssl/md5.h>
#include <openssl/ssl.h>

#include <util/log.h>

namespace https {

static void close_socket(const abs_socket sockfd) {
#ifdef WIN32
    closesocket(sockfd);
    WSACleanup();
#else
    close(sockfd);
#endif // WIN32
}

static void close_ssl(SSL *ssl) {
    SSL_shutdown(ssl);
    SSL_free(ssl);
}

struct Https {
    SSL *ssl = nullptr;
    abs_socket sockfd = 0;
};

static Https init(const std::string &url, const std::string &method = "GET", const uint64_t downloaded_file_size = 0) {
#ifdef WIN32
    // Initialize Winsock
    WORD versionWanted = MAKEWORD(2, 2);
    WSADATA wsaData;
    WSAStartup(versionWanted, &wsaData);
#endif

    // Initialize SSL
    const auto ctx = SSL_CTX_new(SSLv23_client_method());
    if (!ctx) {
        LOG_ERROR("Error creating SSL context");
        return {};
    }

    Https https{};

    // Create socket
    https.sockfd = socket(AF_INET, SOCK_STREAM, 0);
#ifdef WIN32
    if (https.sockfd == INVALID_SOCKET) {
        LOG_ERROR("ERROR opening socket: {}", log_hex(WSAGetLastError()));
#else
    if (https.sockfd < 0) {
        LOG_ERROR("ERROR opening socket: {}", log_hex(https.sockfd));
#endif
        SSL_CTX_free(ctx);
        return {};
    }

    // Parse URL to get host and uri
    std::string host, uri;
    size_t start = url.find("://");
    if (start != std::string::npos) {
        start += 3; // skip "://"
        size_t end = url.find('/', start);
        if (end != std::string::npos) {
            host = url.substr(start, end - start);
            uri = url.substr(end);
        } else {
            host = url.substr(start);
            uri = "/";
        }
    }

    // Set address info option
    const addrinfo hints = {
        AI_PASSIVE, /* For wildcard IP address */
        AF_UNSPEC, /* Allow IPv4 or IPv6 */
        SOCK_DGRAM, /* Datagram socket */
        0, /* Any protocol */
    };
    addrinfo *result = { 0 };

    // Get address info with using host and port
    auto ret = getaddrinfo(host.c_str(), "https", &hints, &result);

    // Check if getaddrinfo failed or unable to resolve address for host
    if ((ret < 0) || !result) {
        if (!result)
            LOG_ERROR("Unable to resolve address for host: {}", host);
        else {
            LOG_ERROR("getaddrinfo error: {}", gai_strerror(ret));
            freeaddrinfo(result);
        }
        SSL_CTX_free(ctx);
        close_socket(https.sockfd);
        return {};
    }

    // Connect to host
    ret = connect(https.sockfd, result->ai_addr, static_cast<uint32_t>(result->ai_addrlen));
    if (ret < 0) {
        LOG_ERROR("connect({},...) = {}, errno={}({})", https.sockfd, ret, errno, strerror(errno));
        freeaddrinfo(result);
        SSL_CTX_free(ctx);
        close_socket(https.sockfd);
        return {};
    }

    // Check if socket is connected
    int error = 0;
    socklen_t errlen = sizeof(error);
    ret = getsockopt(https.sockfd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&error), &errlen);
    if (ret < 0) {
        LOG_ERROR("getsockopt({}, SOL_SOCKET, SO_ERROR, ...) failed: {}", https.sockfd, strerror(errno));
        freeaddrinfo(result);
        SSL_CTX_free(ctx);
        close_socket(https.sockfd);
        return {};
    }
    if (error != 0) {
        LOG_ERROR("connect({}, ...) failed: {}", https.sockfd, error);
        freeaddrinfo(result);
        SSL_CTX_free(ctx);
        close_socket(https.sockfd);
        return {};
    }

    // Create and connect SSL
    https.ssl = SSL_new(ctx);
    SSL_set_fd(https.ssl, static_cast<uint32_t>(https.sockfd));
    if (SSL_connect(https.ssl) <= 0) {
        char err_buf[256];
        ERR_error_string_n(ERR_get_error(), err_buf, sizeof(err_buf));
        LOG_ERROR("Error establishing SSL connection: {}", err_buf);
        freeaddrinfo(result);
        SSL_CTX_free(ctx);
        close_ssl(https.ssl);
        close_socket(https.sockfd);
        return {};
    }

    // Send HTTP GET request to extracted URI
    std::string request = method + " " + uri + " HTTP/1.1\r\n";
    request += "Host: " + host + "\r\n";
    if (downloaded_file_size > 0) {
        request += "Accept-Ranges: bytes\r\n";
        request += "Range: bytes=" + std::to_string(downloaded_file_size) + "-\r\n";
    }
    request += "User-Agent: OpenSSL/1.1.1\r\n";
    request += "Connection: close\r\n\r\n";

    // Send request to host
    if (SSL_write(https.ssl, request.c_str(), static_cast<uint32_t>(request.length())) <= 0) {
        char err_buf[256];
        ERR_error_string_n(ERR_get_error(), err_buf, sizeof(err_buf));
        freeaddrinfo(result);
        SSL_CTX_free(ctx);
        close_ssl(https.ssl);
        close_socket(https.sockfd);
        LOG_ERROR("Error sending request: {},\n{}", err_buf, request);
        return {};
    }

    // Free address info and ssl ctx
    freeaddrinfo(result);
    SSL_CTX_free(ctx);

    return https;
}

std::string get_web_response(const std::string &url, const std::string &method) {
    // Initialize SSL and socket connection
    auto https = init(url, method);
    if (!https.ssl)
        return {};

    // Create read buffer and response string
    std::vector<char> read_buffer(1024);
    std::string response;
    uint64_t downloaded_size = 0;

    // Read response from SSL connection and append to response string
    while (const auto bytes_read = SSL_read(https.ssl, read_buffer.data(), static_cast<uint32_t>(read_buffer.size()))) {
        response += std::string(read_buffer.data(), bytes_read);
        downloaded_size += bytes_read;
    }

    // Close SSL and socket connection
    close_ssl(https.ssl);
    close_socket(https.sockfd);

    // Check if the response is resource not found
    if (response.find("HTTP/1.1 404 Not Found") != std::string::npos) {
        LOG_ERROR("404 Not Found");
        return {};
    }

    return response;
}

std::string get_web_regex_result(const std::string &url, const std::regex &regex) {
    std::string result;

    // Get the response of the web
    const auto response = https::get_web_response(url);

    // Check if the response is not empty
    if (!response.empty()) {
        // Get the content of the response (without headers)
        const auto content = response.substr(response.find("\r\n\r\n") + 4);

        std::smatch match;
        // Check if the response matches the regex
        if (std::regex_search(content, match, regex)) {
            result = match[1];
        } else
            LOG_ERROR("No success found regex: {}", content);
    }

    return result;
}

static uint64_t get_file_size(const std::string &header) {
    uint64_t file_size = 0;

    std::smatch match;
    // Check if the response matches the regex for content length
    if (std::regex_search(header, match, std::regex("Content-Length: (\\d+)"))) {
        const std::string file_size_str = match[1];

        // Check if the file size from content length is a number
        if (std::all_of(file_size_str.begin(), file_size_str.end(), ::isdigit))
            file_size = std::stoll(file_size_str);
    } else
        LOG_ERROR("No success found regex: {}", header);

    return file_size;
}

static std::string convert_md5_bytes_to_str(unsigned char *md5_bytes) {
    std::string content_md5(MD5_DIGEST_LENGTH * 2, '\0');
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(&content_md5[i * 2], "%02X", md5_bytes[i]);
    }

    return content_md5;
}

static std::string calculate_md5_file(const std::string &file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        LOG_ERROR("Failed to open file: {}", file_path);
        return {};
    }

    MD5_CTX md5_context;
    MD5_Init(&md5_context);

    char buffer[1024];
    while (!file.eof()) {
        file.read(buffer, sizeof(buffer));
        MD5_Update(&md5_context, buffer, file.gcount());
    }

    unsigned char md5_digest[MD5_DIGEST_LENGTH];
    MD5_Final(md5_digest, &md5_context);

    return convert_md5_bytes_to_str(md5_digest);
}

static std::string get_content_md5(const std::string &header) {
    std::smatch match;
    std::string content_md5_base64;
    // Get the redirection URL from the response header (Location)
    if (std::regex_search(header, match, std::regex("Content-MD5: ([-A-Za-z0-9+/=]+)"))) {
        content_md5_base64 = match[1];
    } else {
        LOG_ERROR("No success found Content-MD5:\n{}", header);
        return {};
    }

    const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    unsigned char md5_bytes[MD5_DIGEST_LENGTH];
    int i = 0;
    uint32_t accum = 0;
    int accum_bits = 0;

    for (const auto c : content_md5_base64) {
        if (std::isspace(c) || (c == '=')) {
            continue;
        }

        size_t char_value = base64_chars.find(c);
        if (char_value == std::string::npos) {
            LOG_ERROR("Invalid character in base64 string: {}", c);
            return {};
        }

        accum = static_cast<uint32_t>(accum << 6) | char_value;
        accum_bits += 6;

        if (accum_bits >= 8) {
            accum_bits -= 8;
            md5_bytes[i++] = static_cast<unsigned char>((accum >> accum_bits) & 0xFF);
        }
    }

    return convert_md5_bytes_to_str(md5_bytes);
}

bool download_file(std::string url, const std::string &output_file_path, ProgressCallback progress_callback) {
    // Get the HEAD of response
    auto response = get_web_response(url, "HEAD");

    // Check if the response is not empty
    if (response.empty()) {
        LOG_ERROR("Failed to get header on url: {}", url);
        return false;
    }

    // Check if the response is a redirection
    if (response.find("HTTP/1.1 302 Found") != std::string::npos) {
        std::smatch match;
        // Get the redirection URL from the response header (Location)
        if (std::regex_search(response, match, std::regex("Location: (https?://[^\\s]+)"))) {
            url = match[1];
        } else {
            LOG_ERROR("No success found redirection location:\n{}", response);
            return false;
        }

        // Get the HEAD of response of the redirected url and check if it is not empty
        response = get_web_response(url, "HEAD");
        if (response.empty()) {
            LOG_ERROR("Failed to get header on redirected url: {}", url);
            return false;
        }
    }

    // Check if the response is resource not found
    if (response.find("HTTP/1.1 404 Not Found") != std::string::npos) {
        LOG_ERROR("404 Not Found");
        return false;
    }

    // Get the MD5 from the response header (Content-MD5)
    const auto content_md5 = get_content_md5(response);
    if (content_md5.empty()) {
        LOG_ERROR("Failed to get Content-MD5 on header: {}", response);
        return false;
    }

    // Get the file size from the response header (Content-Length)
    const auto file_size = get_file_size(response);
    if (file_size == 0) {
        LOG_ERROR("Failed to get file size");
        return false;
    }

    // Get the downloaded file size
    uint64_t downloaded_file_size = 0;
    std::ifstream file(output_file_path, std::ios::binary);
    if (file.is_open()) {
        file.seekg(0, std::ios::end);
        downloaded_file_size = file.tellg();
        file.close();
    }

    // Init SSL and socket connection with the downloaded file size (if exists)
    auto https = init(url, "GET", downloaded_file_size);
    if (!https.ssl)
        return false;

    // Calculate the header size based on the HEAD of response
    auto header_size = response.length();
    if (downloaded_file_size > 0) {
        // Calculate the size of the added line of Content-Range with the end of line characters
        const auto content_range_length = fmt::format("\r\nContent-Range: bytes {}-{}/{}\r\n", downloaded_file_size, file_size, file_size).length();
        // Add the size of diference between the HEAD of response and the GET of response
        header_size += 13 + content_range_length - (std::to_string(file_size).length() - std::to_string(downloaded_file_size).length());
    }

    // Create read buffer with using header size
    std::vector<char> read_buffer(header_size);

    // Read the header of the response and check if the response is resource not found
    uint32_t bytes_read = SSL_read(https.ssl, read_buffer.data(), static_cast<uint32_t>(read_buffer.size()));
    if (std::string(read_buffer.data()).find("HTTP/1.1 404 Not Found") != std::string::npos) {
        LOG_ERROR("404 Not Found");
        close_ssl(https.ssl);
        close_socket(https.sockfd);

        return false;
    }

    // Check if bytes read is diferent of header size
    if (bytes_read < (header_size * 0.99f)) {
        LOG_ERROR("Error reading header: {}/{}\n{}", bytes_read, header_size, read_buffer.data());
        close_ssl(https.ssl);
        close_socket(https.sockfd);
        return false;
    }

    // Create lambda to get current time in milliseconds
    const auto get_current_time_ms = []() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    };

    // Open the output file
    std::ofstream outfile(output_file_path, std::ios::app | std::ios::binary);

    // Set the initial downloaded file size to calculate the remaining time of download
    auto initial_downloaded_file_size = downloaded_file_size;

    // Set the initial time to calculate the remaining time of download
    auto start_time = get_current_time_ms();

    float progress_percent = 0.f;
    uint64_t remaining_time = 0;
    ProgressState progress_state{};

    while (progress_state.download && bytes_read) {
        if (!progress_state.pause) {
            if ((bytes_read = SSL_read(https.ssl, read_buffer.data(), static_cast<uint32_t>(read_buffer.size())))) {
                // Write the read buffer to the output file
                outfile.write(read_buffer.data(), bytes_read);

                // Update the downloaded file size
                downloaded_file_size += bytes_read;

                if (progress_callback) {
                    // Update progress percent
                    progress_percent = static_cast<float>(downloaded_file_size) / static_cast<float>(file_size) * 100.0f;

                    // Calculate elapsed time since start of download in seconds
                    const auto elapsed_time_ms = std::difftime(get_current_time_ms(), start_time);

                    // Calculate remaining time in seconds
                    const auto downloaded_bytes = static_cast<double>(downloaded_file_size - initial_downloaded_file_size);
                    const auto remaining_bytes = static_cast<double>(file_size - downloaded_file_size);
                    remaining_time = static_cast<uint64_t>((remaining_bytes / downloaded_bytes) * elapsed_time_ms) / 1000;
                }
            }
        } else {
            // Sleep for 100ms to not consume CPU
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            // Reset initial downloaded file size and start time to calculate remaining time correctly when resume download
            initial_downloaded_file_size = downloaded_file_size;
            start_time = get_current_time_ms();
        }

        // Call progress callback function to update progress info and state
        if (progress_callback)
            progress_state = progress_callback(progress_percent, remaining_time);
    }

    // Close the output file
    outfile.close();

    // Close SSL and socket connection
    close_ssl(https.ssl);
    close_socket(https.sockfd);

    // Reset progress state to 0
    if (progress_callback)
        progress_state = progress_callback(0, 0);

    // Check if download file size is same of file size
    if (downloaded_file_size < (file_size * 0.99)) {
        if (progress_state.download)
            LOG_ERROR("Downloaded size is not equal to file size, downloaded size: {}/{}", downloaded_file_size, file_size);
        else
            LOG_WARN("Canceled by user, dowloaded size: {}/{}", downloaded_file_size, file_size);
        return false;
    }

    // Check if the downloaded file is corrupted
    const auto downloaded_file_md5 = calculate_md5_file(output_file_path);
    if (downloaded_file_md5 != content_md5) {
        LOG_ERROR("Downloaded file is corrupted, MD5 Expected: {}; Downloaded: {}", content_md5, downloaded_file_md5);
        fs::remove(output_file_path);
        return false;
    }

    return true;
}

} // namespace https
