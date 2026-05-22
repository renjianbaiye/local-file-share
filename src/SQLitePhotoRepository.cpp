#include "SQLitePhotoRepository.h"

#include "sqlite3.h"

#include <sstream>
#include <stdexcept>

namespace {

class Statement {
public:
    Statement(sqlite3* db, const char* sql) : db_(db) {
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt_, nullptr) != SQLITE_OK) {
            throw std::runtime_error(sqlite3_errmsg(db_));
        }
    }

    ~Statement() {
        sqlite3_finalize(stmt_);
    }

    sqlite3_stmt* get() const {
        return stmt_;
    }

private:
    sqlite3* db_;
    sqlite3_stmt* stmt_ = nullptr;
};

void bindText(sqlite3_stmt* stmt, int index, const std::string& value) {
    sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT);
}

void bindOptionalInt64(sqlite3_stmt* stmt, int index, const std::optional<int64_t>& value) {
    if (value.has_value()) {
        sqlite3_bind_int64(stmt, index, *value);
    } else {
        sqlite3_bind_null(stmt, index);
    }
}

void bindOptionalInt(sqlite3_stmt* stmt, int index, const std::optional<int>& value) {
    if (value.has_value()) {
        sqlite3_bind_int(stmt, index, *value);
    } else {
        sqlite3_bind_null(stmt, index);
    }
}

std::optional<int64_t> optionalInt64(sqlite3_stmt* stmt, int column) {
    if (sqlite3_column_type(stmt, column) == SQLITE_NULL) {
        return std::nullopt;
    }
    return sqlite3_column_int64(stmt, column);
}

std::optional<int> optionalInt(sqlite3_stmt* stmt, int column) {
    if (sqlite3_column_type(stmt, column) == SQLITE_NULL) {
        return std::nullopt;
    }
    return sqlite3_column_int(stmt, column);
}

std::string columnText(sqlite3_stmt* stmt, int column) {
    const unsigned char* text = sqlite3_column_text(stmt, column);
    return text == nullptr ? std::string() : reinterpret_cast<const char*>(text);
}

std::optional<int64_t> parseCursorTime(const std::string& cursor) {
    if (cursor.empty()) {
        return std::nullopt;
    }
    size_t colon = cursor.find(':');
    std::string value = colon == std::string::npos ? cursor : cursor.substr(0, colon);
    try {
        return std::stoll(value);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<int64_t> parseCursorId(const std::string& cursor) {
    size_t colon = cursor.find(':');
    if (colon == std::string::npos || colon + 1 >= cursor.size()) {
        return std::nullopt;
    }
    try {
        return std::stoll(cursor.substr(colon + 1));
    } catch (...) {
        return std::nullopt;
    }
}

PhotoRecord readPhoto(sqlite3_stmt* stmt) {
    PhotoRecord photo;
    photo.id = sqlite3_column_int64(stmt, 0);
    photo.relative_path = columnText(stmt, 1);
    photo.absolute_path_hash = columnText(stmt, 2);
    photo.file_name = columnText(stmt, 3);
    photo.folder_path = columnText(stmt, 4);
    photo.extension = columnText(stmt, 5);
    photo.media_type = columnText(stmt, 6);
    photo.mime_type = columnText(stmt, 7);
    photo.size_bytes = sqlite3_column_int64(stmt, 8);
    photo.modified_at = sqlite3_column_int64(stmt, 9);
    photo.captured_at = optionalInt64(stmt, 10);
    photo.width = optionalInt(stmt, 11);
    photo.height = optionalInt(stmt, 12);
    photo.orientation = optionalInt(stmt, 13);
    photo.content_hash = columnText(stmt, 14);
    photo.thumbnail_status = columnText(stmt, 15);
    photo.thumbnail_path = columnText(stmt, 16);
    photo.is_favorite = sqlite3_column_int(stmt, 17) != 0;
    photo.indexed_at = sqlite3_column_int64(stmt, 18);
    photo.missing = sqlite3_column_int(stmt, 19) != 0;
    return photo;
}

const char* photoColumns() {
    return "id, relative_path, absolute_path_hash, file_name, folder_path, extension, "
           "media_type, mime_type, size_bytes, modified_at, captured_at, width, height, "
           "orientation, content_hash, thumbnail_status, thumbnail_path, is_favorite, "
           "indexed_at, missing";
}

void stepDone(sqlite3* db, sqlite3_stmt* stmt) {
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
}

} // namespace

SQLitePhotoRepository::SQLitePhotoRepository(std::wstring db_path)
    : db_path_(std::move(db_path)) {}

SQLitePhotoRepository::~SQLitePhotoRepository() {
    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

void SQLitePhotoRepository::open() const {
    if (db_ != nullptr) {
        return;
    }
    if (sqlite3_open16(db_path_.c_str(), &db_) != SQLITE_OK) {
        std::string message = db_ == nullptr ? "Failed to open SQLite database" : sqlite3_errmsg(db_);
        throw std::runtime_error(message);
    }
}

void SQLitePhotoRepository::exec(const char* sql) const {
    open();
    char* error = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &error) != SQLITE_OK) {
        std::string message = error == nullptr ? sqlite3_errmsg(db_) : error;
        sqlite3_free(error);
        throw std::runtime_error(message);
    }
}

