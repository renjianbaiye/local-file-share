# 本地智能相册 / 智能照片仓库设计

## 1. 目标

LocalFileShare 当前是一个轻量的局域网文件分享工具。下一步要把它扩展成一个本地智能相册 / 智能照片仓库。

第一版的核心目标不是马上做复杂 AI，而是先把相册框架跑通、跑顺、跑稳定：

- 能扫描本地分享目录里的照片、视频、RAW 文件。
- 能把媒体文件写入本地索引。
- 能生成和缓存缩略图。
- 能在前端用相册视图浏览。
- 能按时间轴、文件夹、媒体类型、收藏状态做基础筛选。
- 能让上传后的照片和视频自动进入相册索引。
- 能为后续 AI 标签、人脸、地点、Redis、MySQL 留出扩展位置。

第一版应保持“打开程序即可使用”的轻量体验，因此默认不依赖 Redis、MySQL 或外部服务。

## 2. 当前项目背景

现有项目结构：

- 后端：C++，使用 `httplib` 提供 HTTP 服务。
- 前端：Vue，位于 `frontend/`。
- 已有能力：局域网访问地址、二维码、访问 token、目录浏览、文件下载、多文件上传。
- 当前 UI 更偏文件管理器，还没有照片仓库、时间轴、缩略图缓存、索引数据库。

新功能应该作为并列能力加入，而不是替换现有文件分享功能。

前端入口建议：

```text
[ 文件 ] [ 相册 ]
```

`文件` 保留现有目录浏览和上传下载。  
`相册` 新增照片仓库体验。

## 3. 第一版范围

### 3.1 第一版要做

- 新增相册标签页。
- 支持 8,000 到 20,000 级别媒体文件的基础使用。
- 使用 SQLite 保存本地索引。
- 使用本地磁盘保存缩略图缓存。
- 启动时轻量检查，后台增量扫描。
- 提供手动刷新索引按钮。
- 默认时间轴网格浏览。
- 支持按文件夹筛选。
- 支持基础搜索入口。
- 支持收藏 / 取消收藏。
- 支持 JPG、JPEG、PNG、WebP 缩略图。
- 支持视频入库，并让浏览器可播放的视频能在前端播放。
- 支持 RAW 入库，但第一版先显示占位和元数据。
- 上传成功后自动加入索引队列。

### 3.2 第一版不做

- 不做完整 AI 图片识别。
- 不做人脸识别。
- 不做人脸聚类。
- 不做地点地图。
- 不做 RAW 预览图生成。
- 不做高级视频抽帧缩略图。
- 不做多用户权限体系。
- 不在相册 UI 中删除、移动、重命名原始文件。
- 不强制安装 Redis。
- 不强制安装 MySQL。

## 4. 第一版分步骤拆分

第一版也要拆成多个小步骤实现，避免一次性铺太大。

### V1.0：相册基础骨架

目标：让前后端有完整相册入口，但可以先显示空状态或假数据。

功能点：

- 前端新增 `文件 / 相册` 标签切换。
- `文件` 标签继续复用现有目录浏览能力。
- `相册` 标签新增基础页面结构。
- 相册页面包含：
  - 顶部搜索框。
  - 媒体类型筛选入口。
  - 文件夹筛选入口。
  - 刷新索引按钮。
  - 扫描状态区域。
  - 相册网格区域。
- 后端预留 `/api/photos/*` 路由分组。
- 相册接口初期可返回空列表，先跑通前后端通信。

验收标准：

- 现有文件分享功能不受影响。
- 页面可以在文件视图和相册视图之间切换。
- 相册页面能请求后端接口并展示空状态。

### V1.1：SQLite 索引基础

目标：建立照片仓库的本地持久化基础。

功能点：

- 引入 SQLite。
- 程序启动时创建或打开 `photos.db`。
- 增加数据库 schema 初始化逻辑。
- 增加 schema version，方便后续迁移。
- 建立 `photos`、`folders`、`scan_runs`、`jobs` 表。
- 建立 `PhotoRepository` 接口。
- 第一版实现 `SQLitePhotoRepository`。
- 提供基础增删改查能力：
  - 写入照片记录。
  - 更新照片记录。
  - 标记文件缺失。
  - 查询时间轴列表。
  - 查询文件夹列表。
  - 更新收藏状态。

验收标准：

