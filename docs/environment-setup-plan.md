# LocalFileShare 环境配置与安装计划

本文档用于规划 LocalFileShare 的开发环境、依赖安装、构建方式和基础验证流程。当前阶段目标不是一次性完成所有平台支持，而是先把 Windows 下的 C++ 开发链路跑通，为后续实现 HTTP 文件共享服务打基础。

## 1. 环境目标

第一阶段优先支持：

- Windows 10 / Windows 11
- C++11
- CMake
- Visual Studio 2022 Build Tools 或完整 Visual Studio 2022
- `cpp-httplib` 作为轻量 HTTP 服务库
- Win32 API 作为第一版文件系统访问方案

后续扩展目标：

- Linux
- macOS
- MinGW / Ninja 构建
- GitHub Actions 自动构建

## 2. Windows 开发环境

### 2.1 安装 Git

用途：

- 拉取项目代码
- 管理版本提交
- 后续引入第三方依赖

安装地址：

```text
https://git-scm.com/download/win
```

安装完成后验证：

```powershell
git --version
```

预期输出示例：

```text
git version 2.x.x.windows.x
```

### 2.2 安装 Visual Studio 2022

推荐安装：

- Visual Studio 2022 Community
- 或 Visual Studio 2022 Build Tools

安装时必须勾选：

- Desktop development with C++
- MSVC v143 C++ build tools
- Windows 10/11 SDK
- C++ CMake tools for Windows

安装完成后验证：

```powershell
cl
```

如果能看到 Microsoft C/C++ Compiler 版本信息，说明 MSVC 可用。

### 2.3 安装 CMake

用途：

- 统一管理 C++ 构建
- 后续兼容 Windows、Linux、macOS

安装地址：

```text
https://cmake.org/download/
```

安装时建议勾选：

```text
Add CMake to the system PATH
```

安装完成后验证：

```powershell
cmake --version
```

预期输出示例：

```text
cmake version 3.x.x
```

## 3. 项目依赖规划

### 3.1 必选依赖

第一版建议只使用一个第三方 HTTP 库：

```text
cpp-httplib
```

原因：

- 单头文件
- 使用简单
- 非常适合本项目的轻量定位
- 可以快速实现 HTTP 服务、路由、文件下载

计划放置位置：

```text
third_party/httplib.h
```

### 3.2 标准库依赖

项目第一版使用 C++11 标准库能力：

- `std::string`
- `std::vector`
- `std::wstring`
- `std::ostringstream`
- C 标准文件读写 API

第一版在 Windows 上使用 Win32 API 负责：

- 遍历共享目录
- 判断文件 / 文件夹
- 读取文件大小
- 处理路径规范化
- 做路径安全校验

相关 API：

- `GetFullPathNameW`
- `GetFileAttributesW`
- `GetFileAttributesExW`
- `FindFirstFileW`
- `FindNextFileW`
- `WideCharToMultiByte`
- `MultiByteToWideChar`

## 4. 推荐目录结构

环境配置完成后，项目目录建议逐步整理为：

```text
local-file-share/
├── CMakeLists.txt
├── README.md
├── docs/
│   └── environment-setup-plan.md
├── include/
│   ├── Server.h
│   ├── FileManager.h
│   └── HtmlRenderer.h
├── src/
│   ├── main.cpp
│   ├── Server.cpp
│   ├── FileManager.cpp
│   └── HtmlRenderer.cpp
├── third_party/
│   └── httplib.h
└── tests/
    └── path_security_test.cpp
```

## 5. 构建计划

### 5.1 创建构建目录

建议使用 out-of-source build，避免构建文件污染源码目录。

```powershell
cmake -S . -B build
```

### 5.2 编译项目

```powershell
cmake --build build --config Release
```

编译完成后，预期生成：

```text
build/Release/LocalFileShare.exe
```

### 5.3 Debug 构建

开发阶段建议使用 Debug：

```powershell
cmake --build build --config Debug
```

Debug 版本便于后续排查：

- 路由问题
- 路径问题
- 中文文件名问题
- 文件下载问题

## 6. 运行验证计划

### 6.1 本机访问验证

启动命令计划：