void SQLitePhotoRepository::initialize() {
    open();
    exec("PRAGMA foreign_keys = ON;");
    exec("PRAGMA journal_mode = WAL;");
    exec("BEGIN;");
    try {
        exec(
            "CREATE TABLE IF NOT EXISTS photos ("
            "id INTEGER PRIMARY KEY,"
            "relative_path TEXT NOT NULL UNIQUE,"
            "absolute_path_hash TEXT NOT NULL,"
            "file_name TEXT NOT NULL,"
            "folder_path TEXT NOT NULL,"
            "extension TEXT NOT NULL,"
            "media_type TEXT NOT NULL,"
            "mime_type TEXT,"
            "size_bytes INTEGER NOT NULL,"
            "modified_at INTEGER NOT NULL,"
            "captured_at INTEGER,"
            "width INTEGER,"
            "height INTEGER,"
            "orientation INTEGER,"
            "content_hash TEXT,"
            "thumbnail_status TEXT NOT NULL,"
            "thumbnail_path TEXT,"
            "is_favorite INTEGER NOT NULL DEFAULT 0,"
            "indexed_at INTEGER NOT NULL,"
            "missing INTEGER NOT NULL DEFAULT 0"
            ");"
            "CREATE TABLE IF NOT EXISTS folders ("
            "id INTEGER PRIMARY KEY,"
            "relative_path TEXT NOT NULL UNIQUE,"
            "photo_count INTEGER NOT NULL DEFAULT 0,"
            "video_count INTEGER NOT NULL DEFAULT 0,"
            "raw_count INTEGER NOT NULL DEFAULT 0,"
            "latest_captured_at INTEGER,"
            "latest_modified_at INTEGER,"
            "indexed_at INTEGER NOT NULL"
            ");"
            "CREATE TABLE IF NOT EXISTS scan_runs ("
            "id INTEGER PRIMARY KEY,"
            "status TEXT NOT NULL,"
            "started_at INTEGER NOT NULL,"
            "finished_at INTEGER,"
            "total_seen INTEGER NOT NULL DEFAULT 0,"
            "total_indexed INTEGER NOT NULL DEFAULT 0,"
            "total_updated INTEGER NOT NULL DEFAULT 0,"
            "total_removed INTEGER NOT NULL DEFAULT 0,"
            "error_message TEXT"
            ");"
            "CREATE TABLE IF NOT EXISTS jobs ("
            "id INTEGER PRIMARY KEY,"
            "job_type TEXT NOT NULL,"
            "target_path TEXT NOT NULL,"
            "status TEXT NOT NULL,"
            "attempts INTEGER NOT NULL DEFAULT 0,"
            "last_error TEXT,"
            "created_at INTEGER NOT NULL,"
            "updated_at INTEGER NOT NULL"
            ");"
            "CREATE TABLE IF NOT EXISTS photo_tags ("
            "photo_id INTEGER NOT NULL,"
            "tag TEXT NOT NULL,"
            "probability REAL NOT NULL,"
            "threshold_value REAL NOT NULL,"
            "predicted INTEGER NOT NULL,"
            "derived INTEGER NOT NULL,"
            "PRIMARY KEY(photo_id, tag),"
            "FOREIGN KEY(photo_id) REFERENCES photos(id) ON DELETE CASCADE"
            ");"
            "CREATE INDEX IF NOT EXISTS idx_photos_folder_path ON photos(folder_path);"
            "CREATE INDEX IF NOT EXISTS idx_photos_media_type ON photos(media_type);"
            "CREATE INDEX IF NOT EXISTS idx_photos_captured_at ON photos(captured_at DESC, id DESC);"
            "CREATE INDEX IF NOT EXISTS idx_photos_modified_at ON photos(modified_at DESC, id DESC);"
            "CREATE INDEX IF NOT EXISTS idx_photos_favorite ON photos(is_favorite);"
            "CREATE INDEX IF NOT EXISTS idx_photos_missing ON photos(missing);"
            "CREATE INDEX IF NOT EXISTS idx_jobs_status_type ON jobs(status, job_type);"
            "CREATE INDEX IF NOT EXISTS idx_photo_tags_tag ON photo_tags(tag);"
            "PRAGMA user_version = 2;"
        );
        exec("COMMIT;");
    } catch (...) {
        exec("ROLLBACK;");
        throw;
    }
}