- 启动程序后能自动创建数据库。
- 重启后索引数据仍然存在。
- repository 测试能覆盖插入、更新、查询、收藏。

### V1.2：媒体扫描与增量索引

目标：可以扫描分享目录，并把媒体文件写入索引。

功能点：

- 增加 `PhotoService`。
- 增加后台扫描线程。
- 支持手动触发扫描：

```http
POST /api/photos/scan
```

- 支持查询扫描状态：

```http
GET /api/photos/scan/status
```

- 扫描分享目录下的媒体文件。
- 根据扩展名识别：
  - 图片：JPG、JPEG、PNG、WebP、HEIC。
  - 视频：MP4、WebM、MOV 等。
  - RAW：NEF、NRW、CR2、ARW、DNG 等。
- 写入基础元数据：
  - 相对路径。
  - 文件名。
  - 文件夹路径。
  - 扩展名。
  - 媒体类型。
  - 文件大小。
  - 修改时间。
  - 入库时间。
- 增量扫描时：
  - 新文件插入。
  - 大小或修改时间变化的文件更新。
  - 不存在的文件标记为 `missing`。
- 更新 `folders` 聚合信息。
- 同一时间只允许一个扫描任务运行。

验收标准：

- 手动点击刷新后能扫描目录。
- 扫描状态能显示 `scanning`、`completed`、`failed`。
- 新增文件能进入索引。
- 删除文件后能在下一次扫描中标记缺失。

### V1.3：相册时间轴 API

目标：前端能从真实索引读取相册列表。

功能点：

- 增加时间轴接口：

```http
GET /api/photos/timeline?cursor=<cursor>&limit=100&folder=<path>&media=image,video,raw&favorite=1
```

- 排序规则：
  - 优先使用拍摄时间 `captured_at`。
  - 没有拍摄时间时使用修改时间 `modified_at`。
  - 默认最新在前。
- 支持分页。
- 支持按文件夹筛选。
- 支持按媒体类型筛选。
- 支持只看收藏。
- 返回下载地址和缩略图地址。

示例响应：

```json
{
  "items": [
    {
      "id": 101,
      "fileName": "DSC_0001.JPG",
      "relativePath": "camera/DSC_0001.JPG",
      "folderPath": "camera",
      "mediaType": "image",
      "sizeBytes": 7340032,
      "capturedAt": 1779166800,
      "modifiedAt": 1779166810,
      "width": 6000,
      "height": 4000,
      "thumbnailStatus": "pending",
      "thumbnailUrl": "/api/photos/101/thumbnail",
      "downloadUrl": "/download/camera/DSC_0001.JPG",
      "isFavorite": false
    }
  ],
  "nextCursor": "1779166800:101"
}
```

验收标准：

- 相册页能显示真实媒体文件。
- 8K 级别文件不一次性全部返回。
- 翻页或继续加载能正常工作。

### V1.4：前端时间轴网格

目标：把相册从文件列表变成可用的照片浏览体验。

功能点：

- 相册页默认显示时间轴网格。
- 按日期分组展示。
- 支持缩略图占位状态：
  - `pending`：生成中。
  - `ready`：显示缩略图。
  - `failed`：显示失败占位。
  - `unsupported`：显示类型占位。
- 支持媒体类型角标：
  - 图片。
  - 视频。
  - RAW。
- 支持继续加载。
- 支持移动端响应式布局。
- 支持空状态：
  - 未扫描。
  - 没有媒体文件。
  - 搜索无结果。

验收标准：

- 大量照片不会一次性渲染卡死。
- 图片、视频、RAW 有不同视觉状态。
- 移动端能正常浏览。

### V1.5：缩略图生成与缓存

目标：支持常见图片缩略图，提高浏览体验。

功能点：

- 增加缩略图缓存目录：

```text
%LOCALAPPDATA%\LocalFileShare\thumbnails\
```

- 增加缩略图状态字段。
- 扫描发现新图片后创建缩略图任务。
- 后台 worker 处理缩略图任务。
- 支持 JPG、JPEG、PNG、WebP 生成缩略图。
- HEIC 如果当前库不支持，则显示占位。
- RAW 第一版显示 RAW 占位。
- 视频第一版可先显示视频占位。
- 缩略图通过后端接口访问：

```http
GET /api/photos/:id/thumbnail
```

- 不直接暴露本地文件路径。

验收标准：

