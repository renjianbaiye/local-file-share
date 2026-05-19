# Local Smart Photo Album Design

## Purpose

LocalFileShare will grow from a LAN file sharing tool into a local smart photo album and photo warehouse. The first version focuses on making the photo framework run smoothly: scan a medium-sized local library, build a persistent metadata index, generate thumbnails, browse by timeline and folder, search basic metadata, and keep uploads flowing into the album.

This version does not try to deliver full AI recognition yet. It creates the product and technical foundation that later features such as AI tags, face grouping, location search, Redis queues, or MySQL storage can extend.

## Current Project Context

The existing application has:

- A C++ backend using `httplib`.
- A Vue frontend under `frontend/`.
- LAN access URL generation and QR code sharing.
- Token-based access control.
- Directory listing, file download, and multi-file upload.
- A file-oriented UI with local search over the current directory.

The album feature should preserve these behaviors and add a parallel "Album" experience instead of replacing the current file browser.

## Goals

- Add an "Files / Album" tab structure in the frontend.
- Support a medium-sized library, roughly 8,000 to 20,000 media files.
- Build a persistent local index for photos, videos, RAW files, and selected metadata.
- Generate and cache thumbnails without blocking the frontend.
- Provide a timeline-first album view with folder filtering.
- Provide a smart-search-shaped UI that initially searches only basic metadata.
- Support favorite and unfavorite as local album metadata.
- Automatically enqueue uploaded photos and videos for indexing.
- Keep the first version easy to run without MySQL or Redis.
- Design storage and runtime boundaries so MySQL and Redis can be added later.

## Non-Goals For The First Version

- Full AI image recognition.
- Face recognition or person clustering.
- RAW image preview generation.
- Advanced video thumbnail extraction.
- Multi-user permission management.
- Deleting, moving, renaming, or batch-organizing source files from the album UI.
- Requiring Redis, MySQL, or any external database service.

## Recommended Architecture

```text
Vue frontend
  - Files view
  - Album view
  - Timeline grid
  - Folder filter
  - Metadata search
  - Scan status and manual refresh controls

C++ HTTP API
  - Existing file APIs
  - Photo listing APIs
  - Photo search APIs
  - Scan task APIs
  - Thumbnail serving APIs
  - Favorite toggle API

PhotoService
  - Directory scan orchestration
  - Metadata extraction
  - Thumbnail job scheduling
  - Upload-to-index integration
  - Folder aggregate updates

Storage layer
  - PhotoRepository interface
  - SQLitePhotoRepository first
  - MySqlPhotoRepository possible later

Runtime layer
  - In-memory queue first
  - Redis queue/cache possible later
```

The first implementation should use SQLite for persistent index data, local disk for thumbnail files, and C++ background workers for scan and thumbnail jobs.

## Storage Strategy

SQLite is the first storage backend because this project is currently an "open the executable and share on LAN" local tool. SQLite keeps setup simple while being more suitable than JSON for pagination, filtering, incremental updates, and favorites.

The backend should still avoid coupling business logic directly to SQLite statements. Use repository-style interfaces so future MySQL support can replace only the storage implementation.

Suggested interfaces:

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

Redis should be treated as a future runtime accelerator, not as the source of truth. It can later replace or augment the in-memory job queue and cache hot timeline/search results.

## SQLite Schema

### `photos`

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

`media_type` values:

- `image`
- `video`
- `raw`
- `other`

`thumbnail_status` values:

- `pending`
- `ready`
- `failed`
- `unsupported`

### `folders`

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

### `scan_runs`

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

`status` values:

- `idle`
- `scanning`
- `completed`
- `failed`
- `cancelled`

### `jobs`

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

`job_type` values:

- `scan`
- `thumbnail`

`status` values:

- `pending`
- `running`
- `done`
- `failed`

## File And Cache Locations

Default local app cache:

```text
%LOCALAPPDATA%\LocalFileShare\photos.db
%LOCALAPPDATA%\LocalFileShare\thumbnails\
```

Planned command-line overrides:

```text
--photo-db <path>
--thumb-dir <path>
```

Keeping the default outside the shared photo directory avoids polluting the user's library. The command-line overrides leave room for portable photo warehouses later.

## Media Support

### Images

JPG, JPEG, PNG, and WebP should be indexed and should receive thumbnails in the first version.

HEIC should be indexed. Thumbnail support depends on the available image library and Windows codecs. If thumbnail generation fails, the item remains usable with a clear placeholder.

### Videos

