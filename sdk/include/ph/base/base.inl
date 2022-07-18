#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <errno.h>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// Disable some known "harmless" warnings. So we can use /W4 throughout our code base.
#ifdef _MSC_VER
    #pragma warning(disable : 4201) // nameless struct/union
    #pragma warning(disable : 4458) // declaration hides class member
#endif

/// determine operating system
//@{
#ifdef _WIN32
    #define PH_MSWIN 1
#else
    #define PH_MSWIN 0
#endif

#ifdef __APPLE__
    #define PH_DARWIN 1
#else
    #define PH_DARWIN 0
#endif

#ifdef __linux__
    #define PH_LINUX 1
#else
    #define PH_LINUX 0
#endif

#ifdef __ANDROID__
    #define PH_ANDROID 1
#else
    #define PH_ANDROID 0
#endif

#define PH_UNIX_LIKE (PH_DARWIN || PH_LINUX || PH_ANDROID)
//@}

/// determine if the current platform is 32 or 64 bit
#if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) && !defined(__ILP32__)) || defined(_M_X64) || defined(__ia64) || defined(_M_IA64) || \
    defined(__aarch64__) || defined(__powerpc64__)
    #define PH_64BIT 1
    #define PH_32BIT 0
#else
    #define PH_64BIT 0
    #define PH_32BIT 1
#endif

/// macros to import/export function from shared library.
//@{
#ifdef _MSC_VER
    #define PH_IMPORT __declspec(dllimport)
#else
    #define PH_IMPORT
#endif

#ifdef _MSC_VER
    #define PH_EXPORT __declspec(dllexport)
#else
    #define PH_EXPORT __attribute__ ((visibility("default"))
#endif

#if PH_BUILD_STATIC
    #define PH_API
#elif defined(PH_INTERNAL)
    #define PH_API PH_EXPORT
#else
    #define PH_API PH_IMPORT
#endif
//@}

#ifdef _MSC_VER
    #define PH_FUNCSIG __FUNCSIG__
#elif defined(__GNUC__)
    #define PH_FUNCSIG __PRETTY_FUNCTION__
#else
    #define PH_FUNCSIG __func__
#endif

/// Log macros
///
/// All log messages are associated with a severity number. The lower the number, the higher the severity.
/// Here's the list of built-in severities:
///
///     FATAL    :  0
///     ERROR    : 10
///     WARNING  : 20
///     INFO     : 30
///     VERBOSE  : 40
///     BABBLING : 50
///
/// By default, only log with severity higher or equal to INFO (<=30) is enabled. To change that, set environment
/// variable physray-sdk.log.level to the level you prefer. For example: `set physray-sdk.log.level=10` will leave only
/// FATAL and ERROR messages enabled.
///
/// On Android system, instead of environment variable, please use system property 'debug.physray-sdk.log.level' or
/// 'persist.physray-sdk.log.level'.
///
//@{
#define PH_LOG(tag__, severity, ...)                                                                                                              \
    [&]() -> void {                                                                                                                               \
        using namespace ph::log::macros;                                                                                                          \
        auto ctrl__ = ph::log::Controller::getInstance(tag__);                                                                                    \
        if (ctrl__->enabled(severity)) { ph::log::Helper(ctrl__->tag().c_str(), __FILE__, __LINE__, __FUNCTION__, (int) severity)(__VA_ARGS__); } \
    }()
#define PH_LOGE(...) PH_LOG(, E, __VA_ARGS__)
#define PH_LOGW(...) PH_LOG(, W, __VA_ARGS__)
#define PH_LOGI(...) PH_LOG(, I, __VA_ARGS__)
#define PH_LOGV(...) PH_LOG(, V, __VA_ARGS__)
#define PH_LOGB(...) PH_LOG(, B, __VA_ARGS__)
//@}

/// Log macros enabled only in debug build
//@{
#if PH_BUILD_DEBUG
    #define PH_DLOG PH_LOG
#else
    #define PH_DLOG(...) void(0)
#endif
#define PH_DLOGE(...) PH_DLOG(, E, __VA_ARGS__)
#define PH_DLOGW(...) PH_DLOG(, W, __VA_ARGS__)
#define PH_DLOGI(...) PH_DLOG(, I, __VA_ARGS__)
#define PH_DLOGV(...) PH_DLOG(, V, __VA_ARGS__)
#define PH_DLOGB(...) PH_DLOG(, B, __VA_ARGS__)
//@}

