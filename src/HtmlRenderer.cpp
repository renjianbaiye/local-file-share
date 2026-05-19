#include "HtmlRenderer.h"
#include "FileManager.h"
#include "qrcodegen.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

std::string html_escape(const std::string& input) {
    std::string output;
    output.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i) {
        switch (input[i]) {
        case '&': output += "&amp;"; break;
        case '<': output += "&lt;"; break;
        case '>': output += "&gt;"; break;
        case '"': output += "&quot;"; break;
        case '\'': output += "&#39;"; break;
        default: output += input[i]; break;
        }
    }

    return output;
}

std::string json_escape(const std::string& input) {
    std::string output;
    output.reserve(input.size() + 8);

    for (size_t i = 0; i < input.size(); ++i) {
        unsigned char ch = static_cast<unsigned char>(input[i]);
        switch (ch) {
        case '\\': output += "\\\\"; break;
        case '"': output += "\\\""; break;
        case '\b': output += "\\b"; break;
        case '\f': output += "\\f"; break;
        case '\n': output += "\\n"; break;
        case '\r': output += "\\r"; break;
        case '\t': output += "\\t"; break;
        default:
            if (ch < 0x20) {
                std::ostringstream escaped;
                escaped << "\\u" << std::uppercase << std::hex << std::setw(4)
                        << std::setfill('0') << static_cast<int>(ch);
                output += escaped.str();
            } else {
                output += input[i];
            }
            break;
        }
    }

    return output;
}

std::string url_encode(const std::string& input) {
    std::ostringstream encoded;
    encoded << std::uppercase << std::hex;

    for (size_t i = 0; i < input.size(); ++i) {
        unsigned char ch = static_cast<unsigned char>(input[i]);
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~' || ch == '/') {
            encoded << static_cast<char>(ch);
        } else {
            encoded << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
        }
    }

    return encoded.str();
}

std::string url_decode(const std::string& input) {
    std::string output;
    output.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%' && i + 2 < input.size()) {
            std::string hex = input.substr(i + 1, 2);
            char* end = NULL;
            long value = std::strtol(hex.c_str(), &end, 16);
            if (end == hex.c_str() + 2) {
                output += static_cast<char>(value);
                i += 2;
                continue;
            }
        }

        output += input[i] == '+' ? ' ' : input[i];
    }

    return output;
}

std::string file_size_text(unsigned long long bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double size = static_cast<double>(bytes);
    int unit = 0;

    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        ++unit;
    }

    std::ostringstream text;
    if (unit == 0) {
        text << static_cast<unsigned long long>(size) << ' ' << units[unit];
    } else {
        text << std::fixed << std::setprecision(1) << size << ' ' << units[unit];
    }
    return text.str();
}

std::string guess_mime_type(const std::wstring& path) {
    std::wstring ext = extension_of(path);

    if (ext == L".html" || ext == L".htm") return "text/html; charset=utf-8";
    if (ext == L".txt" || ext == L".log") return "text/plain; charset=utf-8";
    if (ext == L".css") return "text/css; charset=utf-8";
    if (ext == L".js") return "application/javascript; charset=utf-8";
    if (ext == L".json") return "application/json; charset=utf-8";
    if (ext == L".png") return "image/png";
    if (ext == L".jpg" || ext == L".jpeg") return "image/jpeg";
    if (ext == L".gif") return "image/gif";
    if (ext == L".webp") return "image/webp";
    if (ext == L".pdf") return "application/pdf";
    if (ext == L".mp4") return "video/mp4";
    if (ext == L".mp3") return "audio/mpeg";
    return "application/octet-stream";
}