int SQLitePhotoRepository::schemaVersion() const {
    open();
    Statement stmt(db_, "PRAGMA user_version;");
    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        throw std::runtime_error(sqlite3_errmsg(db_));
    }
    return sqlite3_column_int(stmt.get(), 0);
}

bool SQLitePhotoRepository::hasTable(const std::string& table_name) const {
    open();
    Statement stmt(db_, "SELECT 1 FROM sqlite_schema WHERE type = 'table' AND name = ? LIMIT 1;");
    bindText(stmt.get(), 1, table_name);
    return sqlite3_step(stmt.get()) == SQLITE_ROW;
}

void SQLitePhotoRepository::upsertPhoto(const PhotoRecord& photo) {
    open();
    Statement stmt(db_,
        "INSERT INTO photos (relative_path, absolute_path_hash, file_name, folder_path, extension, "
        "media_type, mime_type, size_bytes, modified_at, captured_at, width, height, orientation, "
        "content_hash, thumbnail_status, thumbnail_path, is_favorite, indexed_at, missing) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(relative_path) DO UPDATE SET "
        "absolute_path_hash = excluded.absolute_path_hash,"
        "file_name = excluded.file_name,"
        "folder_path = excluded.folder_path,"
        "extension = excluded.extension,"
        "media_type = excluded.media_type,"
        "mime_type = excluded.mime_type,"
        "size_bytes = excluded.size_bytes,"
        "modified_at = excluded.modified_at,"
        "captured_at = excluded.captured_at,"
        "width = excluded.width,"
        "height = excluded.height,"
        "orientation = excluded.orientation,"
        "content_hash = excluded.content_hash,"
        "thumbnail_status = excluded.thumbnail_status,"
        "thumbnail_path = excluded.thumbnail_path,"
        "indexed_at = excluded.indexed_at,"
        "missing = 0;");

    bindText(stmt.get(), 1, photo.relative_path);
    bindText(stmt.get(), 2, photo.absolute_path_hash);
    bindText(stmt.get(), 3, photo.file_name);
    bindText(stmt.get(), 4, photo.folder_path);
    bindText(stmt.get(), 5, photo.extension);
    bindText(stmt.get(), 6, photo.media_type);
    bindText(stmt.get(), 7, photo.mime_type);
    sqlite3_bind_int64(stmt.get(), 8, photo.size_bytes);
    sqlite3_bind_int64(stmt.get(), 9, photo.modified_at);
    bindOptionalInt64(stmt.get(), 10, photo.captured_at);
    bindOptionalInt(stmt.get(), 11, photo.width);
    bindOptionalInt(stmt.get(), 12, photo.height);
    bindOptionalInt(stmt.get(), 13, photo.orientation);
    bindText(stmt.get(), 14, photo.content_hash);
    bindText(stmt.get(), 15, photo.thumbnail_status);
    bindText(stmt.get(), 16, photo.thumbnail_path);
    sqlite3_bind_int(stmt.get(), 17, photo.is_favorite ? 1 : 0);
    sqlite3_bind_int64(stmt.get(), 18, photo.indexed_at);
    sqlite3_bind_int(stmt.get(), 19, photo.missing ? 1 : 0);

    stepDone(db_, stmt.get());
    refreshFolderStats(photo.folder_path);
}

PhotoRecord SQLitePhotoRepository::getPhoto(int64_t id) const {
    open();
    std::string sql = std::string("SELECT ") + photoColumns() + " FROM photos WHERE id = ?;";
    Statement stmt(db_, sql.c_str());
    sqlite3_bind_int64(stmt.get(), 1, id);
    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        throw std::runtime_error("Photo not found");
    }
    return readPhoto(stmt.get());
}

