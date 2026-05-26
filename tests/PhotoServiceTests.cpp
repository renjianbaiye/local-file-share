#include "PhotoService.h"
#include "SQLitePhotoRepository.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

static void write_file(const fs::path& path, const char* content) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << content;
}

class CountingTagger : public PhotoTagger {
public:
    bool available() const override {
        return true;
    }

    std::vector<PhotoTag> predict(const std::wstring&) const override {
        ++calls;
        PhotoTag tag;
        tag.tag = "test";
        tag.probability = 1.0;
        tag.threshold = 0.5;
        tag.predicted = true;
        tag.derived = false;
        return {tag};
    }

    mutable int calls = 0;
};

class BlockingTagger : public PhotoTagger {
public:
    bool available() const override {
        return true;
    }

    std::vector<PhotoTag> predict(const std::wstring&) const override {
        int call = ++calls;
        if (call == 1) {
            while (!release_first.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }

        PhotoTag tag;
        tag.tag = "queued";
        tag.probability = 1.0;
        tag.threshold = 0.5;
        tag.predicted = true;
        tag.derived = false;
        return {tag};
    }

    mutable std::atomic<int> calls{0};
    std::atomic<bool> release_first{false};
};

static void wait_until_completed(PhotoService& service) {
    for (int i = 0; i < 200; ++i) {
        if (service.latestStatus().status == "completed") {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(false && "scan did not complete");
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

        fs::path tag_root = fs::temp_directory_path() / L"lfs-photo-service-tagger-test-root";
        fs::path tag_db = fs::temp_directory_path() / L"lfs-photo-service-tagger-test.db";
        fs::remove_all(tag_root);
        fs::remove(tag_db);
        fs::remove(tag_db.wstring() + L"-wal");
        fs::remove(tag_db.wstring() + L"-shm");

        write_file(tag_root / L"same.JPG", "image");
        {
            SQLitePhotoRepository repository(tag_db.wstring());
            repository.initialize();
            CountingTagger tagger;
            PhotoService service(tag_root.wstring(), repository, &tagger);

            ScanStatus first = service.scanNow();
            assert(first.status == "completed");
            assert(tagger.calls == 1);

            ScanStatus second = service.scanNow();
            assert(second.status == "completed");
            assert(tagger.calls == 1);
        }

        fs::remove_all(tag_root);
        fs::remove(tag_db);
        fs::remove(tag_db.wstring() + L"-wal");
        fs::remove(tag_db.wstring() + L"-shm");

        fs::path queued_root = fs::temp_directory_path() / L"lfs-photo-service-queued-scan-test-root";
        fs::path queued_db = fs::temp_directory_path() / L"lfs-photo-service-queued-scan-test.db";
        fs::remove_all(queued_root);
        fs::remove(queued_db);
        fs::remove(queued_db.wstring() + L"-wal");
        fs::remove(queued_db.wstring() + L"-shm");

        fs::path queued_image = queued_root / L"same.JPG";
        write_file(queued_image, "first");
        {
            SQLitePhotoRepository repository(queued_db.wstring());
            repository.initialize();
            BlockingTagger tagger;
            PhotoService service(queued_root.wstring(), repository, &tagger);

            ScanStatus first = service.startScanAsync();
            assert(first.status == "scanning");
            for (int i = 0; i < 200 && tagger.calls.load() == 0; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            assert(tagger.calls.load() == 1);
            while (std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::system_clock::now().time_since_epoch()).count() <= first.started_at) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            write_file(queued_image, "second-version");
            fs::last_write_time(queued_image, fs::file_time_type::clock::now() + std::chrono::seconds(2));

            ScanStatus queued = service.startScanAsync();
            assert(queued.status == "scanning");

            tagger.release_first = true;
            wait_until_completed(service);
            assert(service.latestStatus().started_at > first.started_at);
            assert(tagger.calls.load() == 2);
        }

        fs::remove_all(queued_root);
        fs::remove(queued_db);
        fs::remove(queued_db.wstring() + L"-wal");
        fs::remove(queued_db.wstring() + L"-shm");
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "PhotoServiceTests failed: " << ex.what() << "\n";
        return 1;
    }
}