- 支持的图片能生成缩略图。
- 生成失败不会影响相册列表。
- 重启后缩略图缓存仍然可用。

### V1.6：基础搜索与筛选

目标：做出“智能搜索”的入口，但第一版先基于元数据。

功能点：

- 增加搜索接口：

```http
GET /api/photos/search?q=<text>&media=image,video,raw&from=<timestamp>&to=<timestamp>&favorite=1&limit=100
```

- 支持搜索：
  - 文件名。
  - 文件夹路径。
  - 媒体类型。
  - 日期范围。
  - 收藏状态。
- 前端搜索框放在相册工具栏。
- 搜索结果继续复用相册网格。
- UI 可以保留智能搜索的形态，但文案不要承诺 AI 已经可用。

验收标准：

- 按文件名能找到照片。
- 按文件夹能筛选照片。
- 按媒体类型能筛选图片、视频、RAW。
- 搜索无结果时有清晰提示。

### V1.7：预览、视频播放、收藏

目标：让相册可用性更接近真实照片库。

功能点：

- 点击相册项打开预览层。
- 图片：显示大图或原图。
- 视频：浏览器支持的格式使用 `<video>` 播放。
- RAW：显示 RAW 占位和文件元数据。
- 预览层显示：
  - 文件名。
  - 文件夹。
  - 文件大小。
  - 日期。
  - 媒体类型。
  - 下载按钮。
  - 收藏按钮。
- 增加收藏接口：

```http
POST /api/photos/:id/favorite
```

请求体：

```json
{ "favorite": true }
```

- 收藏状态只写 SQLite，不修改原始照片。

验收标准：

- 图片能预览。
- 常见 MP4 能播放。
- RAW 有清楚占位。
- 收藏后刷新页面仍然保留。

### V1.8：上传后自动入库

目标：把现有上传功能和相册仓库连接起来。

功能点：

- 现有 `/api/upload` 上传成功后，不阻塞响应。
- 后台把上传文件加入索引任务。
- 如果是支持缩略图的图片，加入缩略图任务。
- 前端上传完成后刷新当前文件视图。
- 相册页在扫描完成后能看到新上传媒体。

验收标准：

- 上传 JPG 后能进入相册。
- 上传视频后能进入相册。
- 上传 RAW 后能显示 RAW 占位。
- 上传接口不会因为缩略图生成变慢。

### V1.9：稳定性与体验收口

目标：让第一版可以长期使用。

功能点：

- 数据库打开失败时，文件分享功能仍可尽量运行。
- 相册不可用时前端显示明确错误。
- 扫描失败时保留旧索引。
- 缩略图失败时保留媒体项。
- 重复触发扫描时返回当前扫描状态，不创建多个扫描。
- 给常用字段加索引：
  - `relative_path`
  - `folder_path`
  - `media_type`
  - `captured_at`
  - `modified_at`
  - `is_favorite`
  - `missing`
- 前端移动端布局不溢出、不重叠。
- 写基础测试和手工验收清单。

验收标准：

- 8K 左右媒体库可正常扫描、分页、浏览。
- 相册页不会因为部分坏文件整体失败。
- 现有文件分享功能仍然稳定。

## 5. 总体架构

```text
Vue 前端
  - 文件视图
  - 相册视图
  - 时间轴网格
  - 文件夹筛选
  - 基础搜索
  - 扫描状态
  - 预览层

C++ HTTP API
  - 现有文件 API
  - 照片时间轴 API
  - 照片搜索 API
  - 文件夹聚合 API
  - 扫描任务 API
  - 缩略图 API
  - 收藏 API

PhotoService
  - 扫描目录
  - 提取基础元数据
  - 调度缩略图任务
  - 上传后入库
  - 更新文件夹聚合

Storage Layer
  - PhotoRepository 接口
  - SQLitePhotoRepository 第一版实现
  - MySqlPhotoRepository 后续可扩展

Runtime Layer
  - 第一版本地内存队列
  - 后续可扩展 Redis 队列 / 缓存
```

## 6. 存储设计

第一版使用 SQLite，不使用 JSON 作为主索引。

原因：

- 8K 到 2W 媒体文件已经需要分页、筛选和增量更新。
- SQLite 不需要用户安装数据库服务。
- SQLite 比 JSON 更适合收藏、筛选、扫描状态、任务状态。
- 后续迁移 MySQL 时，可以通过 Repository 接口替换底层实现。