PhotoRecord SQLitePhotoRepository::getPhotoByRelativePath(const std::string& relative_path) const {
    open();
    std::string sql = std::string("SELECT ") + photoColumns() + " FROM photos WHERE relative_path = ?;";
    Statement stmt(db_, sql.c_str());
    bindText(stmt.get(), 1, relative_path);
    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        throw std::runtime_error("Photo not found");
    }
    return readPhoto(stmt.get());
}

std::vector<PhotoRecord> SQLitePhotoRepository::listTimeline(const TimelineQuery& query) const {
    open();
    int limit = query.limit <= 0 ? 100 : query.limit;
    if (limit > 500) {
        limit = 500;
    }
    std::optional<int64_t> cursor_time = parseCursorTime(query.cursor);
    std::optional<int64_t> cursor_id = parseCursorId(query.cursor);

    std::string sql = std::string("SELECT ") + photoColumns() + " FROM photos WHERE 1 = 1";
    if (!query.folder_path.empty()) {
        sql += " AND folder_path = ?";
    }
    if (query.favorite.has_value()) {
        sql += " AND is_favorite = ?";
    }
    if (query.missing.has_value()) {
        sql += " AND missing = ?";
    }
    if (!query.media_types.empty()) {
        sql += " AND media_type IN (";
        for (size_t i = 0; i < query.media_types.size(); ++i) {
            sql += i == 0 ? "?" : ", ?";
        }
        sql += ")";
    }
    if (cursor_time.has_value() && cursor_id.has_value()) {
        sql += " AND (COALESCE(captured_at, modified_at) < ? OR "
               "(COALESCE(captured_at, modified_at) = ? AND id < ?))";
    }
    sql += " ORDER BY COALESCE(captured_at, modified_at) DESC, id DESC LIMIT ?;";

    Statement stmt(db_, sql.c_str());
    int index = 1;
    if (!query.folder_path.empty()) {
        bindText(stmt.get(), index++, query.folder_path);
    }
    if (query.favorite.has_value()) {
        sqlite3_bind_int(stmt.get(), index++, *query.favorite ? 1 : 0);
    }
    if (query.missing.has_value()) {
        sqlite3_bind_int(stmt.get(), index++, *query.missing ? 1 : 0);
    }
    for (const std::string& media_type : query.media_types) {
        bindText(stmt.get(), index++, media_type);
    }
    if (cursor_time.has_value() && cursor_id.has_value()) {
        sqlite3_bind_int64(stmt.get(), index++, *cursor_time);
        sqlite3_bind_int64(stmt.get(), index++, *cursor_time);
        sqlite3_bind_int64(stmt.get(), index++, *cursor_id);
    }
    sqlite3_bind_int(stmt.get(), index, limit);

    std::vector<PhotoRecord> photos;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        photos.push_back(readPhoto(stmt.get()));
    }
    return photos;
}

std::vector<FolderRecord> SQLitePhotoRepository::listFolders() const {
    open();
    Statement stmt(db_,
        "SELECT relative_path, photo_count, video_count, raw_count, latest_captured_at, "
        "latest_modified_at, indexed_at FROM folders "
        "WHERE photo_count > 0 OR video_count > 0 OR raw_count > 0 "
        "ORDER BY relative_path ASC;");

    std::vector<FolderRecord> folders;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        FolderRecord folder;
        folder.relative_path = columnText(stmt.get(), 0);
        folder.photo_count = sqlite3_column_int64(stmt.get(), 1);
        folder.video_count = sqlite3_column_int64(stmt.get(), 2);
        folder.raw_count = sqlite3_column_int64(stmt.get(), 3);
        folder.latest_captured_at = optionalInt64(stmt.get(), 4);
        folder.latest_modified_at = optionalInt64(stmt.get(), 5);
        folder.indexed_at = sqlite3_column_int64(stmt.get(), 6);
        folders.push_back(folder);
    }
    return folders;
}

std::vector<std::string> SQLitePhotoRepository::listIndexedRelativePaths() const {
    open();
    Statement stmt(db_, "SELECT relative_path FROM photos WHERE missing = 0;");
    std::vector<std::string> paths;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        paths.push_back(columnText(stmt.get(), 0));
    }
    return paths;
}

