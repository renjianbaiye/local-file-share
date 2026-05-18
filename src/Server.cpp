#include "Server.h"
#include "FileManager.h"
#include "HtmlRenderer.h"
#include "httplib.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <shellapi.h>

#include <iostream>
#include <thread>
#include <vector>
#include <stdio.h>
#include <algorithm>
#include <cwctype>

bool is_private_ipv4(const std::string& ip) {
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

bool is_virtual_adapter(IP_ADAPTER_ADDRESSES* adapter) {
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

std::string detect_lan_ipv4() {
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

std::string make_access_url(const std::string& lan_ip, int port) {
    if (lan_ip.empty()) {
        return std::string();
    }

    std::ostringstream url;
    url << "http://" << lan_ip << ':' << port;
    return url.str();
}

void open_browser_after_start(const std::string& url) {
    std::thread([url]() {
        Sleep(500);
        ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }).detach();
}

void send_error(httplib::Response& res, int status, const std::string& message) {
    res.status = status;
    res.set_content(message, "text/plain; charset=utf-8");
}

int Server::run(const Options& options) {
    httplib::Server server;
    std::string lan_ip = detect_lan_ipv4();
    std::string access_url = make_access_url(lan_ip, options.port);

    server.set_pre_routing_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        return httplib::Server::HandlerResponse::Unhandled;
    });

    server.Get("/", [&](const httplib::Request&, httplib::Response& res) {
        res.set_content(render_directory_page(options.share_dir, options.share_dir, access_url), "text/html; charset=utf-8");
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

            res.set_header("Cache-Control", "no-store, max-age=0");
            res.set_content(render_directory_json(options.share_dir, target, access_url), "application/json; charset=utf-8");
        } catch (const std::exception& ex) {
            send_error(res, 403, ex.what());
        }
    });

    server.Get("/qr.svg", [&](const httplib::Request&, httplib::Response& res) {
        if (access_url.empty()) {
            send_error(res, 503, "LAN address is unavailable");
            return;
        }

        res.set_header("Cache-Control", "no-store, max-age=0");
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

            res.set_content(render_directory_page(options.share_dir, target, access_url), "text/html; charset=utf-8");
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
            std::string filename = wide_to_utf8(target.substr(target.find_last_of(L"\\/") + 1));
            res.set_header("Content-Disposition", "attachment; filename=\"download\"; filename*=UTF-8''" + url_encode(filename));
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

    std::cout << "LocalFileShare started.\n"
              << "Shared directory: " << wide_to_utf8(options.share_dir) << "\n"
              << "Local URL: http://127.0.0.1:" << options.port << "\n"
              << "LAN URL:   " << (access_url.empty() ? "http://<your-lan-ip>:" + std::to_string(options.port) : access_url) << "\n"
              << "QR page:   http://127.0.0.1:" << options.port << "\n"
              << "Press Ctrl+C to stop.\n";

    if (options.open_browser) {
        open_browser_after_start("http://127.0.0.1:" + std::to_string(options.port));
    }

    if (!server.listen(options.host.c_str(), options.port)) {
        std::cerr << "Failed to listen on " << options.host << ':' << options.port << "\n";
        return 1;
    }

    return 0;
}