### 6.1 Repository 边界

业务层不要到处直接拼 SQLite SQL。建议抽象：

```text
PhotoRepository
  - upsertPhoto(photo)
  - markMissing(relativePath)
  - listTimeline(query)
  - listByFolder(folderPath, query)
  - searchPhotos(query)
  - toggleFavorite(photoId, favorite)
  - getPhoto(photoId)

ScanRepository
  - createScanRun()
  - updateScanRunProgress()
  - finishScanRun()
  - getLatestScanRun()

JobRepository
  - enqueueJob(job)
  - claimNextJob(type)
  - markJobDone(jobId)
  - markJobFailed(jobId, error)
```

这样以后从 SQLite 换 MySQL，主要改 repository 实现，而不是重写扫描、前端和 API。

## 7. SQLite 表结构

### 7.1 `photos`

```text
id INTEGER PRIMARY KEY
relative_path TEXT NOT NULL UNIQUE
absolute_path_hash TEXT NOT NULL
file_name TEXT NOT NULL
folder_path TEXT NOT NULL
extension TEXT NOT NULL
media_type TEXT NOT NULL
mime_type TEXT
size_bytes INTEGER NOT NULL
modified_at INTEGER NOT NULL
captured_at INTEGER
width INTEGER
height INTEGER
orientation INTEGER
content_hash TEXT
thumbnail_status TEXT NOT NULL
thumbnail_path TEXT
is_favorite INTEGER NOT NULL DEFAULT 0
indexed_at INTEGER NOT NULL
missing INTEGER NOT NULL DEFAULT 0
```

`media_type`：

- `image`
- `video`
- `raw`
- `other`

`thumbnail_status`：

- `pending`
- `ready`
- `failed`
- `unsupported`

### 7.2 `folders`

```text
id INTEGER PRIMARY KEY
relative_path TEXT NOT NULL UNIQUE
photo_count INTEGER NOT NULL DEFAULT 0
video_count INTEGER NOT NULL DEFAULT 0
raw_count INTEGER NOT NULL DEFAULT 0
latest_captured_at INTEGER
latest_modified_at INTEGER
indexed_at INTEGER NOT NULL
```

### 7.3 `scan_runs`

```text
id INTEGER PRIMARY KEY
status TEXT NOT NULL
started_at INTEGER NOT NULL
finished_at INTEGER
total_seen INTEGER NOT NULL DEFAULT 0
total_indexed INTEGER NOT NULL DEFAULT 0
total_updated INTEGER NOT NULL DEFAULT 0
total_removed INTEGER NOT NULL DEFAULT 0
error_message TEXT
```

`status`：

- `idle`
- `scanning`
- `completed`
- `failed`
- `cancelled`

### 7.4 `jobs`

```text
id INTEGER PRIMARY KEY
job_type TEXT NOT NULL
target_path TEXT NOT NULL
status TEXT NOT NULL
attempts INTEGER NOT NULL DEFAULT 0
last_error TEXT
created_at INTEGER NOT NULL
updated_at INTEGER NOT NULL
```

`job_type`：

- `scan`
- `thumbnail`

`status`：

- `pending`
- `running`
- `done`
- `failed`

## 8. 本地文件位置

默认位置：

```text
%LOCALAPPDATA%\LocalFileShare\photos.db
%LOCALAPPDATA%\LocalFileShare\thumbnails\
```

后续可加命令行参数：

```text
--photo-db <path>
--thumb-dir <path>
```

默认不把索引和缩略图写进照片目录，避免污染用户原始照片库。

## 9. 后端 API

### 9.1 扫描

```http
POST /api/photos/scan
```

触发增量扫描。如果已有扫描在运行，则直接返回当前扫描状态。

```http
GET /api/photos/scan/status
```

返回扫描状态。

### 9.2 时间轴

```http
GET /api/photos/timeline?cursor=<cursor>&limit=100&folder=<path>&media=image,video,raw&favorite=1
```

返回相册时间轴数据。

### 9.3 搜索

```http
GET /api/photos/search?q=<text>&media=image,video,raw&from=<timestamp>&to=<timestamp>&favorite=1&limit=100
```

第一版只基于元数据搜索。

### 9.4 文件夹

```http
GET /api/photos/folders
```

返回文件夹聚合数据。

### 9.5 收藏