std::string render_directory_page(const std::wstring& root, const std::wstring& current, const std::string& access_url) {
    std::vector<FileEntry> entries = list_directory(current);

    std::sort(entries.begin(), entries.end(), [](const FileEntry& a, const FileEntry& b) {
        if (a.is_directory != b.is_directory) {
            return a.is_directory > b.is_directory;
        }
        return lowercase_path(a.name) < lowercase_path(b.name);
    });

    size_t folder_count = 0;
    size_t file_count = 0;
    unsigned long long total_size = 0;
    for (size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].is_directory) {
            ++folder_count;
        } else {
            ++file_count;
            total_size += entries[i].size;
        }
    }

    std::string current_relative = relative_url_path(root, current);
    bool is_root_view = lowercase_path(full_path(current)) == lowercase_path(full_path(root));
    std::ostringstream html;

    html
        << "<!doctype html><html lang=\"zh-CN\"><head>"
        << "<meta charset=\"utf-8\">"
        << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        << "<title>LocalFileShare</title>"
        << "<style>"
        << ":root{--ink:#16181d;--muted:#6b7280;--line:rgba(25,31,44,.11);--panel:rgba(255,255,255,.76);--blue:#1677ff;--green:#15b88f;--gold:#bd8755;--shadow:0 24px 70px rgba(21,30,48,.11)}"
        << "*{box-sizing:border-box}body{margin:0;font-family:\"SF Pro Display\",\"Segoe UI\",Helvetica,Arial,sans-serif;background:radial-gradient(circle at 78% 10%,rgba(85,159,255,.22),transparent 28%),linear-gradient(135deg,#fbfcff 0%,#f3f6fb 48%,#eef3f8 100%);color:var(--ink);min-height:100vh}"
        << "body:before{content:\"\";position:fixed;inset:0;background-image:linear-gradient(rgba(22,24,29,.035) 1px,transparent 1px),linear-gradient(90deg,rgba(22,24,29,.035) 1px,transparent 1px);background-size:44px 44px;mask-image:linear-gradient(to bottom,rgba(0,0,0,.55),transparent 70%);pointer-events:none}"
        << "main{width:min(1180px,calc(100% - 32px));margin:0 auto;padding:28px 0 44px;position:relative}.topbar{display:flex;align-items:center;justify-content:space-between;gap:16px;margin-bottom:28px}.brand{display:flex;align-items:center;gap:11px;font-weight:700}.logo{width:36px;height:36px;border-radius:12px;background:linear-gradient(145deg,#111827,#2f3848);box-shadow:inset 0 1px 0 rgba(255,255,255,.18),0 14px 34px rgba(17,24,39,.18);display:grid;place-items:center;color:#fff}.logo svg{width:19px;height:19px}.status{display:flex;align-items:center;gap:8px;color:var(--muted);font-size:13px;background:rgba(255,255,255,.72);border:1px solid var(--line);border-radius:999px;padding:8px 12px;backdrop-filter:blur(18px)}.dot{width:8px;height:8px;border-radius:50%;background:var(--green);box-shadow:0 0 0 5px rgba(21,184,143,.12)}"
        << ".hero{display:grid;grid-template-columns:minmax(0,1.2fr) minmax(310px,.8fr);gap:22px;align-items:stretch;margin-bottom:22px}.hero-main,.connect,.files{background:var(--panel);border:1px solid var(--line);box-shadow:var(--shadow);backdrop-filter:blur(24px)}.hero-main{border-radius:30px;padding:34px;position:relative;overflow:hidden;min-height:310px}.hero-main:after{content:\"\";position:absolute;right:-90px;bottom:-120px;width:360px;height:360px;border-radius:50%;background:radial-gradient(circle,rgba(22,119,255,.22),transparent 62%)}.eyebrow{font-size:13px;color:var(--blue);font-weight:700;margin-bottom:18px}h1{font-size:clamp(36px,6vw,72px);line-height:.96;margin:0;max-width:760px;font-weight:800}.subtitle{max-width:650px;color:#4b5563;font-size:17px;line-height:1.72;margin:22px 0 28px}.path-chip{display:inline-flex;max-width:100%;align-items:center;gap:10px;border:1px solid var(--line);background:rgba(255,255,255,.62);border-radius:999px;padding:10px 14px;color:#4b5563;box-shadow:0 12px 28px rgba(31,41,55,.08)}.path-chip svg{width:16px;height:16px;flex:0 0 auto}.path-text{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
        << ".stats{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:10px;margin-top:30px;max-width:620px}.stat{border:1px solid var(--line);border-radius:18px;padding:14px 16px;background:rgba(255,255,255,.54)}.stat strong{display:block;font-size:24px}.stat span{display:block;color:var(--muted);font-size:12px;margin-top:3px}"
        << ".connect{border-radius:30px;padding:20px;display:flex;flex-direction:column;justify-content:space-between;gap:18px}.connect-head{display:flex;justify-content:space-between;align-items:flex-start;gap:12px}.connect-title{font-size:21px;font-weight:800;margin:0 0 8px}.connect-copy{color:var(--muted);font-size:14px;line-height:1.55;margin:0}.tag{border:1px solid rgba(21,184,143,.22);background:rgba(21,184,143,.1);color:#08785f;border-radius:999px;padding:7px 10px;font-size:12px;white-space:nowrap}.qr-wrap{display:grid;place-items:center;background:linear-gradient(180deg,#fff,#f7f9fc);border:1px solid var(--line);border-radius:24px;padding:18px}.qr-wrap img{width:232px;height:232px;display:block;max-width:100%;border-radius:12px;background:#fff}.connect-url{font-family:Consolas,\"SFMono-Regular\",monospace;color:#0f766e;word-break:break-all;background:rgba(255,255,255,.62);border:1px solid var(--line);border-radius:16px;padding:12px 13px;font-size:13px}.actions{display:flex;gap:10px;flex-wrap:wrap}.button{display:inline-flex;align-items:center;justify-content:center;gap:8px;min-height:42px;border-radius:999px;padding:0 16px;text-decoration:none;font-weight:700;font-size:14px;border:1px solid var(--line);color:var(--ink);background:rgba(255,255,255,.7);transition:transform .18s ease,box-shadow .18s ease,background .18s ease}.button.primary{background:#111827;color:#fff;border-color:#111827;box-shadow:0 14px 32px rgba(17,24,39,.2)}.button:hover{transform:translateY(-2px);box-shadow:0 16px 34px rgba(21,30,48,.13)}.button svg{width:16px;height:16px}"
        << ".files{border-radius:26px;overflow:hidden}.files-head{display:flex;align-items:center;justify-content:space-between;gap:14px;padding:18px 20px;border-bottom:1px solid var(--line)}.files-title{font-size:18px;font-weight:800}.files-sub{color:var(--muted);font-size:13px;margin-top:4px}.pill{border-radius:999px;border:1px solid var(--line);background:rgba(255,255,255,.62);padding:8px 11px;font-size:12px;color:var(--muted)}.list{background:rgba(255,255,255,.42)}.row{display:grid;grid-template-columns:auto minmax(0,1fr) auto;gap:13px;align-items:center;padding:15px 20px;border-top:1px solid rgba(25,31,44,.075);text-decoration:none;color:inherit;transition:background .16s ease,transform .16s ease}.row:first-child{border-top:0}.row:hover{background:rgba(255,255,255,.75);transform:translateX(3px)}.icon{width:38px;height:38px;border-radius:14px;display:grid;place-items:center;background:#eef4ff;color:#1d5fd1}.icon.folder{background:#fff5e8;color:#9a5b16}.icon.parent{background:#eef2f7;color:#475569}.icon svg{width:19px;height:19px}.name{min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-weight:650}.meta{color:var(--muted);font-size:13px;white-space:nowrap}.empty{padding:44px 20px;text-align:center;color:var(--muted)}.empty strong{display:block;color:var(--ink);font-size:18px;margin-bottom:7px}"
        << "@media(max-width:860px){main{width:min(100% - 22px,680px);padding-top:18px}.hero{grid-template-columns:1fr}.hero-main{min-height:auto;padding:26px}.stats{grid-template-columns:1fr 1fr}.connect{border-radius:24px}.topbar{margin-bottom:18px}.status{display:none}}@media(max-width:560px){h1{font-size:40px}.subtitle{font-size:15px}.stats{grid-template-columns:1fr}.files-head{align-items:flex-start;flex-direction:column}.row{grid-template-columns:auto minmax(0,1fr);padding:14px}.meta{grid-column:2}.qr-wrap img{width:210px;height:210px}}"
        << "</style></head><body><main>"
        << "<header class=\"topbar\"><div class=\"brand\"><div class=\"logo\"><svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\"><path d=\"M12 3v12\"/><path d=\"m7 8 5-5 5 5\"/><path d=\"M4 14v4a3 3 0 0 0 3 3h10a3 3 0 0 0 3-3v-4\"/></svg></div><span>LocalFileShare</span></div><div class=\"status\"><span class=\"dot\"></span><span>&#23616;&#22495;&#32593;&#20849;&#20139;&#20013;</span></div></header>"
        << "<section class=\"hero\"><div class=\"hero-main\"><div class=\"eyebrow\">AIR SHARE / &#36731;&#37327;&#20256;&#36755;</div><h1>&#20687; AirDrop &#19968;&#26679;&#25226;&#25991;&#20214;&#36865;&#21040;&#36523;&#36793;&#30340;&#35774;&#22791;</h1><p class=\"subtitle\">&#25171;&#24320;&#21516;&#19968;&#20010; Wi-Fi&#65292;&#25195;&#30721;&#25110;&#36755;&#20837;&#38142;&#25509;&#21363;&#21487;&#27983;&#35272;&#12289;&#19979;&#36733;&#20849;&#20139;&#25991;&#20214;&#12290;&#20445;&#30041;&#22269;&#20869;&#24037;&#20855;&#31449;&#30340;&#28165;&#26224;&#20449;&#24687;&#23618;&#32423;&#65292;&#21152;&#19978; Apple &#24335;&#30340;&#24178;&#20928;&#21644;&#36136;&#24863;&#12290;</p>"
        << "<div class=\"path-chip\"><svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\"><path d=\"M3 7h6l2 2h10v9a3 3 0 0 1-3 3H6a3 3 0 0 1-3-3z\"/></svg><span class=\"path-text\">/" << html_escape(current_relative) << "</span></div>"
        << "<div class=\"stats\"><div class=\"stat\"><strong>" << folder_count << "</strong><span>&#25991;&#20214;&#22841;</span></div><div class=\"stat\"><strong>" << file_count << "</strong><span>&#25991;&#20214;</span></div><div class=\"stat\"><strong>" << file_size_text(total_size) << "</strong><span>&#21487;&#19979;&#36733;&#24635;&#37327;</span></div></div></div>";

    if (!access_url.empty() && is_root_view) {
        html
            << "<aside class=\"connect\"><div class=\"connect-head\"><div><h2 class=\"connect-title\">&#25195;&#30721;&#35775;&#38382;</h2><p class=\"connect-copy\">&#25163;&#26426;&#12289;&#24179;&#26495;&#21644;&#20854;&#20182;&#30005;&#33041;&#21482;&#35201;&#22312;&#21516;&#19968;&#32593;&#32476;&#65292;&#23601;&#33021;&#30452;&#25509;&#25171;&#24320;&#36825;&#20010;&#20849;&#20139;&#31354;&#38388;&#12290;</p></div><span class=\"tag\">LAN READY</span></div>"
            << "<div class=\"qr-wrap\"><img src=\"/qr.svg?v=2\" alt=\"QR code\"></div><div class=\"connect-url\">" << html_escape(access_url) << "</div>"
            << "<div class=\"actions\"><a class=\"button primary\" href=\"/qr\" target=\"_blank\"><svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\"><path d=\"M15 3h6v6\"/><path d=\"M10 14 21 3\"/><path d=\"M21 14v4a3 3 0 0 1-3 3H6a3 3 0 0 1-3-3V6a3 3 0 0 1 3-3h4\"/></svg>&#25171;&#24320;&#22823;&#23631;&#30721;</a><button class=\"button\" type=\"button\" onclick=\"navigator.clipboard&&navigator.clipboard.writeText('" << html_escape(access_url) << "')\"><svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\"><rect x=\"9\" y=\"9\" width=\"13\" height=\"13\" rx=\"2\"/><path d=\"M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1\"/></svg>&#22797;&#21046;&#38142;&#25509;</button></div></aside>";
    } else {
        html
            << "<aside class=\"connect\"><div class=\"connect-head\"><div><h2 class=\"connect-title\">&#24403;&#21069;&#30446;&#24405;</h2><p class=\"connect-copy\">&#36825;&#26159;&#20849;&#20139;&#31354;&#38388;&#30340;&#23376;&#30446;&#24405;&#35270;&#22270;&#65292;&#21487;&#20197;&#38543;&#26102;&#36820;&#22238;&#19978;&#19968;&#32423;&#12290;</p></div><span class=\"tag\">BROWSE</span></div><div class=\"qr-wrap\"><svg viewBox=\"0 0 220 220\" width=\"220\" height=\"220\" fill=\"none\"><rect x=\"22\" y=\"34\" width=\"176\" height=\"132\" rx=\"28\" fill=\"#fff\" stroke=\"rgba(25,31,44,.12)\"/><path d=\"M67 80h37l13 14h36\" stroke=\"#1677ff\" stroke-width=\"11\" stroke-linecap=\"round\"/><path d=\"M58 107h104\" stroke=\"#15b88f\" stroke-width=\"11\" stroke-linecap=\"round\"/><path d=\"M58 132h72\" stroke=\"#bd8755\" stroke-width=\"11\" stroke-linecap=\"round\"/></svg></div><div class=\"connect-url\">/" << html_escape(current_relative) << "</div></aside>";
    }

    html
        << "</section><section class=\"files\"><div class=\"files-head\">"
        << "<div><div class=\"files-title\">&#20849;&#20139;&#25991;&#20214;</div><div class=\"files-sub\">&#25353;&#25991;&#20214;&#22841;&#20248;&#20808;&#25490;&#21015;&#65292;&#28857;&#20987;&#21363;&#21487;&#36827;&#20837;&#25110;&#19979;&#36733;</div></div>"
        << "<div style=\"display:flex;gap:10px;align-items:center;\">"
        << "<div class=\"pill\">" << entries.size() << " &#20010;&#26465;&#30446;</div>"
        << "<label class=\"button primary\" style=\"cursor:pointer;margin:0;min-height:34px;padding:0 14px;font-size:13px;\">"
        << "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\"><path d=\"M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4\"/><polyline points=\"17 8 12 3 7 8\"/><line x1=\"12\" y1=\"3\" x2=\"12\" y2=\"15\"/></svg>"
        << "&#19978;&#20256;&#25991;&#20214;<input type=\"file\" id=\"file-upload\" multiple style=\"display:none;\"></label>"
        << "</div></div><div class=\"list\">";

    if (!is_root_view) {
        std::string parent_relative = relative_url_path(root, parent_path(current));
        html << "<a class=\"row\" href=\"/browse/" << url_encode(parent_relative) << "\"><span class=\"icon parent\"><svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\"><path d=\"m12 19-7-7 7-7\"/><path d=\"M19 12H5\"/></svg></span><span class=\"name\">&#36820;&#22238;&#19978;&#19968;&#32423;</span><span class=\"meta\">folder</span></a>";
    }

    if (entries.empty()) {
        html << "<div class=\"empty\"><strong>&#36825;&#20010;&#25991;&#20214;&#22841;&#36824;&#26159;&#31354;&#30340;</strong><span>&#25918;&#20837;&#25991;&#20214;&#21518;&#21047;&#26032;&#39029;&#38754;&#21363;&#21487;&#20986;&#29616;&#12290;</span></div>";
    }

    for (size_t i = 0; i < entries.size(); ++i) {
        const FileEntry& entry = entries[i];
        std::string name = wide_to_utf8(entry.name);
        std::string relative = relative_url_path(root, entry.path);
        std::string escaped_name = html_escape(name);

        if (entry.is_directory) {
            html << "<a class=\"row\" href=\"/browse/" << url_encode(relative) << "\"><span class=\"icon folder\"><svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\"><path d=\"M3 7h6l2 2h10v9a3 3 0 0 1-3 3H6a3 3 0 0 1-3-3z\"/></svg></span><span class=\"name\">" << escaped_name << "</span><span class=\"meta\">folder</span></a>";
        } else {
            html << "<a class=\"row\" href=\"/download/" << url_encode(relative) << "\"><span class=\"icon\"><svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\"><path d=\"M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z\"/><path d=\"M14 2v6h6\"/><path d=\"M12 18v-6\"/><path d=\"m9 15 3 3 3-3\"/></svg></span><span class=\"name\">" << escaped_name << "</span><span class=\"meta\">" << file_size_text(entry.size) << "</span></a>";
        }
    }

    html << "</div></section></main>"
         << "<script>"
         << "document.getElementById('file-upload').addEventListener('change', async function(e) {"
         << "  const files = e.target.files;"
         << "  if(files.length === 0) return;"
         << "  const fd = new FormData();"
         << "  for(let i=0; i<files.length; i++) fd.append('file', files[i]);"
         << "  const btn = this.parentElement;"
         << "  const origText = btn.innerHTML;"
         << "  btn.innerHTML = '&#19978;&#20256;&#20013;...';"
         << "  try {"
         << "    const res = await fetch('/api/upload?path=' + encodeURIComponent('" << json_escape(current_relative) << "'), {"
         << "      method: 'POST', body: fd"
         << "    });"
         << "    if(res.ok) window.location.reload();"
         << "    else alert('&#19978;&#20256;&#22833;&#36133;: ' + await res.text());"
         << "  } catch(err) {"
         << "    alert('&#19978;&#20256;&#20986;&#38169;: ' + err.message);"
         << "  } finally {"
         << "    btn.innerHTML = origText;"
         << "    e.target.value = '';"
         << "  }"
         << "});"
         << "</script></body></html>";
    return html.str();
}

