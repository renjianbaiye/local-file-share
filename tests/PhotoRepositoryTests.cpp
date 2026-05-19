#include "SQLitePhotoRepository.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

static std::wstring temp_db_path() {
    std::filesystem::path path = std::filesystem::temp_directory_path();
    path /= L"local-file-share-photo-repository-test.db";
    std::filesystem::remove(path);
    return path.wstring();
}

int main() {
    try {
        std::wstring db_path = temp_db_path();
        std::filesystem::path cleanup_path(db_path);

        {
            SQLitePhotoRepository repository(db_path);
            repository.initialize();

            assert(repository.schemaVersion() == 1);
            assert(repository.hasTable("photos"));
            assert(repository.hasTable("folders"));
            assert(repository.hasTable("scan_runs"));
            assert(repository.hasTable("jobs"));

            PhotoRecord photo;
            photo.relative_path = "camera/DSC_0001.JPG";
            photo.absolute_path_hash = "root-hash";
            photo.file_name = "DSC_0001.JPG";
            photo.folder_path = "camera";
            photo.extension = ".jpg";
            photo.media_type = "image";
            photo.mime_type = "image/jpeg";
            photo.size_bytes = 7340032;
            photo.modified_at = 1779166810;
            photo.captured_at = 1779166800;
            photo.width = 6000;
            photo.height = 4000;
            photo.orientation = 1;
            photo.thumbnail_status = "pending";
            photo.indexed_at = 1779166820;

            repository.upsertPhoto(photo);

            PhotoRecord stored = repository.getPhotoByRelativePath("camera/DSC_0001.JPG");
            assert(stored.file_name == "DSC_0001.JPG");
            assert(stored.folder_path == "camera");
            assert(stored.media_type == "image");
            assert(stored.missing == false);

            repository.toggleFavorite(stored.id, true);
            stored = repository.getPhoto(stored.id);
            assert(stored.is_favorite == true);

            repository.markMissing("camera/DSC_0001.JPG");
            stored = repository.getPhoto(stored.id);
            assert(stored.missing == true);
        }

        std::filesystem::remove(cleanup_path);
        std::filesystem::remove(cleanup_path.wstring() + L"-wal");
        std::filesystem::remove(cleanup_path.wstring() + L"-shm");
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "PhotoRepositoryTests failed: " << ex.what() << "\n";
        return 1;
    }
}
