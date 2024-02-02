/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

/**
 *
 */
#pragma once

#include <ph/rt-utils.h>

#include <cstdio>
#include <string>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

/// Allows for recording a series of images. Currently only supports
/// outputting them to a folder.
class Recorder {
public:
    Recorder() {
        for (size_t i = 0; i < QUEUE_SIZE; ++i) { _workers[i].init(_queue); }
    }

    ~Recorder() {
        // send quit signals to all worker threads.
        for (size_t i = 0; i < QUEUE_SIZE; ++i) { _queue.push(nullptr); }
    }

    /// Writes a frame to the recording.
    void write(ph::RawImage && image, uint64_t frameIndex) {
        // tell worker thread to save the image.
        push(std::move(image), toFilePath(frameIndex));
    }

    const std::string & outputPath() const { return _outputPath; }

    /// @param outputPath How you want the images saved. Treats the argument
    /// as a formatted string accepting the frame number as an unsigned 64 bit parameter.
    /// For example, "/home/me/Pictures/some_album/%02llu.png" would write
    /// ["/home/me/Pictures/some_album/00.png", "/home/me/Pictures/some_album/01.png", etc]
    void setOutputPath(const std::string & outputPath) {
        // Path output path will generate when given frame zero.
        std::string zeroPath = ph::formatstr(outputPath.c_str(), 0);

        // Path output path will generate when given max frame
        std::string maxPath = ph::formatstr(outputPath.c_str(), std::numeric_limits<uint64_t>::max());

        // Make sure format string accepts parameters.
        // If both paths are the same, this means that it is probably missing the frame argument.
        if (zeroPath == maxPath) { PH_THROW("Recorder output path \"%s\" missing frame number parameter (e.g. \"%%llu\").", outputPath.c_str()); }

        // Save the file path.
        _outputPath = outputPath;
    }

private:
    struct WorkItem {
        ph::RawImage image;
        std::string  path;

        PH_NO_COPY(WorkItem);

        WorkItem(ph::RawImage && i, std::string && p): image(std::move(i)), path(std::move(p)) {}

        WorkItem(WorkItem && rhs) {
            image = std::move(rhs.image);
            path  = std::move(rhs.path);
        }

        WorkItem & operator=(WorkItem && rhs) {
            if (this == &rhs) return *this;
            image = std::move(rhs.image);
            path  = std::move(rhs.path);
            return *this;
        }
    };

    struct JobQueue {
        const size_t            MAX_PENDING;
        std::queue<WorkItem *>  pending;
        std::mutex              mu;
        std::condition_variable cv;

        JobQueue(size_t N): MAX_PENDING(N) {}

        void push(WorkItem * p) {
            std::unique_lock<std::mutex> lock(mu);
            cv.wait(lock, [this] { return pending.size() < MAX_PENDING; });
            pending.push(p);
            cv.notify_all();
        }

        WorkItem * pop() {
            std::unique_lock<std::mutex> lock(mu);
            cv.wait(lock, [this] { return !pending.empty(); });
            auto w = pending.front();
            pending.pop();
            cv.notify_one();
            return w;
        }
    };

    struct WorkerThread {
        std::thread th;
        JobQueue *  q  = nullptr;
        WorkerThread() = default;
        ~WorkerThread() {
            if (th.joinable()) th.join();
        }

        void init(JobQueue & q) {
            this->q = &q;
            th      = std::move(std::thread([this] { proc(); }));
        }

        void proc() {
            for (;;) {
                WorkItem * w = q->pop();
                if (!w) return;
                resetAlpha(w->image);
                const auto & desc = w->image.desc().plane();
                desc.save(w->path, w->image.data());
                PH_LOGI("frame saved as: %s", w->path.c_str());

                delete w;
            }
        }

        void resetAlpha(const ph::RawImage & img) {
            if (img.desc().plane().format.layout == ph::ColorFormat::LAYOUT_8_8_8_8) {
                auto w = img.width();
                auto h = img.height();
                for (uint32_t x = 0; x < w; x++) {
                    for (uint32_t y = 0; y < h; y++) {
                        uint8_t * refPx = (uint8_t *) img.proxy().pixel(0, 0, x, y);
                        refPx[3]        = 255;
                    }
                }
            }
        }
    };

    inline static constexpr size_t QUEUE_SIZE = 8;

    JobQueue     _queue {QUEUE_SIZE};
    WorkerThread _workers[QUEUE_SIZE];

    /// Place to where we are saving the images.
    std::string _outputPath;

    void push(ph::RawImage && image, std::string && path) { _queue.push(new WorkItem {std::move(image), std::move(path)}); }

    std::string toFilePath(uint64_t frameCount) { return ph::formatstr(_outputPath.c_str(), frameCount); }
};