Videos should be indexed and surfaced in the album. Browser-playable formats such as MP4 and WebM should open in a preview/player experience. First-version thumbnails may use a type placeholder if frame extraction is not available yet.

### RAW

Nikon RAW and other RAW formats should be indexed as `raw`, with file name, size, folder, date metadata where available, and a RAW placeholder. RAW thumbnail or preview generation is a later enhancement.

## Indexing Flow

### Startup

```text
Application starts
  -> Open SQLite database
  -> Apply schema migrations
  -> Start PhotoService background thread
  -> Run lightweight share-directory change check
  -> If changes are likely, enqueue incremental scan
```

The startup path should not block the server from accepting requests. The frontend can show the latest known album data while a scan runs.

### Incremental Scan

```text
Walk share directory
  -> Classify supported media files
  -> Resolve relative path
  -> Compare with existing photos row
  -> Insert new rows
  -> Update changed rows when size or modified_at changed
  -> Mark missing rows when files disappeared
  -> Enqueue thumbnail jobs for new or changed images
  -> Recompute folder aggregates
  -> Update scan_runs progress
```

Full content hashing should not run by default in version one. It is useful for deduplication later, but hashing thousands of large photos, videos, or RAW files would slow the first framework pass.

### Upload Integration

After a successful upload through the existing `/api/upload` endpoint:

```text
Write uploaded file
  -> Return upload success
  -> Enqueue indexing for uploaded path
  -> Enqueue thumbnail job if media type supports thumbnails
```

The upload response should not wait for thumbnail generation.

## Thumbnail Flow

```text
Scan finds new or changed image
  -> Set thumbnail_status = pending
  -> Enqueue thumbnail job
  -> Worker claims job
  -> Generate thumbnail file on disk
  -> Update width, height, thumbnail_path, thumbnail_status
  -> On failure, set thumbnail_status = failed or unsupported
```

Thumbnail URLs should be served through the backend rather than exposing local filesystem paths directly.

## Backend API Design

### Scan APIs

```http
POST /api/photos/scan
```

Starts or requests an incremental scan. If a scan is already running, return the current scan status instead of starting another one.

```http
GET /api/photos/scan/status
```

Returns latest scan state:

```json
{
  "status": "scanning",
  "startedAt": 1779166800,
  "finishedAt": null,
  "totalSeen": 1200,
  "totalIndexed": 380,
  "totalUpdated": 24,
  "totalRemoved": 0,
  "errorMessage": null
}
```

### Timeline API

```http
GET /api/photos/timeline?cursor=<cursor>&limit=100&folder=<path>&media=image,video&favorite=1
```

Returns timeline items sorted by `captured_at` when present, then `modified_at`.

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
      "thumbnailStatus": "ready",
      "thumbnailUrl": "/api/photos/101/thumbnail",
      "downloadUrl": "/download/camera/DSC_0001.JPG",
      "isFavorite": false
    }
  ],
  "nextCursor": "1779166800:101"
}
```

### Search API

```http
GET /api/photos/search?q=<text>&media=image,video,raw&from=<timestamp>&to=<timestamp>&favorite=1&limit=100
```

First-version search should use basic metadata only:

- File name.
- Folder path.
- Media type.
- Date range.
- Favorite state.

The UI can call this "Search" or "Smart Search" visually, but copy should make clear that AI tags and face/location understanding are future capabilities.

### Folder API

```http
GET /api/photos/folders
```

Returns indexed folder aggregates for the album sidebar or filter menu.

### Favorite API

```http
POST /api/photos/:id/favorite
```

Request:

```json
{ "favorite": true }
```

Stores favorite state in SQLite only. It does not modify source files.

### Thumbnail API

```http
GET /api/photos/:id/thumbnail
```

Returns the cached thumbnail if ready. If unsupported or failed, returns a standard placeholder or an HTTP status that the frontend maps to a placeholder.

### Media Open API

The existing `/download/<path>` route can remain the source for full-size downloads. For browser-playable videos, the frontend can open the download URL in a preview player if the MIME type is supported by the browser.

## Frontend Design

### Top-Level Navigation

Add a compact tab control near the existing workspace area:

```text
[ Files ] [ Album ]
```

The current directory browser remains under `Files`. The new album experience lives under `Album`.

### Album Layout

The album view should be an operational tool surface, not a landing page.

Suggested layout:

```text
Album toolbar
  - Search input
  - Media type filter
  - Date filter
  - Favorite filter
  - Refresh index button
  - Scan status indicator

Main content
  - Timeline grouped by day/month
  - Responsive thumbnail grid
  - Cursor pagination or infinite loading

