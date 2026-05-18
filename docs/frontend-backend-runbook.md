# LocalFileShare 前后端运行说明

本文档说明当前项目的完整运行方式：C++ 程序负责本地文件共享服务，Vue 前端负责展示页面、目录跳转和下载交互。

## 1. 当前架构

项目现在分为两部分：

```text
local-file-share/
├─ src/main.cpp                 C++ 后端入口
├─ build/Debug/LocalFileShare.exe
├─ frontend/                    Vue + Vite 前端项目
│  ├─ package.json
│  ├─ vite.config.js
│  └─ src/
│     ├─ App.vue
│     └─ style.css
├─ start-dev.cmd                一键启动开发环境
└─ docs/
   └─ frontend-backend-runbook.md
```

C++ 后端提供：

```text
GET /api/list                   返回共享目录 JSON
GET /api/list?path=<relative>   返回子目录 JSON
GET /download/<relative>        下载文件
GET /qr                         显示二维码页面
GET /qr.svg                     二维码图片
GET /browse/<relative>          C++ 内置 HTML 目录页
```

Vue 前端提供：

```text
http://127.0.0.1:5173/
```

开发时 Vue 通过 Vite 代理访问 C++ 后端：

```text
/api      -> http://127.0.0.1:8080
/download -> http://127.0.0.1:8080
/qr       -> http://127.0.0.1:8080
```

## 2. 依赖环境

需要安装：

```text
Visual Studio 2022 或 Visual Studio Build Tools
CMake
Node.js
npm
```

在你的电脑上，PowerShell 禁用了 `npm.ps1`，所以运行 npm 时推荐使用：

```bat
npm.cmd
```

不要直接在 PowerShell 中写：

```powershell
npm run dev
```

如果遇到脚本执行策略报错，改用：

```powershell
npm.cmd run dev
```

## 3. 共享目录

当前开发启动脚本默认共享目录是：

```text
D:\Share
```

也就是说，Vue 页面里显示、跳转、下载的文件都来自：

```text
D:\Share
```

如果这个目录不存在，请先创建：

```bat
mkdir D:\Share
```

然后放入一些测试文件，例如：

```text
D:\Share\test.txt
D:\Share\photos\image.png
D:\Share\packages\demo.zip
```

如果要修改共享目录，打开项目根目录的：

```text
start-dev.cmd
```

修改这一行：

```bat
set SHARE_DIR=D:\Share
```

例如改成：

```bat
set SHARE_DIR=E:\MyFiles
```

路径中如果有空格也可以，例如：

```bat
set SHARE_DIR=D:\My Share Folder
```

脚本内部已经会对共享目录加引号。

## 4. 首次安装前端依赖

进入前端目录：

```bat
cd /d D:\Code\local-file-share\frontend
```

安装依赖：

```bat
npm.cmd install
```

只需要首次安装一次。以后如果 `package.json` 依赖变化，再重新运行。

## 5. 构建 C++ 后端

在项目根目录运行：

```bat
cd /d D:\Code\local-file-share
cmake -S . -B build
cmake --build build
```

构建完成后应生成：

```text
D:\Code\local-file-share\build\Debug\LocalFileShare.exe
```

如果你使用 Release 构建，也可以运行：

```bat
cmake --build build --config Release
```

对应可执行文件通常在：

```text
D:\Code\local-file-share\build\Release\LocalFileShare.exe
```

当前 `start-dev.cmd` 使用的是 Debug 版本：

```text
build\Debug\LocalFileShare.exe
```

## 6. 推荐开发启动方式

推荐直接运行：

```bat
cd /d D:\Code\local-file-share
start-dev.cmd
```

脚本会做这些事：

1. 检查 `build\Debug\LocalFileShare.exe` 是否存在。
2. 使用 `D:\Share` 启动 C++ 后端。
3. 后端监听：

```text
http://127.0.0.1:8080
```

4. 等待 `/api/list` 返回成功。
5. 启动 Vue 前端：

```text
http://127.0.0.1:5173
```

启动成功后，打开浏览器访问：

```text
http://127.0.0.1:5173/
```

## 7. 手动启动方式

如果不使用 `start-dev.cmd`，可以开两个命令行窗口。

第一个窗口启动 C++ 后端：

```bat
cd /d D:\Code\local-file-share
build\Debug\LocalFileShare.exe --dir D:\Share --host 127.0.0.1 --port 8080 --no-open
```

第二个窗口启动 Vue 前端：

```bat
cd /d D:\Code\local-file-share\frontend
npm.cmd run dev -- --port 5173 --strictPort
```

然后打开：

```text
http://127.0.0.1:5173/
```

## 8. 为什么开发时使用 127.0.0.1

开发模式下，Vue 前端和 C++ 后端都在本机运行：

