#include "Server.h"
#include "FileManager.h"
#include "HtmlRenderer.h"
#include "httplib.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <shellapi.h>
#include <bcrypt.h>

#include <algorithm>
#include <cwctype>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// Token generation
// ---------------------------------------------------------------------------

static std::string generate_token() {
    unsigned char bytes[16];
    NTSTATUS status = BCryptGenRandom(NULL, bytes, sizeof(bytes),
                                     BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (status != 0) {
        // Fallback: mix time + process id (less secure but functional)
        srand(static_cast<unsigned>(time(NULL)) ^ GetCurrentProcessId());
        for (int i = 0; i < 16; ++i) {
            bytes[i] = static_cast<unsigned char>(rand() % 256);
        }
    }

    char hex[33];
    for (int i = 0; i < 16; ++i) {
        snprintf(hex + i * 2, 3, "%02x", bytes[i]);
    }
    return std::string(hex, 32);
}

// ---------------------------------------------------------------------------
// Cookie parsing – strict key match, not substring
// ---------------------------------------------------------------------------

static std::string trim_whitespace(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t')) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t')) {
        --end;
    }
    return s.substr(start, end - start);
}

static bool has_valid_token_cookie(const httplib::Request& req, const std::string& token) {
    std::string cookie = req.get_header_value("Cookie");
    if (cookie.empty()) {
        return false;
    }

    size_t pos = 0;
    while (pos < cookie.size()) {
        size_t semicolon = cookie.find(';', pos);
        std::string part;
        if (semicolon == std::string::npos) {
            part = cookie.substr(pos);
            pos = cookie.size();
        } else {
            part = cookie.substr(pos, semicolon - pos);
            pos = semicolon + 1;
        }

        std::string trimmed = trim_whitespace(part);
        size_t eq = trimmed.find('=');
        if (eq == std::string::npos) {
            continue;
        }

        std::string key = trim_whitespace(trimmed.substr(0, eq));
        std::string value = trimmed.substr(eq + 1);

        if (key == "lfs_token" && value == token) {
            return true;
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// 403 page
// ---------------------------------------------------------------------------

static void send_auth_denied(httplib::Response& res) {
    res.status = 403;
    std::string html =
        "<!doctype html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "<title>Access Denied</title>"
        "<style>*{box-sizing:border-box}"
        "body{margin:0;display:grid;place-items:center;min-height:100vh;"
        "font-family:\"SF Pro Display\",\"Segoe UI\",Helvetica,Arial,sans-serif;"
        "background:radial-gradient(circle at 78% 10%,rgba(85,159,255,.18),transparent 28%),"
        "linear-gradient(135deg,#fbfcff,#eef3f8);color:#16181d;padding:22px;text-align:center}"
        "main{width:min(480px,100%);background:rgba(255,255,255,.78);"
        "border:1px solid rgba(25,31,44,.11);border-radius:32px;padding:36px;"
        "box-shadow:0 24px 70px rgba(21,30,48,.11);backdrop-filter:blur(24px)}"
        ".code{font-size:72px;font-weight:800;color:#e5e7eb;line-height:1;margin:0 0 8px}"
        "h1{font-size:22px;margin:0 0 12px}"
        "p{color:#6b7280;line-height:1.65;margin:0}"
        "</style></head><body><main>"
        "<div class=\"code\">403</div>"
        "<h1>&#35775;&#38382;&#34987;&#25298;&#32477;</h1>"
        "<p>&#27492;&#20849;&#20139;&#31354;&#38388;&#38656;&#35201;&#26377;&#25928;&#30340;&#35775;&#38382;&#38142;&#25509;&#12290;"
        "&#35831;&#25195;&#25551;&#20027;&#26426;&#23631;&#24149;&#19978;&#30340;&#20108;&#32500;&#30721;&#65292;"
        "&#25110;&#21521;&#20998;&#20139;&#32773;&#32034;&#21462;&#27491;&#30830;&#30340; URL&#12290;</p>"
        "</main></body></html>";
    res.set_content(html, "text/html; charset=utf-8");
}

// ---------------------------------------------------------------------------
// Network helpers
// ---------------------------------------------------------------------------

static bool is_private_ipv4(const std::string& ip) {
    unsigned int a = 0;
    unsigned int b = 0;
    unsigned int c = 0;
    unsigned int d = 0;
    if (std::sscanf(ip.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
        return false;
    }

    return a == 10 ||
           (a == 172 && b >= 16 && b <= 31) ||
           (a == 192 && b == 168);
}

static bool is_virtual_adapter(IP_ADAPTER_ADDRESSES* adapter) {
    if (adapter->FriendlyName != NULL) {
        std::wstring lower_name = adapter->FriendlyName;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), [](wchar_t ch) {
            return static_cast<wchar_t>(std::towlower(ch));
        });
        if (lower_name.find(L"hyper-v")    != std::wstring::npos ||
            lower_name.find(L"wsl")        != std::wstring::npos ||
            lower_name.find(L"loopback")   != std::wstring::npos ||
            lower_name.find(L"virtual")    != std::wstring::npos ||
            lower_name.find(L"vmware")     != std::wstring::npos ||
            lower_name.find(L"virtualbox") != std::wstring::npos ||
            lower_name.find(L"vethernet")  != std::wstring::npos) {
            return true;
        }
    }
    if (adapter->IfType == IF_TYPE_TUNNEL ||
        adapter->IfType == IF_TYPE_PPP    ||
        adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
        return true;
    }
    return false;
}

static std::string detect_lan_ipv4() {
    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG buffer_size = 16 * 1024;
    std::vector<unsigned char> buffer(buffer_size);

    IP_ADAPTER_ADDRESSES* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(&buffer[0]);
    ULONG result = GetAdaptersAddresses(AF_INET, flags, NULL, adapters, &buffer_size);
    if (result == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(buffer_size);
        adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(&buffer[0]);
        result = GetAdaptersAddresses(AF_INET, flags, NULL, adapters, &buffer_size);
    }

    if (result != NO_ERROR) {
        return std::string();
    }

    std::string physical_fallback;
    for (IP_ADAPTER_ADDRESSES* adapter = adapters; adapter != NULL; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
            continue;
        }

        if (is_virtual_adapter(adapter)) {
            continue;
        }

        for (IP_ADAPTER_UNICAST_ADDRESS* address = adapter->FirstUnicastAddress;
             address != NULL;
             address = address->Next) {
            if (address->Address.lpSockaddr == NULL ||
                address->Address.lpSockaddr->sa_family != AF_INET) {
                continue;
            }

            sockaddr_in* ipv4 = reinterpret_cast<sockaddr_in*>(address->Address.lpSockaddr);
            char text[INET_ADDRSTRLEN] = {};
            if (inet_ntop(AF_INET, &ipv4->sin_addr, text, sizeof(text)) == NULL) {
                continue;
            }

            std::string ip = text;
            if (ip.compare(0, 4, "127.") == 0 || ip.compare(0, 8, "169.254.") == 0) {
                continue;
            }

            if (is_private_ipv4(ip)) {
                return ip;
            }

            if (physical_fallback.empty()) {
                physical_fallback = ip;
            }
        }
    }

    if (!physical_fallback.empty()) {
        return physical_fallback;
    }

    std::string fallback;
    for (IP_ADAPTER_ADDRESSES* adapter = adapters; adapter != NULL; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
            continue;
        }

        for (IP_ADAPTER_UNICAST_ADDRESS* address = adapter->FirstUnicastAddress;
             address != NULL;
             address = address->Next) {
            if (address->Address.lpSockaddr == NULL ||
                address->Address.lpSockaddr->sa_family != AF_INET) {
                continue;
            }

            sockaddr_in* ipv4 = reinterpret_cast<sockaddr_in*>(address->Address.lpSockaddr);
            char text[INET_ADDRSTRLEN] = {};
            if (inet_ntop(AF_INET, &ipv4->sin_addr, text, sizeof(text)) == NULL) {
                continue;
            }

            std::string ip = text;
            if (ip.compare(0, 4, "127.") == 0 || ip.compare(0, 8, "169.254.") == 0) {
                continue;
            }

            if (is_private_ipv4(ip)) {
                return ip;
            }

            if (fallback.empty()) {
                fallback = ip;
            }
        }
    }

    return fallback;
}

