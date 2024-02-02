/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

#include <vector>

namespace ph {

/// Represents a constant c-style array pointer with size.
template<typename T, typename SIZE_T = size_t>
class ArrayView {
    const T * _ptr;  ///< pointer to the first element in the list.
    SIZE_T    _size; ///< number of elements in the list.

public:
    ArrayView(): _ptr(nullptr), _size(0) {}

    template<typename T2>
    ArrayView(const ArrayView<T2> & rhs): _ptr(rhs._ptr), _size(rhs._size) {}

    template<typename T2>
    ArrayView(const T2 * ptr, SIZE_T size): _ptr(ptr), _size(size) {}

    template<typename T2, SIZE_T N>
    ArrayView(const T2 (&array)[N]): _ptr(array), _size(N) {}

    template<typename T2>
    ArrayView(const std::vector<T2> & v): _ptr(v.data()), _size(v.size()) {}

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
    constexpr const T & operator[](SIZE_T i) const { return at(i); }
};

} // namespace ph