Side/filter area
  - Folder selector
  - Counts by folder
  - Quick filters: Photos, Videos, RAW, Favorites
```

On narrow mobile screens, folder filters should collapse into a drawer or dropdown.

### Timeline Behavior

- Default sort is newest first.
- Group by captured date if available, otherwise modified date.
- Unknown dates fall into an "Unknown Date" group at the end.
- Use lazy loading or cursor pagination to avoid rendering thousands of items at once.
- Show thumbnail placeholders while thumbnail jobs are pending.

### Preview Behavior

Clicking an album item opens a preview overlay:

- Images: show preview or full image via existing download route.
- Videos: show an HTML video player when browser-playable.
- RAW: show metadata and actions with a RAW placeholder.
- All types: show file name, folder, date, size, favorite toggle, download action.

### Favorite Behavior

Favorite toggles update local SQLite state only. They should not create sidecar files and should not modify image metadata.

### Scan Status Behavior

The album toolbar should show:

- Idle.
- Scanning with progress counts.
- Completed timestamp.
- Failed state with a concise error.

The refresh button calls `POST /api/photos/scan`. The frontend can poll `GET /api/photos/scan/status` while scanning.

## Error Handling

- Database open failure: server should continue file sharing if possible and report album unavailable.
- Scan failure: keep previous index data, mark latest scan as failed, show the error in the album toolbar.
- Thumbnail generation failure: show placeholder and keep the item visible.
- Missing source file: mark item as missing during scan and hide it from normal timeline results by default.
- Unsupported media: index it when useful, but use clear placeholders.
- Concurrent scan request: return current scan status instead of starting overlapping scans.

## Performance Notes

- Use cursor pagination for timeline results rather than loading the full library.
- Generate thumbnails in the background.
- Do not compute full content hashes during normal scans.
- Avoid blocking upload responses on indexing or thumbnail work.
- Index `relative_path`, `folder_path`, `media_type`, `captured_at`, `modified_at`, `is_favorite`, and `missing`.
- Keep thumbnail dimensions modest for grid usage.

## Testing Strategy

### Backend

- Path resolution remains confined to the shared root.
- Media classification handles images, videos, RAW, and unsupported files.
- Repository insert/update/list/search/favorite operations work against a temporary SQLite database.
- Incremental scan inserts new files, updates changed files, and marks missing files.
- Scan status prevents overlapping scans.
- Upload flow enqueues index work after writing files.

### Frontend

- Files tab still renders existing directory listing behavior.
- Album tab renders timeline results, folder filters, and placeholders.
- Search and filters call the expected APIs.
- Favorite toggle updates UI state after success.
- Scan status displays idle, scanning, failed, and completed states.
- Mobile layout keeps controls usable and avoids text overlap.

### Manual Verification

- Start backend and frontend in dev mode.
- Open the LAN page locally.
- Upload sample JPG, PNG, MP4, and RAW files.
- Trigger scan.
- Confirm timeline items appear without refreshing the server.
- Confirm thumbnails appear for supported images.
- Confirm videos open in a player when supported.
- Confirm RAW files show placeholders and metadata.
- Confirm favorites persist after reload.

## Migration Path

### Redis Later

Redis can later be introduced behind runtime interfaces:

```text
PhotoJobQueue
  - InMemoryPhotoJobQueue first
  - RedisPhotoJobQueue later

PhotoQueryCache
  - NoOpPhotoQueryCache first
  - RedisPhotoQueryCache later
```

Possible Redis keys:

```text
lfs:scan:status
lfs:scan:queue
lfs:thumb:queue
lfs:timeline:page:<cursor>
lfs:folder:<folderHash>:page:<cursor>
```

SQLite remains the source of truth.

### MySQL Later

MySQL can later be added as another repository implementation:

```text
PhotoRepository
  - SQLitePhotoRepository
  - MySqlPhotoRepository
```

To keep migration practical:

- Store timestamps as integers.
- Store booleans as `0` or `1`.
- Keep upsert logic inside repository implementations.
- Avoid SQLite-only full-text search in the first version.
- Use stable identifiers such as `relative_path` and optional `content_hash`.

## Implementation Planning Decisions

The product scope and API boundaries are fixed by this design. The implementation plan should make the following engineering choices before coding:

- Exact third-party C++ image library for thumbnail and metadata extraction.
- Whether EXIF extraction ships in the first implementation slice or follows immediately after basic indexing.
- Exact cursor encoding format.
- Whether the preview overlay is a new component or part of the album grid component tree.

These decisions must stay within the backend and frontend contracts above.
