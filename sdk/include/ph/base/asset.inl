#pragma once

#include <future>
#include <variant>

struct AAssetManager;

namespace ph {

class AssetSystem;

/// This class represents a asset loaded by the asset system.
struct Asset {
    /// Asset content type
    struct Content {
        PH_NO_COPY(Content);
        PH_DEFAULT_MOVE(Content);

        /// If the asset is an valid image. This member contains an non-empty image. In this case, 'v' is empty.
        RawImage i;

        /// If the asset is _NOT_ an image. This member contains the raw content of the asset.
        std::vector<uint8_t> v;

        Content() = default;

        const uint8_t * data() const { return i.empty() ? v.data() : i.data(); }

        uint8_t * data() { return i.empty() ? v.data() : i.data(); }

        bool empty() const { return i.empty() && v.empty(); }

        bool emptyImage() const { return i.empty(); }

        size_t size() const { return i.empty() ? v.size() : i.size(); }

        const uint8_t & operator[](size_t i) const { return data()[i]; }

        uint8_t & operator[](size_t i) { return data()[i]; }
    };

    /// Asset name that can be used to uniquely identify the asset within the asset system.
    std::string name;

    /// The asset content.
    Content content;

    /// The modification time stamp when the asset was loaded.
    uint64_t timestamp = 0;

    /// Default constructor
    Asset() { PH_ASSERT(empty()); }

    /// no copy can move
    PH_NO_COPY(Asset);

    /// move constructor
    Asset(Asset && rhs) { moveFrom(rhs); }

    /// move operator
    Asset & operator=(Asset && rhs) {
        moveFrom(rhs);
        return *this;
    }

    /// Check if the asset is empty. An successufly loaded asset is never empty.
    bool empty() const { return content.empty(); }

    /// Check if the image associated with the asset is empty. This is to verify that the asset represents image data.
    bool emptyImage() const { return content.emptyImage(); }

    /// cast to bool
    operator bool() const { return !empty(); }

private:
    void moveFrom(Asset & rhs) {
        if (this == &rhs) return;
        name          = std::move(rhs.name);
        content       = std::move(rhs.content);
        timestamp     = rhs.timestamp;
        rhs.timestamp = 0;
    }
};

/// The cross-platfom asset manager responsible for loading asset through a virtual asset file system.
/// The asset manager maintains an internal virtual file system that could map to actual file system, or
/// archive file (like a zip file), or both. It works similarly as Linux overlay file system.
class AssetSystem {

public:
    /// (Optional) Pointer to android asset manager. If provided, then the asset system will also be able
    /// to load assets packed in the app's apk.
    /// Note that this function is not multithread protected. So it is suggested to set this pointer only
    /// once when application start, and never change it again.
    static void setAndroidAssetManager(AAssetManager * aam);

    /// Asset system creation parameters
    struct CreateParameters {
        /// List of root folders.
        ///
        /// The asset system is able to combine multiple folders into one virtual file system in the similar way
        /// as how Linux overlay file system works. For example, you have a file system like this on your hard drive:
        ///
        ///     /a/
        ///       1.txt
        ///       2.txt
        ///       c/
        ///         3.txt
        ///         4.txt
        ///         a.txt
        ///     /b/
        ///       5.txt
        ///       6.txt
        ///       c/
        ///         7.txt
        ///         8.txt
        ///         a.txt
        ///
        /// And by adding both directory "/a/" and "/b/" to roots, you'll effectively create a virtual file system
        /// like this:
        ///
        ///     <root>/
        ///       1.txt
        ///       2.txt
        ///       5.txt
        ///       6.txt
        ///       c/
        ///         3.txt
        ///         4.txt
        ///         7.txt
        ///         8.txt
        ///         a.txt (pointing to /a/c/a.txt)
        ///
        std::vector<std::string> roots;

        /// size of the internal cache, in MBytes, to load recently used/loaded assets.
        /// Default is 32MB.
        uint64_t memoryBudgetInMB = 32;
    };

    /// Construct a new asset system. returns null, if anything goes wrong.
    static AssetSystem * PH_API create(const CreateParameters &);

    /// Desruct the asset system. This will auto delete/invalidate all loaded asset pointers.
    virtual ~AssetSystem() = default;

    /// check if an particular asset exists or not
    virtual bool exist(const char * name) = 0;

    /// search asset system to return a list of assets that matches certain search criteria.
    virtual std::vector<std::string> grep(const char * folder, const char * pattern, bool recursive) = 0;

    /// Preload a list of assets.
    ///
    /// The asset system should try load as many items specified in the list as possible and skip ones that
    /// couldn't loaded properly (like non-exist resoruce, or permission denied)
    ///
    /// This function needs to be completely thread safe that users are allowed to call preload() again, before the
    /// previous preloading completes. Or call this function simultenously from different threads.
    ///
    /// \return a future object that caller could wait on completion of the preloading.
    virtual std::future<void> preload(const char * const * names, size_t count) = 0;

    /// Preload a list of assets.
    virtual std::future<void> preload(std::vector<std::string> && names) = 0;

    /// Preload all assets within a folder
    virtual std::future<void> preloadFolder(const char * folder) = 0;

    /// Load asset.
    ///
    /// The asset system needs to maintain an internal cache for recently loaded assets. So if loading function is
    /// called repeatedly on same asset, the asset system need to return the cached item instead of always loading it
    /// from external storage.
    ///
    /// The asset system could preload resources in batch from worker thread(s) to speed things up.
    ///
    /// Also, since waiting for preload() is optional, user might call preload() for a bunch of assets, then call load()
    /// immediately on one of them. In this case, the asset manager should prioritize this load() function and return
    /// this asset as early as possible, instead of having this call blocked and wait for all preloading to finish.
    ///
    /// The method is thread safe.
    virtual std::future<Asset> load(const char * name) = 0;

    /// convert asset path to native path, if possible. Return empty string if the asset doesn't map to any
    /// individual native file.
    virtual std::string getNativePath(const char * name) = 0;

    /// Get last-modified timestamp of an asset. Returns 0, if the asset name is invalid.
    virtual uint64_t queryLastModifiedTimestamp(const char * name) = 0;
};

} // namespace ph