/// throw std::runtime_error exception with source location information
#define PH_THROW(message, ...)                                                              \
    do {                                                                                    \
        std::string theExceptionMessage_ = ::ph::formatstr(message, ##__VA_ARGS__);         \
        ::ph::throwRuntimeErrorException(__FILE__, __LINE__, theExceptionMessage_.c_str()); \
    } while (0)

/// Check for required condition, call the failure clause if the condition is not met.
#define PH_CHK(x, action_on_false)                     \
    if (!(x)) {                                        \
        PH_LOG(, E, "Condition (" #x ") didn't met."); \
        action_on_false;                               \
    } else                                             \
        void(0)

/// Check for required conditions. Throw runtime error exception when the condition is not met.
#define PH_REQUIRE(x) PH_CHK(x, PH_THROW(#x))

/// Runtime assertion for debug build. The assertion failure triggers debug-break signal in debug build.
/// It is no-op in profile and release build, thus can be used in performance critical code path.
//@{
#if PH_BUILD_DEBUG
    #define PH_ASSERT(x, ...)                  \
        if (!(x)) {                            \
            PH_LOGE("ASSERT failure: %s", #x); \
            ::ph::breakIntoDebugger();         \
        } else                                 \
            void(0)
#else
    #define PH_ASSERT(...) (void) 0
#endif
//@}

/// disable copy semantic of a class.
#define PH_NO_COPY(X)      \
    X(const X &) = delete; \
    X & operator=(const X &) = delete

/// disable move semantic of a class.
#define PH_NO_MOVE(X)     \
    X(X &&)     = delete; \
    X & operator=(X &&) = delete

/// disable both copy and move semantic of a class.
#define PH_NO_COPY_NO_NOVE(X) \
    PH_NO_COPY(X);            \
    PH_NO_MOVE(X)

/// enable default copy semantics
#define PH_DEFAULT_COPY(X)  \
    X(const X &) = default; \
    X & operator=(const X &) = default

/// enable default move semantics
#define PH_DEFAULT_MOVE(X) \
    X(X &&)     = default; \
    X & operator=(X &&) = default

/// define move semantics for class implemented using pimpl pattern
#define PH_PIMPL_MOVE(X, pimpl_member, method_to_delete_pimpl)            \
    X(X && w): pimpl_member(w.pimpl_member) { w.pimpl_member = nullptr; } \
    X & operator=(X && w) {                                               \
        if (&w != this) {                                                 \
            this->method_to_delete_pimpl();                               \
            this->pimpl_member = w.pimpl_member;                          \
            w.pimpl_member     = nullptr;                                 \
        }                                                                 \
        return *this;                                                     \
    }

/// endianness
//@{
#if defined(_PPC_) || defined(__BIG_ENDIAN__)
    #define PH_LITTLE_ENDIAN 0
    #define PH_BIG_ENDIAN    1
#else
    #define PH_LITTLE_ENDIAN 1
    #define PH_BIG_ENDIAN    0
#endif
//@}

namespace ph {

struct LogDesc {
    const char * tag;
    const char * file;
    int          line;
    const char * func;
    int          severity;
};

struct LogCallback {
    void (*func)(void * context, const LogDesc & desc, const char * text) = nullptr;
    void * context                                                        = nullptr;
    void   operator()(const LogDesc & desc, const char * text) const { func(context, desc, text); }
};

/// Push a new log callback to the stack.
/// \param lc the new log callback.
/// \return returns the ID of the callback.
PH_API uint64_t registerLogCallback(LogCallback lc);

/// Unregister log callback
/// \param id The callback ID.
PH_API void unregisterLogCallback(uint64_t id);

namespace log { // namespace for log implementation details

class PH_API Controller {

    static struct PH_API Globals {
        std::mutex                          m;
        Controller *                        root;
        int                                 severity;
        std::map<std::string, Controller *> instances;
        Globals();
        ~Globals();
    } g;

    std::string _tag;
    bool        _enabled = true;

    Controller(const char * tag): _tag(tag) {}

    ~Controller() = default;

public:
    static inline Controller * getInstance() { return g.root; }
    static inline Controller * getInstance(Controller * c) { return c; }
    static Controller *        getInstance(const char * tag);

    bool enabled(int severity) const { return _enabled && severity <= g.severity; }

    const std::string & tag() const { return _tag; }
};

namespace macros {

inline constexpr int F = 0;  // fatal
inline constexpr int E = 10; // error
inline constexpr int W = 20; // warning
inline constexpr int I = 30; // informational
inline constexpr int V = 40; // verbose
inline constexpr int B = 50; // babble

inline Controller * c(const char * str = nullptr) { return Controller::getInstance(str); }

struct LogStream {
    std::stringstream ss;
    template<typename T>
    LogStream & operator<<(T && t) {
        ss << std::forward<T>(t);
        return *this;
    }
};

/// Allow string stream to be used in PH_LOG macro: PH_LOGI(s("today's date is ") << date());
inline LogStream s(const char * str = nullptr) {
    LogStream ss;
    if (str && *str) ss.ss << str;
    return ss;
}

}; // namespace macros

class PH_API Helper {

    LogDesc _desc;

    const char * formatlog(const char *, ...);

    void post(const char *);

public:
    template<class... Args>
    Helper(Args &&... args): _desc {args...} {}

    template<class... Args>
    void operator()(const char * format, Args &&... args) {
        post(formatlog(format, std::forward<Args>(args)...));
    }

    template<class... Args>
    void operator()(const std::string & format, Args &&... args) {
        post(formatlog(format.c_str(), std::forward<Args>(args)...));
    }

    template<class... Args>
    void operator()(const std::stringstream & format, Args &&... args) {
        post(formatlog(format.str().c_str(), std::forward<Args>(args)...));
    }

    void operator()(const macros::LogStream & s) { post(formatlog(s.ss.str().c_str())); }

    template<class... Args>
    void operator()(Controller * c, const char * format, Args &&... args) {
        if (Controller::getInstance(c)->enabled(_desc.severity)) {
            _desc.tag = c->tag().c_str();
            operator()(format, std::forward<Args>(args)...);
        }
    }

    template<class... Args>
    void operator()(Controller * c, const std::string & format, Args &&... args) {
        operator()(c, format.c_str(), std::forward<Args>(args)...);
    }

    template<class... Args>
    void operator()(Controller * c, const std::stringstream & format, Args &&... args) {
        operator()(c, format.str().c_str(), std::forward<Args>(args)...);
    }

    void operator()(Controller * c, const macros::LogStream & s) {
        if (Controller::getInstance(c)->enabled(_desc.severity)) {
            _desc.tag = c->tag().c_str();
            operator()(s);
        }
    }
};

} // namespace log

/// Send trap signal to debugger
PH_API void breakIntoDebugger();

/// Throw runtime exception.
/// Instead of embed it in a macro. make this a utility function to make it easier to set debug break point on it.
[[noreturn]] PH_API void throwRuntimeErrorException(const char * file, int line, const char * message);

/// Throw runtime exception.
[[noreturn]] inline void throwRuntimeErrorException(const char * file, int line, const std::string & message) {
    throwRuntimeErrorException(file, line, message.c_str());
}

/// Throw runtime exception.
[[noreturn]] inline void throwRuntimeErrorException(const char * file, int line, const std::stringstream & message) {
    throwRuntimeErrorException(file, line, message.str().c_str());
}

/// Interprets the value of errnum, generating a string with a message that describes the error condition.
inline const char * errno2str(int error) {
#ifdef _MSC_VER
    static thread_local char message[256];
    strerror_s(message, error);
    return message;
#else
    return strerror(error);
#endif
}

/// dump current callstck to string
PH_API std::string backtrace(int indent = 0);

/// allocated aligned memory. The returned pointer must be freed by
PH_API void * aalloc(size_t alignment, size_t bytes);

/// free memory allocated by aalloc()
PH_API void afree(void *);

/// Return's pointer to the internal storage. The content will be overwritten
/// by the next call on the same thread.
PH_API const char * formatstr(const char * format, ...);

/// convert duration in nanoseconds to string
PH_API std::string ns2str(uint64_t ns, int width = 6, int precision = 2);

/// convert high resolution durations to string
inline std::string duration2str(const std::chrono::high_resolution_clock::duration & duration, int width = 6, int precision = 2) {
    return ns2str(std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count(), width, precision);
}

/// Different compiler has slightly different way to query environoment (getenv_s vs. getenv). Thus
/// this method to unify the difference.
PH_API std::string getEnvString(const char * name);

/// Get environment variable prefixed with either 'physray-sdk.'
PH_API std::string getJediEnv(const char * name);

#if PH_ANDROID
/// Get system property without any decorations to the name.
std::string getSystemProperty(const char * name);

/// Get system property prefixed with either 'debug.physray-sdk.' or 'persist.physray-sdk.'. The function will check
/// debug.physray-sdk.<name> first. If the value is empty, then check 'persist.physray-sdk.<name>'
std::string getJediProperty(const char * name);
#endif

/// Return full path to the current executable
PH_API std::string getExecutablePath();

/// Return full path to the folder of the current executable
PH_API std::string getExecutableFolder();

/// call exit function automatically at scope exit
template<typename PROC>
class ScopeExit {
    PROC _proc;
    bool _active = true;

public:
    ScopeExit(PROC proc): _proc(proc) {}

    ~ScopeExit() { exit(); }

    PH_NO_COPY(ScopeExit);
    PH_NO_MOVE(ScopeExit);

    /// manually call the exit function.
    void exit() {
        if (_active) {
            _active = false;
            _proc();
        }
    }

    // dismiss the scope exit action w/o calling it.
    void dismiss() { _active = false; }
};

/// A simple 128-bit integer class. This can also be used to hold a GUID.
union UInt128 {
    struct {
        uint64_t lo;
        uint64_t hi;
    };
    uint64_t u64[2];
    uint32_t u32[4];
    uint16_t u16[8];
    uint8_t  u8[16];

    bool operator==(const UInt128 & rhs) const { return lo == rhs.lo && hi == rhs.hi; }

    bool operator!=(const UInt128 & rhs) const { return lo != rhs.lo || hi != rhs.hi; }

    bool operator<(const UInt128 & rhs) const { return (hi != rhs.hi) ? (hi < rhs.hi) : (lo < rhs.lo); }

    static UInt128 make(uint64_t lo, uint64_t hi) {
        UInt128 r;
        r.lo = lo;
        r.hi = hi;
        return r;
    }

    /// Make a 128 bit integer from GUID in form of: {aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee}
    static UInt128 make(uint32_t a, uint16_t b, uint16_t c, uint16_t d, uint64_t e) {
        UInt128 r;
        r.lo     = e;
        r.u16[3] = d;
        r.u16[4] = c;
        r.u16[5] = b;
        r.u32[3] = a;
        return r;
    }
};
static_assert(128 == sizeof(UInt128) * 8);

/// Commonly used math constants
//@{
inline static constexpr float PI         = 3.1415926535897932385f;
inline static constexpr float QUARTER_PI = (PI / 4.0f);
inline static constexpr float HALF_PI    = (PI / 2.0f);
inline static constexpr float TWO_PI     = (PI * 2.0f);
//@}

// Basic math utlities
//@{

// -----------------------------------------------------------------------------
/// degree -> radian
template<typename T>
inline T deg2rad(T a) {
    return a * (T) 0.01745329252f;
}

// -----------------------------------------------------------------------------
/// radian -> degree
template<typename T>
inline T rad2deg(T a) {
    return a * (T) 57.29577951f;
}

// -----------------------------------------------------------------------------
/// check if n is power of 2
template<typename T>
inline constexpr bool isPowerOf2(T n) {
    return (0 == (n & (n - 1))) && (0 != n);
}

// -----------------------------------------------------------------------------
/// the smallest power of 2 larger or equal to n
inline constexpr uint32_t ceilPowerOf2(uint32_t n) {
    n -= 1;
    n |= n >> 16;
    n |= n >> 8;
    n |= n >> 4;
    n |= n >> 2;
    n |= n >> 1;
    return n + 1;
}

// -----------------------------------------------------------------------------
/// the smallest power of 2 larger or equal to n
inline constexpr uint64_t ceilPowerOf2(uint64_t n) {
    n -= 1;
    n |= n >> 32;
    n |= n >> 16;
    n |= n >> 8;
    n |= n >> 4;
    n |= n >> 2;
    n |= n >> 1;
    return n + 1;
}

// -----------------------------------------------------------------------------
/// the lastest power of 2 smaller or equal to n
inline constexpr uint32_t floorPowerOf2(uint32_t n) {
    n |= n >> 16;
    n |= n >> 8;
    n |= n >> 4;
    n |= n >> 2;
    n |= n >> 1;
    return (n + 1) >> 1;
}

// -----------------------------------------------------------------------------
/// the lastest power of 2 smaller or equal to n
inline constexpr uint64_t floorPowerOf2(uint64_t n) {
    n |= n >> 32;
    n |= n >> 16;
    n |= n >> 8;
    n |= n >> 4;
    n |= n >> 2;
    n |= n >> 1;
    return (n + 1) >> 1;
}

// -----------------------------------------------------------------------------
//
template<typename T>
inline constexpr T clamp(T value, const T & vmin, const T & vmax) {
    return vmin > value ? vmin : vmax < value ? vmax : value;
}

// -----------------------------------------------------------------------------
/// align numeric value up to the next multiple of the alignment.
/// Note that the alignment must be 2^N.
template<typename T>
inline constexpr T nextMultiple(const T & value, const T & alignment) {
    PH_ASSERT(isPowerOf2(alignment));
    return (value + (alignment - 1)) & ~(alignment - 1);
}

//@}

/// This is a little helper class to do static_assert in constexpr if statement within a template.
///
/// Example:
///
///     template<typename T>
///     void foo() {
///         if constexpr (contdition1<T>) { ... }
///         else if constexpr (condition2<T>) { ... }
///         else {
///             // generate build error if neither condition1 nor condition2 are met.
///             static_assert(AlwaysFalse<T>);
///         }
///     }
///
template<class...>
static constexpr std::false_type AlwaysFalse {};

/// An array allocated on stack, similar as std::array. But it supports common array operations like push, pop,
/// insert, remove and etc. No dynamic allocation during the life time the array.
template<class T, size_t N_MAX, typename SIZE_TYPE = size_t>
class StackArray {
    uint8_t   _buffer[sizeof(T) * N_MAX];
    SIZE_TYPE _count = 0;

    /// default construct
    static inline void ctor(T * ptr, SIZE_TYPE count) {
        for (SIZE_TYPE i = 0; i < count; ++i, ++ptr) {
            new (ptr) T();
        }
    }

    /// construct from known value
    static inline void ctor(T * ptr, SIZE_TYPE count, const T & value) {
        for (SIZE_TYPE i = 0; i < count; ++i, ++ptr) {
            new (ptr) T(value);
        }
    }

    /// destructor
    static inline void dtor(T * ptr) { ptr->T::~T(); }

    void doClear() {
        T * p = data();
        for (SIZE_TYPE i = 0; i < _count; ++i, ++p) {
            dtor(p);
        }
        _count = 0;
    }

    void copyFrom(const StackArray & other) {
        T *       dst = data();
        const T * src = other.data();

        SIZE_TYPE mincount = std::min<SIZE_TYPE>(_count, other._count);
        for (SIZE_TYPE i = 0; i < mincount; ++i) {
            dst[i] = src[i];
        }

        // destruct extra objects, only when other._count < _count
        for (SIZE_TYPE i = other._count; i < _count; ++i) {
            dtor(dst + i);
        }

        // copy-construct new objects, only when _count < other._count
        for (SIZE_TYPE i = _count; i < other._count; ++i) {
            new (dst + i) T(src[i]);
        }

        _count = other._count;
    }

    void doInsert(SIZE_TYPE position, const T & t) {
        PH_REQUIRE(_count < N_MAX);
        PH_REQUIRE(position <= _count);

        T * p = data();

        for (SIZE_TYPE i = _count; i > position; --i) {
            // TODO: use move operator when available
            p[i] = p[i - 1];
        }

        // insert new elements
        p[position] = t;

        ++_count;
    }

    void doErase(SIZE_TYPE position) {
        if (position >= _count) {
            PH_LOGE("Invalid eraseIdx position");
            return;
        }

        --_count;

        T * p = data();

        // move elements
        for (SIZE_TYPE i = position; i < _count; ++i) {
            // TODO: use move operator when available
            p[i] = p[i + 1];
        }

        // destruct last element
        dtor(p + _count);
    }

    void doResize(SIZE_TYPE count) {
        if (count == _count) return; // shortcut for redundant call.

        PH_REQUIRE(count <= N_MAX);

        T * p = data();

        // destruct extra objects, only when count < _count
        for (SIZE_TYPE i = count; i < _count; ++i) {
            dtor(p + i);
        }

        // construct new objects, only when _count < count
        for (SIZE_TYPE i = _count; i < count; ++i) {
            ctor(p + i, 1);
        }

        _count = count;
    }

    bool equal(const StackArray & other) const {
        if (_count != other._count) return false;

        const T * p1 = data();
        const T * p2 = other.data();

        for (SIZE_TYPE i = 0; i < _count; ++i) {
            if (p1[i] != p2[i]) return false;
        }
        return true;
    }

public:
    typedef T ElementType; ///< element type

    static const SIZE_TYPE MAX_SIZE = (SIZE_TYPE) N_MAX; ///< maximum size

    ///
    /// default constructor
    ///
    StackArray() = default;

    ///
    /// constructor with user-defined count
    ///
    explicit StackArray(SIZE_TYPE count) { ctor(data(), count); }

    ///
    /// constructor with user-defined count and value
    ///
    explicit StackArray(SIZE_TYPE count, const T & value) { ctor(data(), count, value); }

    ///
    /// copy constructor
    ///
    StackArray(const StackArray & other) { copyFrom(other); }

    ///
    /// dtor
    ///
    ~StackArray() { doClear(); }

    /// \name Common array operations.
    ///
    //@{
    void      append(const T & t) { doInsert(_count, t); }
    const T & back() const {
        PH_ASSERT(_count > 0);
        return data()[_count - 1];
    }
    T & back() {
        PH_ASSERT(_count > 0);
        return data()[_count - 1];
    }
    const T * begin() const { return data(); }
    T *       begin() { return data(); }
    void      clear() { doClear(); }
    const T * data() const { return (const T *) _buffer; }
    T *       data() { return (T *) _buffer; }
    bool      empty() const { return 0 == _count; }
    const T * end() const { return data() + _count; }
    T *       end() { return data() + _count; }
    void      eraseIdx(SIZE_TYPE position) { doErase(position); }
    void      erasePtr(const T * ptr) { doErase(ptr - _buffer); }
    const T & front() const {
        PH_ASSERT(_count > 0);
        return data()[0];
    }
    T & front() {
        PH_ASSERT(_count > 0);
        return data()[0];
    }
    void      insert(SIZE_TYPE position, const T & t) { doInsert(position, t); }
    void      resize(SIZE_TYPE count) { doResize(count); }
    void      popBack() { doErase(_count - 1); }
    SIZE_TYPE size() const { return _count; }
    //@}

    /// \name common operators
    ///
    //@{
    StackArray & operator=(const StackArray & other) {
        copyFrom(other);
        return *this;
    }
    bool operator==(const StackArray & other) const { return equal(other); }
    bool operator!=(const StackArray & other) const { return !equal(other); }
    T &  operator[](SIZE_TYPE i) {
        PH_ASSERT(i < _count);
        return data()[i];
    }
    const T & operator[](SIZE_TYPE i) const {
        PH_ASSERT(i < _count);
        return data()[i];
    }
    //@}
};

/// A binary block that can't resize at all. Its difference to std::array is that its size is determined at runtime.
/// The memory is allocated on heap, not stack. It does not move or copy elements. So the class T can have both move and
/// copy operators deleted. The only requirement to T is that it must have default constructor.
///
/// This class can be used to pass binary data around (like across DLL boundaries), w/o dependencies to STL.
template<typename T>
class Blob {
    T *    _ptr  = nullptr;
    size_t _size = 0;

public:
    Blob() = default;

    Blob(size_t n) { discardAndReallocate(n); }

    Blob(const T * begin, const T * end) {
        size_t n = (end - begin);
        discardAndReallocate(n);
        if (n > 0) { memcpy(_ptr, begin, n * sizeof(T)); }
    }

    /// Copy data from vector.
    Blob(const std::vector<T> & v): Blob(v.data(), v.data() + v.size()) {}

    /// Move data from vector is not allowed
    Blob(std::vector<T> && v) = delete;

    PH_NO_COPY(Blob);

    // can move
    Blob(Blob && that) {
        _ptr       = that._ptr;
        that._ptr  = nullptr;
        _size      = that._size;
        that._size = 0;
    }
    Blob & operator=(Blob && that) {
        if (this != &that) {
            deallocate();
            _ptr       = that._ptr;
            that._ptr  = nullptr;
            _size      = that._size;
            that._size = 0;
        }
        return *this;
    }

    ~Blob() { deallocate(); }

    /// IMPORTANT: this method is different from std::vector::resize() that it does _NOT_ preserve old content.
    void discardAndReallocate(size_t n) {
        deallocate();
        if (n > 0) {
            _ptr  = new T[n];
            _size = n;
        }
    }

    void deallocate() {
        if (_ptr) {
            delete[] _ptr;
            _ptr = nullptr;
        }
        _size = 0;
    }

    /// Copy data to std::vector
    std::vector<T> toStdVector() const { return std::vector<T>(_ptr, _ptr + _size); }

    /// make a clone of the current blob
    Blob<T> clone() const { return Blob<T>(begin(), end()); }

    bool empty() const { return !_ptr; }

    auto size() const -> size_t { return _size; }

    auto data() -> T * { return _ptr; }
    auto data() const -> const T * { return _ptr; }

    auto begin() -> T * { return _ptr; }
    auto begin() const -> const T * { return _ptr; }

    auto end() -> T * { return _ptr + _size; }
    auto end() const -> const T * { return _ptr + _size; }

    auto back() -> T & { return _ptr[_size - 1]; }
    auto back() const -> const T & { return _ptr[_size - 1]; }

    auto operator[](size_t index) const -> const T & {
        PH_ASSERT(index < _size);
        return _ptr[index];
    }
    auto operator[](size_t index) -> T & {
        PH_ASSERT(index < _size);
        return _ptr[index];
    }
};

/// Represents a non-resizealbe list of elements. The range is fixed. But the content/elemnts could be mutable.
template<typename T, typename SIZE_T = size_t>
class MutableRange {
    T *    _ptr;  ///< pointer to the first element in the list.
    SIZE_T _size; ///< number of elements in the list.

public:
    MutableRange(): _ptr(nullptr), _size(0) {}

    MutableRange(T * ptr, SIZE_T size): _ptr(ptr), _size(size) {}

    MutableRange(std::vector<T> & v): _ptr(v.data()), _size(v.size()) {}

    MutableRange(Blob<T> & v): _ptr(v.data()), _size((SIZE_T) v.size()) {}

    template<SIZE_T N>
    MutableRange(StackArray<T, N> & v): _ptr(v.data()), _size(v.size()) {}

    template<SIZE_T N>
    MutableRange(std::array<T, N> & v): _ptr(v.data()), _size(v.size()) {}

    PH_DEFAULT_COPY(MutableRange);

    PH_DEFAULT_MOVE(MutableRange);

    void clear() {
        _ptr  = nullptr;
        _size = 0;
    }
    bool   empty() const { return 0 == _ptr || 0 == _size; }
    SIZE_T size() const { return _size; }
    T *    begin() const { return _ptr; }
    T *    end() const { return _ptr + _size; }
    T *    data() const { return _ptr; }
    T &    at(SIZE_T i) const {
        PH_ASSERT(i < _size);
        return _ptr[i];
    }
    T & operator[](SIZE_T i) const { return at(i); }
};

/// Represents a constant non-resizealbe list of elements.
template<typename T, typename SIZE_T = size_t>
class ConstRange {
    const T * _ptr;  ///< pointer to the first element in the list.
    SIZE_T    _size; ///< number of elements in the list.

public:
    constexpr ConstRange(): _ptr(nullptr), _size(0) {}

    constexpr ConstRange(const MutableRange<T, SIZE_T> & mr): _ptr(mr.begin()), _size(mr.size()) {}

    constexpr ConstRange(const T * ptr, SIZE_T size): _ptr(ptr), _size(size) {}

    constexpr ConstRange(const std::vector<T> & v): _ptr(v.data()), _size((SIZE_T) v.size()) {}

    constexpr ConstRange(const Blob<T> & v): _ptr(v.data()), _size((SIZE_T) v.size()) {}

    template<SIZE_T N>
    constexpr ConstRange(const T (&array)[N]): _ptr(array), _size(N) {}

    template<SIZE_T N>
    constexpr ConstRange(const StackArray<T, N> & v): _ptr(v.data()), _size(v.size()) {}

    template<SIZE_T N>
    constexpr ConstRange(const std::array<T, N> & v): _ptr(v.data()), _size(v.size()) {}

    PH_DEFAULT_COPY(ConstRange);

    PH_DEFAULT_MOVE(ConstRange);

    void clear() {
        _ptr  = nullptr;
        _size = 0;
    }

    constexpr bool empty() const { return 0 == _ptr || 0 == _size; }

    constexpr SIZE_T size() const { return _size; }

    constexpr const T * begin() const { return _ptr; }
    constexpr const T * end() const { return _ptr + _size; }
    constexpr const T * data() const { return _ptr; }
    constexpr const T & at(SIZE_T i) const {
        PH_ASSERT(i < _size);
        return _ptr[i];
    }
    constexpr const T & operator[](SIZE_T i) const { return at(i); }

    /// conver to std::initialize_list
    constexpr std::initializer_list<T> il() const { return std::initializer_list<T>(_ptr, _ptr + _size); }
};

struct ScopedCpuTrace {
    using TimePoint = std::chrono::high_resolution_clock::time_point;

    bool      begun = true;
    TimePoint beginTime;
    TimePoint endTime;

    ScopedCpuTrace(const char * name);

    ~ScopedCpuTrace() { end(); }

    /// end the scope. Returns the duration in nanoseconds.
    uint64_t end();
};

/// T must be an numerical type that supports common numerical operations like +,-,*,/.
/// TODO: move to base module
template<typename T>
struct NumericalAverager {
    ph::Blob<T> buffer;
    size_t      cursor = 0; // point to the next empty space in the buffer.
    T           low = T {0}, high = T {0}, average = T {0};

    NumericalAverager(size_t N_ = 60, std::chrono::high_resolution_clock::duration refreshInterval = std::chrono::seconds(1))
        : buffer(N_), _refreshInternal(refreshInterval) {
        PH_REQUIRE(N_ > 0);
        reset();
    }

    NumericalAverager & reset() {
        for (size_t i = 0; i < buffer.size(); ++i) {
            buffer[i] = T {0};
        }
        cursor  = 0;
        low     = T {0};
        high    = T {0};
        average = T {0};
        return *this;
    }

    /// Return the latest value.
    const T & latest() const { return buffer[(cursor - 1) % buffer.size()]; }

    NumericalAverager & update(T newValue) {
        buffer[cursor % buffer.size()] = newValue;
        cursor++;
        refreshAverage();
        return *this;
    }

    NumericalAverager & operator=(T newValue) { return update(newValue); }

private:
    using TimePoint = std::chrono::high_resolution_clock::time_point;
    using Duration  = std::chrono::high_resolution_clock::duration;

    const Duration _refreshInternal;
    TimePoint      _lastRefreshTimePoint {};

    // TODO: amortize the cost to each frame using moving-window algorithm.
    void refreshAverage() {
        using namespace std::chrono_literals;
        auto now = std::chrono::high_resolution_clock::now();
        if ((now - _lastRefreshTimePoint) > _refreshInternal) {
            _lastRefreshTimePoint = now;
            auto count            = std::min(cursor, buffer.size());
            low = high = buffer[0];
            average    = buffer[0] / count;
            for (size_t i = 1; i < count; ++i) {
                const auto & v = buffer[i];
                average += v / count;
                if (v < low)
                    low = v;
                else if (v > high)
                    high = v;
            }
        }
    }
};

/// A utility class to help collecting CPU times on frame bassis.
class SimpleCpuFrameTimes {
public:
    struct Report {
        const char * name       = nullptr;
        uint64_t     durationNs = 0;
        uint32_t     level      = 0;
    };

    SimpleCpuFrameTimes();

    ~SimpleCpuFrameTimes();

    PH_NO_COPY(SimpleCpuFrameTimes);
    PH_NO_MOVE(SimpleCpuFrameTimes);

    void begin(const char * name);

    /// \returns Returns duration in nanoseconds from the call to the paired begin(). Or 0, in case of error.
    uint64_t end();

    /// Must be called once and only once every frame.
    void frame();

    ConstRange<Report> reportAll() const;

    struct ScopedTimer {
        SimpleCpuFrameTimes * t;
        ScopedTimer(SimpleCpuFrameTimes & t_, const char * name): t(&t_) { t_.begin(name); }
        ScopedTimer(SimpleCpuFrameTimes * t_, const char * name): t(t_) {
            if (t_) t_->begin(name);
        }
        ~ScopedTimer() {
            if (t) t->end();
        }
    };

private:
    class Impl;
    Impl * _impl;
};

} // namespace ph
