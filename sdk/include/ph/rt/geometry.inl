#include <stddef.h>
#include <assert.h>

namespace ph {
namespace rt {

struct Float2 {
    float x, y;

    static Float2 make(float x, float y) {
        Float2 f = {x, y};
        return f;
    }

    void set(float x_, float y_) {
        x = x_;
        y = y_;
    }

    bool operator==(const Float2 & rhs) const { return x == rhs.x && y == rhs.y; }
    bool operator!=(const Float2 & rhs) const { return x != rhs.x || y != rhs.y; }

    const float & operator[](size_t index) const {
        assert(index < 2);
        return (&x)[index];
    }
    float & operator[](size_t index) {
        assert(index < 2);
        return (&x)[index];
    }
    const float & operator()(size_t index) const {
        assert(index < 2);
        return (&x)[index];
    }
    float & operator()(size_t index) {
        assert(index < 2);
        return (&x)[index];
    }
};

struct Float3 {
    float x, y, z;

    static Float3 make(float x_ = 0.f, float y_ = 0.f, float z_ = 0.f) {
        Float3 f;
        f.set(x_, y_, z_);
        return f;
    }

    void set(float x_, float y_, float z_) {
        x = x_;
        y = y_;
        z = z_;
    }

    bool operator==(const Float3 & rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z; }
    bool operator!=(const Float3 & rhs) const { return x != rhs.x || y != rhs.y || z != rhs.z; }

    const float & operator[](size_t index) const {
        assert(index < 3);
        return (&x)[index];
    }
    float & operator[](size_t index) {
        assert(index < 3);
        return (&x)[index];
    }
    const float & operator()(size_t index) const {
        assert(index < 3);
        return (&x)[index];
    }
    float & operator()(size_t index) {
        assert(index < 3);
        return (&x)[index];
    }
};

struct Float4 {
    float x, y, z, w;

    static Float4 make(float x_ = 0.f, float y_ = 0.f, float z_ = 0.f, float w_ = 1.f) {
        Float4 f;
        f.set(x_, y_, z_, w_);
        return f;
    }

    void set(float x_, float y_, float z_, float w_) {
        x = x_;
        y = y_;
        z = z_;
        w = w_;
    }

    bool operator==(const Float4 & rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w; }
    bool operator!=(const Float4 & rhs) const { return x != rhs.x || y != rhs.y || z != rhs.z || w != rhs.w; }

    const float & operator[](size_t index) const {
        assert(index < 4);
        return (&x)[index];
    }
    float & operator[](size_t index) {
        assert(index < 4);
        return (&x)[index];
    }
    const float & operator()(size_t index) const {
        assert(index < 4);
        return (&x)[index];
    }
    float & operator()(size_t index) {
        assert(index < 4);
        return (&x)[index];
    }
};

/// A 3x3 column major matrix.
struct Float3x3 {
    Float3 col0;
    Float3 col1;
    Float3 col2;

    bool operator==(const Float3x3 & rhs) const { return col0 == rhs.col0 && col1 == rhs.col1 && col2 == rhs.col2; }
    bool operator!=(const Float3x3 & rhs) const { return col0 != rhs.col0 || col1 != rhs.col1 || col2 != rhs.col2; }

    const Float3 & operator[](size_t index) const {
        assert(index < 3);
        return (&col0)[index];
    }
    Float3 & operator[](size_t index) {
        assert(index < 3);
        return (&col0)[index];
    }
    const float & operator()(size_t row, size_t col) const {
        assert(row < 3);
        assert(col < 3);
        return (*this)[col][row];
    }
    float & operator()(size_t row, size_t col) {
        assert(row < 3);
        assert(col < 3);
        return (*this)[col][row];
    }
};

/// A 3x4 column major matrix. Usually used to represent affine transform.
struct Float3x4 {
    Float3 col0;
    Float3 col1;
    Float3 col2;
    Float3 col3;

    // FIXME: Removing this constructor causes rendering glitch in cornell scene.
    // It means somewhere in our code base, we rely on Float3x4 to be constructed to
    // identity.
    Float3x4() {
        col0.set(1, 0, 0);
        col1.set(0, 1, 0);
        col2.set(0, 0, 1);
        col3.set(0, 0, 0);
    }