```http
POST /api/photos/:id/favorite
```

请求：

```json
{ "favorite": true }
```

### 9.6 缩略图

```http
GET /api/photos/:id/thumbnail
```

返回缓存缩略图。失败或不支持时，前端显示占位。

## 10. 前端设计

### 10.1 页面结构

```text
顶部导航 / 品牌区
  - 文件 / 相册 标签

文件视图
  - 现有目录浏览
  - 上传
  - 下载
  - 二维码

相册视图
  - 工具栏
  - 时间轴网格
  - 文件夹筛选
  - 预览层
```

### 10.2 相册工具栏

包含：

- 搜索框。
- 媒体类型筛选。
- 日期筛选。
- 收藏筛选。
- 刷新索引按钮。
- 扫描状态。

### 10.3 时间轴网格

- 默认最新在前。
- 按日期分组。
- 优先用拍摄时间。
- 没有拍摄时间则用修改时间。
- 未知日期放在最后。
- 使用分页或继续加载。
- 缩略图未生成时显示占位。

### 10.4 预览层

点击相册项后打开：

- 图片显示预览。
- 视频显示播放器。
- RAW 显示占位和元数据。
- 提供下载按钮。
- 提供收藏按钮。

## 11. 错误处理

- 数据库打开失败：文件分享尽量继续，相册显示不可用。
- 扫描失败：保留旧索引，显示失败原因。
- 缩略图失败：显示占位，不影响列表。
- 文件缺失：扫描后标记 `missing`，默认不展示。
- 不支持的媒体：可以入库，但显示清晰占位。
- 重复扫描：返回当前扫描状态，不创建并发扫描。

## 12. 性能策略

- 时间轴使用分页或 cursor，不一次返回全部照片。
- 缩略图后台生成，不阻塞请求。
- 上传接口不等待索引和缩略图。
- 第一版不默认计算所有文件 hash。
- 常用查询字段建立索引。
- 前端避免一次渲染几千个 DOM 节点。

## 13. 测试与验收

### 13.1 后端测试

- 路径解析不能越过分享根目录。
- 媒体类型识别正确。
- SQLite 初始化正确。
- repository 插入、更新、查询、收藏正确。
- 增量扫描能处理新增、修改、删除。
- 扫描状态能阻止并发扫描。
- 上传后能加入索引任务。

### 13.2 前端测试

- 文件视图仍可正常使用。
- 相册视图能显示空状态。
- 相册视图能显示真实时间轴。
- 搜索和筛选能调用正确 API。
- 收藏后 UI 更新。
- 扫描状态能显示 idle、scanning、completed、failed。
- 移动端布局不重叠。

### 13.3 手工验收

- 启动后文件分享功能正常。
- 扫描一个含 JPG、PNG、MP4、RAW 的目录。
- 确认相册中能看到媒体文件。
- 确认支持的图片能显示缩略图。
- 确认 MP4 可以播放。
- 确认 RAW 显示占位。
- 确认收藏刷新后仍存在。
- 上传新照片后能进入相册。

## 14. 后续扩展

### 14.1 Redis

Redis 后续只做运行时加速，不做主数据源。

可扩展位置：

```text
PhotoJobQueue
  - InMemoryPhotoJobQueue
  - RedisPhotoJobQueue

PhotoQueryCache
  - NoOpPhotoQueryCache
  - RedisPhotoQueryCache
```

可选 key：

```text
lfs:scan:status
lfs:scan:queue
lfs:thumb:queue
lfs:timeline:page:<cursor>
lfs:folder:<folderHash>:page:<cursor>
```

SQLite 仍是第一阶段的数据真相来源。

### 14.2 MySQL

后续可以新增：

```text
PhotoRepository
  - SQLitePhotoRepository
  - MySqlPhotoRepository
```

为了方便迁移：

- 时间统一存整数时间戳。
- 布尔值统一存 `0/1`。
- upsert 逻辑封装在 repository 内。
- 第一版不依赖 SQLite 专属全文搜索。
- 使用 `relative_path` 和可选 `content_hash` 作为稳定标识。

### 14.3 AI 能力

第一版只预留入口。后续可以增加：

- AI 标签。
- 人脸聚类。
- 地点识别。
- 相似照片。
- 重复照片。
- 自然语言搜索。

这些能力应该作为新索引字段和新后台任务接入，不影响第一版基础相册流程。