std::string render_directory_json(const std::wstring& root, const std::wstring& current, const std::string& access_url, int offset, int limit) {
    std::vector<FileEntry> entries = list_directory(current);

    std::sort(entries.begin(), entries.end(), [](const FileEntry& a, const FileEntry& b) {
        if (a.is_directory != b.is_directory) {
            return a.is_directory > b.is_directory;
        }
        return lowercase_path(a.name) < lowercase_path(b.name);
    });

    size_t folder_count = 0;
    size_t file_count = 0;
    unsigned long long total_size = 0;
    for (size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].is_directory) {
            ++folder_count;
        } else {
            ++file_count;
            total_size += entries[i].size;
        }
    }

    int total_entries = static_cast<int>(entries.size());
    int start_idx = std::min(offset, total_entries);
    int end_idx = std::min(start_idx + limit, total_entries);
    bool has_more = end_idx < total_entries;

    std::string current_relative = relative_url_path(root, current);
    bool is_root_view = lowercase_path(full_path(current)) == lowercase_path(full_path(root));

    std::ostringstream json;
    json
        << "{"
        << "\"currentPath\":\"" << json_escape(current_relative) << "\","
        << "\"parentPath\":";

    if (is_root_view) {
        json << "null";
    } else {
        json << "\"" << json_escape(relative_url_path(root, parent_path(current))) << "\"";
    }

    json
        << ",\"accessUrl\":\"" << json_escape(access_url) << "\","
        << "\"stats\":{"
        << "\"folders\":" << folder_count << ","
        << "\"files\":" << file_count << ","
        << "\"totalBytes\":" << total_size << ","
        << "\"totalSize\":\"" << json_escape(file_size_text(total_size)) << "\""
        << "},"
        << "\"pagination\":{"
        << "\"total\":" << total_entries << ","
        << "\"offset\":" << start_idx << ","
        << "\"limit\":" << limit << ","
        << "\"hasMore\":" << (has_more ? "true" : "false")
        << "},"
        << "\"entries\":[";

    for (int i = start_idx; i < end_idx; ++i) {
        const FileEntry& entry = entries[i];
        std::string name = wide_to_utf8(entry.name);
        std::string relative = relative_url_path(root, entry.path);
        if (i != start_idx) {
            json << ",";
        }
        json
            << "{"
            << "\"name\":\"" << json_escape(name) << "\","
            << "\"path\":\"" << json_escape(relative) << "\","
            << "\"type\":\"" << (entry.is_directory ? "folder" : "file") << "\","
            << "\"sizeBytes\":" << entry.size << ","
            << "\"size\":\"" << json_escape(entry.is_directory ? std::string("folder") : file_size_text(entry.size)) << "\","
            << "\"url\":\"" << (entry.is_directory ? "/api/list?path=" : "/download/") << url_encode(relative) << "\""
            << "}";
    }

    json << "]}";
    return json.str();
}

std::string make_qr_svg(const std::string& value) {
    qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(value.c_str(), qrcodegen::QrCode::Ecc::LOW);
    const int quiet_zone = 4;
    const int module_pixels = 8;
    const int module_count = qr.getSize();
    const int viewbox_size = module_count + quiet_zone * 2;
    std::ostringstream svg;

    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""
        << (viewbox_size * module_pixels) << "\" height=\"" << (viewbox_size * module_pixels)
        << "\" viewBox=\"0 0 "
        << viewbox_size << ' ' << viewbox_size
        << "\" shape-rendering=\"crispEdges\">"
        << "<rect width=\"100%\" height=\"100%\" fill=\"#fff\"/>";

    for (int y = 0; y < module_count; ++y) {
        for (int x = 0; x < module_count; ++x) {
            if (qr.getModule(x, y)) {
                svg << "<rect x=\"" << (x + quiet_zone)
                    << "\" y=\"" << (y + quiet_zone)
                    << "\" width=\"1\" height=\"1\" fill=\"#000\"/>";
            }
        }
    }

    svg << "</svg>";
    return svg.str();
}