static std::string make_base_url(const std::string& host, int port) {
    if (host.empty()) {
        return std::string();
    }
    std::ostringstream url;
    url << "http://" << host << ':' << port;
    return url.str();
}

static std::string append_token_to_url(const std::string& base_url, const std::string& token) {
    if (base_url.empty() || token.empty()) {
        return base_url;
    }
    return base_url + "/?token=" + token;
}

static void open_browser_after_start(const std::string& url) {
    std::thread([url]() {
        Sleep(500);
        ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }).detach();
}

static void send_error(httplib::Response& res, int status, const std::string& message) {
    res.status = status;
    res.set_content(message, "text/plain; charset=utf-8");
}

// ---------------------------------------------------------------------------
// Server entry point
// ---------------------------------------------------------------------------

int Server::run(const Options& options) {
    httplib::Server server;
    std::string lan_ip = detect_lan_ipv4();

    // Generate auth token (empty string when auth is disabled)
    std::string token;
    if (!options.no_auth) {
        token = generate_token();
    }

    // Build URLs
    std::string lan_base_url = make_base_url(lan_ip, options.port);
    std::string local_base_url = make_base_url("127.0.0.1", options.port);

    // access_url passed to render functions: includes token so QR/copy-link work
    std::string access_url = append_token_to_url(lan_base_url, token);
    std::string local_url = append_token_to_url(local_base_url, token);

    // -----------------------------------------------------------------------
    // Pre-routing: security headers + CORS + authentication
    // -----------------------------------------------------------------------
    server.set_pre_routing_handler(
        [&](const httplib::Request& req, httplib::Response& res) {
            // 1. Security headers on ALL responses
            res.set_header("Referrer-Policy", "no-referrer");
            res.set_header("X-Content-Type-Options", "nosniff");
            res.set_header("Cache-Control", "no-store");

            // 2. CORS – only in dev mode, only for Vite dev server
            if (options.dev_mode) {
                std::string origin = req.get_header_value("Origin");
                if (origin == "http://127.0.0.1:5173" ||
                    origin == "http://localhost:5173") {
                    res.set_header("Access-Control-Allow-Origin", origin);
                    res.set_header("Vary", "Origin");
                }

                // Handle preflight
                if (req.method == "OPTIONS") {
                    res.set_header("Access-Control-Allow-Methods", "GET, OPTIONS");
                    res.set_header("Access-Control-Allow-Headers", "Content-Type");
                    res.status = 204;
                    return httplib::Server::HandlerResponse::Handled;
                }
            }

            // 3. Authentication (skip when --no-auth)
            if (!options.no_auth) {
                // Auth entry point: GET /?token=xxx (strictly path == "/" with token param)
                if (req.path == "/" && req.has_param("token")) {
                    std::string provided = req.get_param_value("token");
                    if (provided == token) {
                        res.set_header("Set-Cookie",
                            "lfs_token=" + token + "; Path=/; HttpOnly; SameSite=Strict");
                        res.set_header("Location", "/");
                        res.status = 302;
                        return httplib::Server::HandlerResponse::Handled;
                    } else {
                        send_auth_denied(res);
                        return httplib::Server::HandlerResponse::Handled;
                    }
                }

                // All other requests: require valid cookie
                if (!has_valid_token_cookie(req, token)) {
                    send_auth_denied(res);
                    return httplib::Server::HandlerResponse::Handled;
                }
            }

            return httplib::Server::HandlerResponse::Unhandled;
        });

    // -----------------------------------------------------------------------
    // Route handlers
    // -----------------------------------------------------------------------

    server.Get("/", [&](const httplib::Request&, httplib::Response& res) {
        res.set_content(render_directory_page(options.share_dir, options.share_dir, access_url),
                        "text/html; charset=utf-8");
    });

    server.Get("/api/list", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string request_path;
            if (req.has_param("path")) {
                request_path = req.get_param_value("path");
            }

            std::wstring target = resolve_request_path(options.share_dir, request_path);
            if (!is_directory(target)) {
                send_error(res, 404, "Directory not found");
                return;
            }

            int offset = 0;
            int limit = 1000;
            if (req.has_param("offset")) {
                try { offset = std::stoi(req.get_param_value("offset")); } catch (...) {}
            }
            if (req.has_param("limit")) {
                try { limit = std::stoi(req.get_param_value("limit")); } catch (...) {}
            }
            if (offset < 0) offset = 0;
            if (limit <= 0) limit = 1000;

            res.set_content(render_directory_json(options.share_dir, target, access_url, offset, limit),
                            "application/json; charset=utf-8");
        } catch (const std::exception& ex) {
            send_error(res, 403, ex.what());
        }
    });

    server.Get("/qr.svg", [&](const httplib::Request&, httplib::Response& res) {
        if (access_url.empty()) {
            send_error(res, 503, "LAN address is unavailable");
            return;
        }

        res.set_content(make_qr_svg(access_url), "image/svg+xml; charset=utf-8");
    });

    server.Get("/qr", [&](const httplib::Request&, httplib::Response& res) {
        if (access_url.empty()) {
            send_error(res, 503, "LAN address is unavailable");
            return;
        }

        std::ostringstream html;
        html
            << "<!doctype html><html lang=\"zh-CN\"><head>"
            << "<meta charset=\"utf-8\">"
            << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
            << "<title>LocalFileShare QR</title>"
            << "<style>*{box-sizing:border-box}body{margin:0;font-family:\"SF Pro Display\",\"Segoe UI\",Helvetica,Arial,sans-serif;background:radial-gradient(circle at 75% 12%,rgba(85,159,255,.25),transparent 30%),linear-gradient(135deg,#fbfcff,#eef3f8);color:#16181d;display:grid;place-items:center;min-height:100vh;padding:22px;text-align:center}"
            << "main{width:min(520px,100%);background:rgba(255,255,255,.78);border:1px solid rgba(25,31,44,.11);border-radius:32px;padding:26px;box-shadow:0 24px 70px rgba(21,30,48,.13);backdrop-filter:blur(24px)}h1{font-size:30px;line-height:1.08;margin:0 0 8px}.hint{color:#6b7280;margin:0 0 20px}.qr{display:grid;place-items:center;background:linear-gradient(180deg,#fff,#f7f9fc);border:1px solid rgba(25,31,44,.11);border-radius:26px;padding:20px}img{width:328px;height:328px;max-width:78vw;max-height:78vw;image-rendering:pixelated;border-radius:12px}.url{font-family:Consolas,\"SFMono-Regular\",monospace;margin-top:18px;word-break:break-all;color:#0f766e;background:rgba(255,255,255,.68);border:1px solid rgba(25,31,44,.11);border-radius:18px;padding:13px}</style>"
            << "</head><body><main><h1>&#25195;&#30721;&#35775;&#38382;</h1><p class=\"hint\">&#22312;&#21516;&#19968;&#23616;&#22495;&#32593;&#20869;&#25171;&#24320;&#20849;&#20139;&#25991;&#20214;</p><div class=\"qr\"><img src=\"/qr.svg?v=2\" alt=\"QR code\"></div><div class=\"url\">"
            << html_escape(access_url)
            << "</div></main></body></html>";
        res.set_content(html.str(), "text/html; charset=utf-8");
    });

    server.Get(R"(/browse/(.*))", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            std::wstring target = resolve_request_path(options.share_dir, req.matches[1]);
            if (!is_directory(target)) {
                send_error(res, 404, "Directory not found");
                return;
            }

            res.set_content(render_directory_page(options.share_dir, target, access_url),
                            "text/html; charset=utf-8");
        } catch (const std::exception& ex) {
            send_error(res, 403, ex.what());
        }
    });

    server.Get(R"(/download/(.*))", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            std::wstring target = resolve_request_path(options.share_dir, req.matches[1]);
            if (!is_regular_file(target)) {
                send_error(res, 404, "File not found");
                return;
            }

            unsigned long long file_size = get_file_size(target);
            if (sizeof(size_t) < 8 && file_size > 0xFFFFFFFFULL) {
                send_error(res, 500, "File is too large to download on a 32-bit server");
                return;
            }

            std::string filename = wide_to_utf8(target.substr(target.find_last_of(L"\\/") + 1));
            res.set_header("Content-Disposition",
                "attachment; filename=\"download\"; filename*=UTF-8''" + url_encode(filename));
            res.set_content_provider(
                static_cast<size_t>(file_size),
                guess_mime_type(target),
                [target](size_t offset, size_t length, httplib::DataSink& sink) {
                    FILE* file = NULL;
                    if (_wfopen_s(&file, target.c_str(), L"rb") != 0 || file == NULL) {
                        return false;
                    }

                    if (_fseeki64(file, static_cast<__int64>(offset), SEEK_SET) != 0) {
                        fclose(file);
                        return false;
                    }

                    char buffer[8192];
                    bool ok = true;
                    while (length > 0) {
                        size_t to_read = std::min(sizeof(buffer), length);
                        size_t read_count = fread(buffer, 1, to_read, file);
                        if (read_count == 0) {
                            break;
                        }
                        if (!sink.write(buffer, read_count)) {
                            ok = false;
                            break;
                        }
                        length -= read_count;
                    }

                    fclose(file);
                    return ok;
                });
        } catch (const std::exception& ex) {
            send_error(res, 403, ex.what());
        }
    });

    server.Post("/api/upload", [&](const httplib::Request& req, httplib::Response& res, const httplib::ContentReader& content_reader) {
        if (!req.is_multipart_form_data()) {
            send_error(res, 400, "Expected multipart/form-data");
            return;
        }

        try {
            std::string request_path;
            if (req.has_param("path")) {
                request_path = req.get_param_value("path");
            }

            std::wstring target_dir = resolve_request_path(options.share_dir, request_path);
            if (!is_directory(target_dir)) {
                send_error(res, 404, "Directory not found");
                return;
            }

            bool upload_ok = true;
            std::string error_msg;
            FILE* current_file = NULL;

            content_reader(
                [&](const httplib::FormData& file) {
                    if (current_file) {
                        fclose(current_file);
                        current_file = NULL;
                    }

                    if (file.filename.empty()) {
                        return true; // Skip fields that are not files
                    }

                    std::wstring filename = utf8_to_wide(file.filename);
                    size_t slash = filename.find_last_of(L"\\/");
                    if (slash != std::wstring::npos) {
                        filename = filename.substr(slash + 1);
                    }
                    
                    if (filename.empty() || filename == L"." || filename == L"..") {
                        return true;
                    }

                    std::wstring current_file_path = join_path(target_dir, filename);
                    if (_wfopen_s(&current_file, current_file_path.c_str(), L"wb") != 0 || current_file == NULL) {
                        upload_ok = false;
                        error_msg = "Failed to open file for writing: " + file.filename;
                        return false;
                    }
                    return true;
                },
                [&](const char* data, size_t data_length) {
                    if (current_file) {
                        if (fwrite(data, 1, data_length, current_file) != data_length) {
                            upload_ok = false;
                            error_msg = "Write error during upload";
                            return false;
                        }
                    }
                    return true;
                }
            );

            if (current_file) {
                fclose(current_file);
            }

            if (!upload_ok) {
                send_error(res, 500, error_msg.empty() ? "Upload failed" : error_msg);
            } else {
                res.set_content("{\"status\":\"ok\"}", "application/json; charset=utf-8");
            }
        } catch (const std::exception& ex) {
            send_error(res, 403, ex.what());
        }
    });

    // -----------------------------------------------------------------------
    // Startup output
    // -----------------------------------------------------------------------

    std::cout << "LocalFileShare started.\n"
              << "Shared directory: " << wide_to_utf8(options.share_dir) << "\n";

    if (!options.no_auth) {
        std::cout << "Access token: " << token << "\n";
    } else {
        std::cout << "Authentication: DISABLED\n";
    }

    std::cout << "Local URL: " << local_url << "\n"
              << "LAN URL:   "
              << (lan_base_url.empty()
                  ? "http://<your-lan-ip>:" + std::to_string(options.port)
                  : access_url)
              << "\n"
              << "QR page:   " << local_url << "\n"
              << "Press Ctrl+C to stop.\n";

    if (options.open_browser) {
        open_browser_after_start(local_url);
    }

    if (!server.listen(options.host.c_str(), options.port)) {
        std::cerr << "Failed to listen on " << options.host << ':' << options.port << "\n";
        return 1;
    }

    return 0;
}