```powershell
.\build\Debug\LocalFileShare.exe --dir D:\Share --port 8080
```

浏览器访问：

```text
http://127.0.0.1:8080
```

需要验证：

- 页面可以打开
- 能显示共享目录
- 能进入子目录
- 能下载文件

### 6.2 局域网访问验证

查询本机局域网 IP：

```powershell
ipconfig
```

假设本机 IP 是：

```text
192.168.1.23
```

手机或其他设备访问：

```text
http://192.168.1.23:8080
```

需要验证：

- 手机和电脑在同一局域网
- Windows 防火墙允许程序访问网络
- 浏览器能打开文件列表页面
- 文件下载正常

## 7. Windows 防火墙处理

首次运行服务时，Windows 可能弹出防火墙提示。

建议允许：

- 专用网络

不建议默认允许：

- 公用网络

如果浏览器无法从手机访问，需要检查：

1. 电脑和手机是否在同一 Wi-Fi。
2. 服务是否监听 `0.0.0.0`，而不是只监听 `127.0.0.1`。
3. Windows 防火墙是否拦截。
4. 端口 `8080` 是否被其他程序占用。

## 8. 端口规划

默认端口：

```text
8080
```

如果端口被占用，可以手动指定：

```powershell
.\LocalFileShare.exe --dir D:\Share --port 8081
```

后续可实现：

- 启动时检测端口占用
- 自动尝试 `8081`、`8082`、`8083`
- 在控制台打印最终访问地址

## 9. 中文路径与编码计划

项目需要重点支持：

- 中文文件名
- 中文文件夹名
- Windows 中文路径
- 浏览器 URL 编码 / 解码

计划原则：

- Web 页面统一输出 UTF-8
- HTTP Header 使用 UTF-8 相关声明
- Windows 文件路径内部优先使用 `std::wstring`
- 文件系统操作优先使用宽字符 Win32 API
- 页面显示时做 HTML 转义
- URL 路径参数做 decode 后再映射到真实路径

需要验证的示例：

```text
D:\Share\图片\测试文件.txt
D:\Share\视频\春节录像.mp4
```

## 10. 第三方依赖管理计划

第一阶段：

- 手动下载 `httplib.h`
- 放入 `third_party/httplib.h`
- CMake 直接 include `third_party`

后续可选：

- Git submodule
- FetchContent
- vcpkg

当前不建议一开始引入复杂包管理器，因为项目目标是轻量、易构建、低门槛。

## 11. 基础验收清单

环境配置完成后，需要满足：

- `git --version` 可用
- `cmake --version` 可用
- `cl` 或 Visual Studio C++ 工具链可用
- 项目可以执行 `cmake -S . -B build`
- 项目可以执行 `cmake --build build`
- 可以生成 `LocalFileShare.exe`
- 本机浏览器可以访问 `http://127.0.0.1:8080`
- 手机可以访问 `http://电脑局域网IP:8080`

## 12. 下一步任务

完成本文档后，下一步建议按顺序实现：

1. 创建 `CMakeLists.txt`。
2. 创建 `src/main.cpp`。
3. 引入 `third_party/httplib.h`。
4. 实现最小 HTTP 服务。
5. 浏览器访问时返回一个简单 HTML 页面。
6. 再开始加入目录浏览和文件下载功能。

## 13. 当前开发机检查记录

检查日期：2026-05-15

当前机器检查结果：

- `cmake`：未安装或未加入 PATH
- `cl`：未安装或未加载 Visual Studio 开发者环境
- Visual Studio：已选择“使用 C++ 的桌面开发”
- `g++`：存在，路径为 `C:\MinGW\bin\g++.exe`
- `g++` 版本：`6.3.0`

结论：

当前第一版会改为 Windows + Visual Studio + C++11 路线，不依赖 C++17 `<filesystem>`。Visual Studio 安装器中只需要保留“使用 C++ 的桌面开发”，并确认右侧包含：

- MSVC v143 C++ 生成工具
- Windows 10/11 SDK
- C++ CMake tools for Windows

如果 `cmake` 命令仍不可用，可以直接用 Visual Studio 打开项目文件夹，或从“开发者 PowerShell for VS 2022”中执行构建命令。
