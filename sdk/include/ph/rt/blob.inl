#include <vector>

namespace ph {
namespace rt {

/// A binary block that can't resize at all. Its difference to std::array is that its size is determined at runtime.
/// The memory is allocated on heap, not stack.
///
/// Other than not resizable, the other main difference to std::vector is that it is able to hold class that has
/// neither move nor copy operator. The only requirement to T is that it must have a default constructor.
template<typename T>
class Blob {
public:
    Blob(): _ptr(0), _size(0) {}

    Blob(size_t n): _ptr(0), _size(0) { discardAndReallocate(n); }

    Blob(const std::vector<T> & v): _ptr(0), _size(0) { reset(v.data(), v.size()); }

    Blob(const T * begin, const T * end): _ptr(0), _size(0) { reset(begin, end - begin); }

    Blob(const T * ptr, size_t size): _ptr(0), _size(0) { reset(ptr, size); }

    // copy constructor
    Blob(const Blob & rhs): _ptr(0), _size(0) { reset(rhs._ptr, rhs._size); }

    // copy operator
    Blob & operator=(const Blob & rhs) {
        if (this == &rhs) return *this;
        reset(rhs._ptr, rhs._size);
        return *this;
    }

    /// move constructor
    Blob(Blob && that) {
        _ptr       = that._ptr;
        that._ptr  = nullptr;
        _size      = that._size;
        that._size = 0;
    }

    /// move operator
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

    Blob<T>   clone() const { return Blob<T>(begin(), end()); }
    bool      empty() const { return !_ptr; }
    size_t    size() const { return _size; }
    T *       data() { return _ptr; }
    const T * data() const { return _ptr; }
    T *       begin() { return _ptr; }
    const T * begin() const { return _ptr; }
    T *       end() { return _ptr + _size; }
    const T * end() const { return _ptr + _size; }
    T &       back() { return _ptr[_size - 1]; }
    const T & back() const { return _ptr[_size - 1]; }

    const T & operator[](size_t index) const {
        assert(index < _size);
        return _ptr[index];
    }

    T & operator[](size_t index) {
        assert(index < _size);
        return _ptr[index];
    }

private:
    T *    _ptr;
    size_t _size;

    void reset(const T * ptr, size_t size) {
        if (!ptr) size = 0;
        discardAndReallocate(size);
        for (size_t i = 0; i < size; ++i) _ptr[i] = ptr[i];
    }
};

/// Represents a constant c-style array pointer with size.
template<typename T, typename SIZE_T = size_t>
class BlobProxy {
    const T * _ptr;  ///< pointer to the first element in the list.
    SIZE_T    _size; ///< number of elements in the list.

public:
    BlobProxy(): _ptr(nullptr), _size(0) {}

    template<typename T2>
    BlobProxy(const BlobProxy<T2> & rhs): _ptr(rhs._ptr), _size(rhs._size) {}

    template<typename T2>
    BlobProxy(const T2 * ptr, SIZE_T size): _ptr(ptr), _size(size) {}

    template<typename T2>
    BlobProxy(const Blob<T2> & blob): _ptr(blob.data()), _size(blob.size()) {}

    template<typename T2, SIZE_T N>
    BlobProxy(const T2 (&array)[N]): _ptr(array), _size(N) {}

    template<typename T2>
    BlobProxy(const std::vector<T2> & v): _ptr(v.data()), _size(v.size()) {}

    template<typename T2>
    void reset(const T2 * ptr, SIZE_T size) {
        _ptr  = ptr;
        _size = size;
    }

    void clear() {
        _ptr  = nullptr;
        _size = 0;
    }

    bool empty() const { return 0 == _ptr || 0 == _size; }

    SIZE_T size() const { return _size; }

    const T * begin() const { return _ptr; }
    const T * end() const { return _ptr + _size; }
    const T * data() const { return _ptr; }
    const T & at(SIZE_T i) const {
        assert(i < _size);
        return _ptr[i];
    }
    PH_RT_CONSTEXPR const T & operator[](SIZE_T i) const { return at(i); }
};

} // namespace rt
} // namespace ph