#include "PhotoService.h"
#include "SQLitePhotoRepository.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

static void write_file(const fs::path& path, const char* content) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << content;
}

int main() {
    try {
        fs::path root = fs::temp_directory_path() / L"lfs-photo-service-test-root";
        fs::path db = fs::temp_directory_path() / L"lfs-photo-service-test.db";
        fs::remove_all(root);
        fs::remove(db);
        fs::remove(db.wstring() + L"-wal");
        fs::remove(db.wstring() + L"-shm");

        write_file(root / L"camera" / L"DSC_0001.JPG", "image");
        write_file(root / L"camera" / L"clip.MP4", "video");
        write_file(root / L"raw" / L"capture.NEF", "raw");
        write_file(root / L"notes.txt", "skip");

        {
            SQLitePhotoRepository repository(db.wstring());
            repository.initialize();
            PhotoService service(root.wstring(), repository);

            ScanStatus first = service.scanNow();
            assert(first.status == "completed");
            assert(first.total_seen == 3);
            assert(first.total_indexed == 3);

            TimelineQuery query;
            query.limit = 10;
            query.missing = false;
            auto photos = repository.listTimeline(query);
            assert(photos.size() == 3);

            PhotoRecord jpg = repository.getPhotoByRelativePath("camera/DSC_0001.JPG");
            assert(jpg.media_type == "image");
            assert(jpg.thumbnail_status == "ready");
            assert(jpg.folder_path == "camera");

            PhotoRecord mp4 = repository.getPhotoByRelativePath("camera/clip.MP4");
            assert(mp4.media_type == "video");
            assert(mp4.thumbnail_status == "unsupported");

            PhotoRecord raw = repository.getPhotoByRelativePath("raw/capture.NEF");
            assert(raw.media_type == "raw");
            assert(raw.thumbnail_status == "unsupported");

            fs::remove(root / L"camera" / L"clip.MP4");
            ScanStatus second = service.scanNow();
            assert(second.status == "completed");
            assert(second.total_removed == 1);
            assert(repository.getPhotoByRelativePath("camera/clip.MP4").missing);
        }

        fs::remove_all(root);
        fs::remove(db);
        fs::remove(db.wstring() + L"-wal");
        fs::remove(db.wstring() + L"-shm");
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "PhotoServiceTests failed: " << ex.what() << "\n";
        return 1;
    }
}