void SQLitePhotoRepository::replacePhotoTags(const std::string& relative_path, const std::vector<PhotoTag>& tags) {
    PhotoRecord photo = getPhotoByRelativePath(relative_path);
    exec("BEGIN;");
    try {
        {
            Statement stmt(db_, "DELETE FROM photo_tags WHERE photo_id = ?;");
            sqlite3_bind_int64(stmt.get(), 1, photo.id);
            stepDone(db_, stmt.get());
        }

        for (const PhotoTag& tag : tags) {
            Statement stmt(db_,
                "INSERT INTO photo_tags (photo_id, tag, probability, threshold_value, predicted, derived) "
                "VALUES (?, ?, ?, ?, ?, ?);");
            sqlite3_bind_int64(stmt.get(), 1, photo.id);
            bindText(stmt.get(), 2, tag.tag);
            sqlite3_bind_double(stmt.get(), 3, tag.probability);
            sqlite3_bind_double(stmt.get(), 4, tag.threshold);
            sqlite3_bind_int(stmt.get(), 5, tag.predicted ? 1 : 0);
            sqlite3_bind_int(stmt.get(), 6, tag.derived ? 1 : 0);
            stepDone(db_, stmt.get());
        }
        exec("COMMIT;");
    } catch (...) {
        exec("ROLLBACK;");
        throw;
    }
}

std::vector<PhotoTag> SQLitePhotoRepository::listPhotoTags(int64_t photo_id) const {
    open();
    Statement stmt(db_,
        "SELECT tag, probability, threshold_value, predicted, derived "
        "FROM photo_tags WHERE photo_id = ? "
        "ORDER BY predicted DESC, derived DESC, probability DESC, tag ASC;");
    sqlite3_bind_int64(stmt.get(), 1, photo_id);

    std::vector<PhotoTag> tags;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        PhotoTag tag;
        tag.tag = columnText(stmt.get(), 0);
        tag.probability = sqlite3_column_double(stmt.get(), 1);
        tag.threshold = sqlite3_column_double(stmt.get(), 2);
        tag.predicted = sqlite3_column_int(stmt.get(), 3) != 0;
        tag.derived = sqlite3_column_int(stmt.get(), 4) != 0;
        tags.push_back(tag);
    }
    return tags;
}

void SQLitePhotoRepository::markMissing(const std::string& relative_path) {
    PhotoRecord photo = getPhotoByRelativePath(relative_path);
    Statement stmt(db_, "UPDATE photos SET missing = 1 WHERE relative_path = ?;");
    bindText(stmt.get(), 1, relative_path);
    stepDone(db_, stmt.get());
    refreshFolderStats(photo.folder_path);
}

void SQLitePhotoRepository::toggleFavorite(int64_t id, bool favorite) {
    open();
    Statement stmt(db_, "UPDATE photos SET is_favorite = ? WHERE id = ?;");
    sqlite3_bind_int(stmt.get(), 1, favorite ? 1 : 0);
    sqlite3_bind_int64(stmt.get(), 2, id);
    stepDone(db_, stmt.get());
}

void SQLitePhotoRepository::deletePhoto(int64_t id) {
    open();
    // photo_tags are deleted automatically via ON DELETE CASCADE
    Statement stmt(db_, "DELETE FROM photos WHERE id = ?;");
    sqlite3_bind_int64(stmt.get(), 1, id);
    stepDone(db_, stmt.get());
}

void SQLitePhotoRepository::refreshFolderStats(const std::string& folder_path) const {
    Statement stmt(db_,
        "INSERT INTO folders (relative_path, photo_count, video_count, raw_count, "
        "latest_captured_at, latest_modified_at, indexed_at) "
        "SELECT ?,"
        "SUM(CASE WHEN media_type = 'image' AND missing = 0 THEN 1 ELSE 0 END),"
        "SUM(CASE WHEN media_type = 'video' AND missing = 0 THEN 1 ELSE 0 END),"
        "SUM(CASE WHEN media_type = 'raw' AND missing = 0 THEN 1 ELSE 0 END),"
        "MAX(captured_at),"
        "MAX(modified_at),"
        "MAX(indexed_at) "
        "FROM photos WHERE folder_path = ? "
        "ON CONFLICT(relative_path) DO UPDATE SET "
        "photo_count = excluded.photo_count,"
        "video_count = excluded.video_count,"
        "raw_count = excluded.raw_count,"
        "latest_captured_at = excluded.latest_captured_at,"
        "latest_modified_at = excluded.latest_modified_at,"
        "indexed_at = excluded.indexed_at;");
    bindText(stmt.get(), 1, folder_path);
    bindText(stmt.get(), 2, folder_path);
    stepDone(db_, stmt.get());
}