```text
Vue:  http://127.0.0.1:5173
C++:  http://127.0.0.1:8080
```

Vue 页面通过 Vite 代理访问后端，所以 C++ 后端只需要监听本机地址：

```bat
--host 127.0.0.1
```

这能避免 Windows 防火墙、无线网卡、虚拟网卡导致的监听异常。

如果你想让手机访问 C++ 后端内置页面，可以单独启动局域网模式：

```bat
build\Debug\LocalFileShare.exe --dir D:\Share --host 0.0.0.0 --port 8080
```

然后手机访问：

```text
http://你的电脑局域网IP:8080
```

注意：Vue 开发服务默认只给本机开发使用。要让手机访问 Vue 页面，需要额外把 Vite host 改为局域网可访问地址或 `0.0.0.0`。

## 9. 页面功能说明

Vue 页面当前已经连接真实 C++ 后端：

```text
启动后请求 /api/list
点击文件夹进入子目录
点击文件触发 /download/<path> 下载
点击“返回上级”回到父目录
点击“刷新”重新请求当前目录
点击“显示二维码”打开 /qr
点击“复制访问链接”复制 C++ 后端访问地址
搜索框只过滤当前目录已加载的条目
```

## 10. 常见错误

### 10.1 `Error: connect ECONNREFUSED 127.0.0.1:8080`

含义：

```text
Vue 前端启动了，但 C++ 后端没有在 8080 运行。
```

处理：

```bat
cd /d D:\Code\local-file-share
start-dev.cmd
```

或者手动确认：

```bat
build\Debug\LocalFileShare.exe --dir D:\Share --host 127.0.0.1 --port 8080 --no-open
```

再打开：

```text
http://127.0.0.1:8080/api/list
```

如果能看到 JSON，说明后端正常。

### 10.2 `Shared directory does not exist or is not a directory`

含义：

```text
传给 --dir 的共享目录不存在，或者命令参数引号拼错了。
```

正确示例：

```bat
build\Debug\LocalFileShare.exe --dir D:\Share --host 127.0.0.1 --port 8080 --no-open
```

错误示例：

```text
D:\Code\local-file-share" --host 127.0.0.1 --port 8080 --no-open
```

这种情况说明 `.cmd` 的引号把后面的参数拼进了 `--dir`。当前新版 `start-dev.cmd` 已经修复了这个问题。

### 10.3 `npm.ps1 cannot be loaded`

含义：

```text
PowerShell 禁止执行 npm.ps1。
```

处理：

```bat
npm.cmd install
npm.cmd run dev
```

### 10.4 `Port 5173 is already in use`

含义：

```text
Vue 开发服务已经启动过，或者旧进程没有关闭。
```

处理：

在旧的 Vite 窗口按：

```text
Ctrl + C
```

然后重新运行：

```bat
start-dev.cmd
```

### 10.5 `Backend did not start on 127.0.0.1:8080`

处理步骤：

1. 查看打开的 `LocalFileShare Backend :8080` 窗口。
2. 确认 `D:\Share` 存在。
3. 确认 `8080` 没有被其他程序占用。
4. 手动运行后端命令看错误：

```bat
cd /d D:\Code\local-file-share
build\Debug\LocalFileShare.exe --dir D:\Share --host 127.0.0.1 --port 8080 --no-open
```

## 11. 端口说明

默认端口：

```text
C++ 后端: 8080
Vue 前端: 5173
```

如果要修改 C++ 后端端口，需要同步修改：

```text
start-dev.cmd
frontend/vite.config.js
```

例如后端改为 8081：

`start-dev.cmd`：

```bat
set BACKEND_URL=http://127.0.0.1:8081/api/list
...
--port 8081
```

`frontend/vite.config.js`：

```js
proxy: {
  '/api': 'http://127.0.0.1:8081',
  '/download': 'http://127.0.0.1:8081',
  '/qr': 'http://127.0.0.1:8081',
}
```

## 12. 前端生产构建

进入前端目录：

```bat
cd /d D:\Code\local-file-share\frontend
```

构建：

```bat
npm.cmd run build
```

生成目录：

```text
D:\Code\local-file-share\frontend\dist
```

当前生产构建产物还没有自动嵌入 C++ 后端。开发时请继续使用：

```bat
start-dev.cmd
```

后续如果要发布成单个 C++ 程序，可以把 `frontend/dist` 的静态文件接入 C++ 静态资源路由。

## 13. 验证清单

启动前确认：

```text
D:\Share 存在
build\Debug\LocalFileShare.exe 存在
frontend\node_modules 存在
```

启动后验证：

```text
http://127.0.0.1:8080/api/list 返回 JSON
http://127.0.0.1:5173/ 打开 Vue 页面
Vue 页面能显示 D:\Share 下的真实文件
点击文件夹能进入子目录
点击文件能下载
```