    static inline Float3x4 identity() {
        Float3x4 f;
        f.col0.set(1, 0, 0);
        f.col1.set(0, 1, 0);
        f.col2.set(0, 0, 1);
        f.col3.set(0, 0, 0);
        return f;
    }

    const Float3 & translation() const { return col3; }
    Float3 &       translation() { return col3; }

    bool operator==(const Float3x4 & rhs) const { return col0 == rhs.col0 && col1 == rhs.col1 && col2 == rhs.col2 && col3 == rhs.col3; }
    bool operator!=(const Float3x4 & rhs) const { return col0 != rhs.col0 || col1 != rhs.col1 || col2 != rhs.col2 || col3 != rhs.col3; }

    const Float3 & operator[](size_t index) const {
        assert(index < 4);
        return (&col0)[index];
    }
    Float3 & operator[](size_t index) {
        assert(index < 4);
        return (&col0)[index];
    }
    const float & operator()(size_t row, size_t col) const {
        assert(row < 4);
        assert(col < 4);
        return (*this)[col][row];
    }
    float & operator()(size_t row, size_t col) {
        assert(row < 4);
        assert(col < 4);
        return (*this)[col][row];
    }

    friend inline Float3x4 operator*(const Float3x4 & a, const Float3x4 & b) {
        Float3x4 ret;

        ret(0, 0) = a(0, 0) * b(0, 0) + a(0, 1) * b(1, 0) + a(0, 2) * b(2, 0);
        ret(0, 1) = a(0, 0) * b(0, 1) + a(0, 1) * b(1, 1) + a(0, 2) * b(2, 1);
        ret(0, 2) = a(0, 0) * b(0, 2) + a(0, 1) * b(1, 2) + a(0, 2) * b(2, 2);
        ret(0, 3) = a(0, 0) * b(0, 3) + a(0, 1) * b(1, 3) + a(0, 2) * b(2, 3) + a(0, 3);

        ret(1, 0) = a(1, 0) * b(0, 0) + a(1, 1) * b(1, 0) + a(1, 2) * b(2, 0);
        ret(1, 1) = a(1, 0) * b(0, 1) + a(1, 1) * b(1, 1) + a(1, 2) * b(2, 1);
        ret(1, 2) = a(1, 0) * b(0, 2) + a(1, 1) * b(1, 2) + a(1, 2) * b(2, 2);
        ret(1, 3) = a(1, 0) * b(0, 3) + a(1, 1) * b(1, 3) + a(1, 2) * b(2, 3) + a(1, 3);

        ret(2, 0) = a(2, 0) * b(0, 0) + a(2, 1) * b(1, 0) + a(2, 2) * b(2, 0);
        ret(2, 1) = a(2, 0) * b(0, 1) + a(2, 1) * b(1, 1) + a(2, 2) * b(2, 1);
        ret(2, 2) = a(2, 0) * b(0, 2) + a(2, 1) * b(1, 2) + a(2, 2) * b(2, 2);
        ret(2, 3) = a(2, 0) * b(0, 3) + a(2, 1) * b(1, 3) + a(2, 2) * b(2, 3) + a(2, 3);

        return ret;
    }
};

/// A 4x4 column major matrix.
struct Float4x4 {
    Float4 col0;
    Float4 col1;
    Float4 col2;
    Float4 col3;

    static inline Float4x4 identity() {
        Float4x4 f;
        f.col0.set(1, 0, 0, 0);
        f.col1.set(0, 1, 0, 0);
        f.col2.set(0, 0, 1, 0);
        f.col3.set(0, 0, 0, 1);
        return f;
    }

    bool operator==(const Float4x4 & rhs) const { return col0 == rhs.col0 && col1 == rhs.col1 && col2 == rhs.col2 && col3 == rhs.col3; }
    bool operator!=(const Float4x4 & rhs) const { return col0 != rhs.col0 || col1 != rhs.col1 || col2 != rhs.col2 || col3 != rhs.col3; }

    const Float4 & operator[](size_t index) const { return (&col0)[index]; }
    Float4 &       operator[](size_t index) { return (&col0)[index]; }
    const float &  operator()(size_t row, size_t col) const { return (*this)[col][row]; }
    float &        operator()(size_t row, size_t col) { return (*this)[col][row]; }
};

} // namespace rt
} // namespace ph