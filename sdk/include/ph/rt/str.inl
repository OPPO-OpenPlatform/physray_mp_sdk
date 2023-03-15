#include <stdlib.h> // form malloc() and free()
#include <wchar.h>  // for wchar_t
#include <string>

namespace ph {
namespace rt {

///
/// Custom string class since we can't use std::string in rt.h. CHAR type must be POD type.
///
template<typename CHAR>
class Str {
    typedef CHAR CharType;

    CharType * _ptr; ///< string buffer pointer.

public:
    ///
    /// indicate serach failure.
    ///
    static const size_t NOT_FOUND = (size_t) -1;

    ///
    /// default constructor
    ///
    Str(): _ptr(nullptr) {
        setCaps(0);
        _ptr[0] = 0;
    }

    ///
    /// copy constructor
    ///
    Str(const Str & s): _ptr(nullptr) {
        setCaps(s.size());
        ::memcpy(_ptr, s._ptr, (s.size() + 1) * sizeof(CharType));
        setSize(s.size());
    }

    ///
    /// move constructor
    ///
    Str(Str && s): _ptr(s._ptr) {
        if (&s != this) s._ptr = emptyStr();
    }

    ///
    /// constructor from c-style string
    ///
    Str(const CharType * s, size_t l = 0): _ptr(nullptr) {
        if (0 == s) {
            setCaps(0);
            setSize(0);
            _ptr[0] = 0;
        } else {
            l = length(s, l);
            setCaps(l);
            ::memcpy(_ptr, s, l * sizeof(CharType));
            _ptr[l] = 0;
            setSize(l);
        }
    }

    ///
    /// construct from STL string.
    ///
    Str(const std::basic_string<CharType> & s): _ptr(nullptr) {
        auto l = s.size();
        setCaps(l);
        ::memcpy(_ptr, s.c_str(), l * sizeof(CharType));
        _ptr[l] = 0;
        setSize(l);
    }

    ///
    /// destructor
    ///
    ~Str() { deallocate(_ptr); }

    ///
    /// append to this string
    ///
    void append(const CharType * s, size_t l = 0) {
        if (0 == s) return;
        l = length(s, l);
        if (0 == l) return;
        size_t oldsize = size();
        size_t newsize = oldsize + l;
        setCaps(newsize);
        setSize(newsize);
        ::memcpy(_ptr + oldsize, s, l * sizeof(CharType));
        _ptr[newsize] = 0;
    }

    ///
    /// append to this string
    ///
    void append(const Str & s) {
        if (s.empty()) return;
        size_t oldsize = size();
        size_t ssize   = s.size();
        size_t newsize = oldsize + ssize;
        setCaps(newsize);
        setSize(newsize);
        ::memcpy(_ptr + oldsize, s, ssize * sizeof(CharType));
        _ptr[newsize] = 0;
    }

    ///
    /// append to this string
    ///
    void append(CharType ch) {
        if (0 == ch) return;
        size_t oldsize = size();
        size_t newsize = oldsize + 1;
        setCaps(newsize);
        setSize(newsize);
        _ptr[oldsize] = ch;
        _ptr[newsize] = 0;
    }

    ///
    /// assign value to string class
    ///
    void assign(const CharType * s, size_t l = 0) {
        if (0 == s) {
            setCaps(0);
            setSize(0);
            _ptr[0] = 0;
        } else {
            l = length(s, l);
            setCaps(l);
            ::memcpy(_ptr, s, l * sizeof(CharType));
            _ptr[l] = 0;
            setSize(l);
        }
    }

    ///
    /// begin iterator(1)
    ///
    CharType * begin() { return _ptr; }

    ///
    /// begin iterator(2)
    ///
    const CharType * begin() const { return _ptr; }

    ///
    /// Clear to empty string
    ///
    void clear() {
        _ptr[0] = 0;
        setSize(0);
    }

    ///
    /// be syntax compatible with std::string
    ///
    const CharType * c_str() const { return _ptr; }

    ///
    /// return c-style const char pointer
    ///
    const CharType * data() const { return _ptr; }

    ///
    /// empty string or not?
    ///
    bool empty() const { return 0 == size(); }

    ///
    /// begin iterator(1)
    ///
    CharType * end() { return _ptr + size(); }

    ///
    /// begin iterator(2)
    ///
    const CharType * end() const { return _ptr + size(); }

    ///
    /// Searches through a string for the first character that matches any elements in user specified string
    ///
    /// \param s
    ///     User specified search pattern
    /// \param offset, count
    ///     Range of the search. (count==0) means to the end of input string.
    /// \return
    ///     Return index of the character of first instance, or NOT_FOUND.
    ///
    size_t findFirstOf(const CharType * s, size_t offset = 0, size_t count = 0) const {
        if (0 == s || 0 == *s) return NOT_FOUND;
        if (offset >= size()) return NOT_FOUND;
        if (0 == count) count = size();
        if (offset + count > size()) count = size() - offset;
        const CharType * p = _ptr + offset;
        for (size_t i = 0; i < count; ++i, ++p) {
            for (const CharType * t = s; *t; ++t) {
                assert(*p && *t);
                if (*p == *t) return offset + i;
            }
        }
        return NOT_FOUND;
    }

    ///
    /// Searches through a string for the first character that not any elements of user specifed string.
    ///
    size_t findFirstNotOf(const CharType * s, size_t offset = 0, size_t count = 0) const {
        if (0 == s || 0 == *s) return NOT_FOUND;
        if (offset >= size()) return NOT_FOUND;
        if (0 == count) count = size();
        if (offset + count > size()) count = size() - offset;
        const CharType * p = _ptr + offset;
        for (size_t i = 0; i < count; ++i, ++p) {
            for (const CharType * t = s; *t; ++t) {
                assert(*p && *t);
                if (*p != *t) return offset + i;
            }
        }
        return NOT_FOUND;
    }

    ///
    /// Searches through a string for the first character that matches users predication
    ///
    template<typename PRED>
    size_t findFirstOf(PRED pred, size_t offset = 0, size_t count = 0) const {
        if (offset >= size()) return NOT_FOUND;
        if (0 == count) count = size();
        if (offset + count > size()) count = size() - offset;
        const char * p = _ptr + offset;
        for (size_t i = 0; i < count; ++i, ++p) {
            assert(*p);
            if (pred(*p)) return offset + i;
        }
        return NOT_FOUND;
    }

    ///
    /// Searches through a string for the last character that matches any elements in user specified string
    ///
    size_t findLastOf(const CharType * s, size_t offset = 0, size_t count = 0) const {
        if (0 == s || 0 == *s) return NOT_FOUND;
        if (offset >= size()) return NOT_FOUND;
        if (0 == count) count = size();
        if (offset + count > size()) count = size() - offset;
        assert(count > 0);
        const CharType * p = _ptr + offset + count - 1;
        for (size_t i = count; i > 0; --i, --p) {
            for (const CharType * t = s; *t; ++t) {
                assert(*p && *t);
                if (*p == *t) return offset + i - 1;
            }
        }
        return NOT_FOUND;
    }

    ///
    /// get first character of the string. If string is empty, return 0.
    ///
    CharType first() const { return _ptr[0]; }

    ///
    /// get string caps
    ///
    size_t caps() const {
        if (nullptr == _ptr) {
            return 0;
        } else {
            StringHeader * h = ((StringHeader *) _ptr) - 1;
            return h->caps;
        }
    }

    ///
    /// Insert a character at specific position
    ///
    void insert(size_t pos, CharType ch) {
        if (0 == ch) return;
        if (pos >= size()) {
            append(ch);
        } else {
            setCaps(size() + 1);
            for (size_t i = size() + 1; i > pos; --i) { _ptr[i] = _ptr[i - 1]; }
            _ptr[pos] = ch;
            setSize(size() + 1);
        }
    }

    ///
    /// get last character of the string. If string is empty, return 0.
    ///
    CharType last() const { return size() > 0 ? _ptr[size() - 1] : (CharType) 0; }

    ///
    /// pop out the last charater
    ///
    void pop() {
        if (size() > 0) {
            setSize(size() - 1);
            _ptr[size()] = 0;
        }
    }

    ///
    /// Replace specific character with another
    ///
    void replace(CharType from, CharType to) {
        CharType * p = _ptr;
        for (size_t i = 0; i < size(); ++i, ++p) {
            if (from == *p) *p = to;
        }
    }

    ///
    /// remove specific character at specific location
    ///
    void remove(size_t pos) {
        for (size_t i = pos; i < size(); ++i) { _ptr[i] = _ptr[i + 1]; }
        setSize(size() - 1);
    }

    ///
    /// set string caps
    ///
    void setCaps(size_t newCaps) {
        if (nullptr == _ptr) {
            newCaps          = calcCaps(newCaps);
            _ptr             = allocate(newCaps + 1);
            _ptr[0]          = 0;
            StringHeader * h = ((StringHeader *) _ptr) - 1;
            h->caps          = newCaps;
            h->size          = 0;
        } else if (caps() < newCaps) {
            assert(size() <= caps());

            CharType * oldptr  = _ptr;
            size_t     oldsize = size();

            newCaps = calcCaps(newCaps);
            _ptr    = allocate(newCaps + 1);

            StringHeader * h = ((StringHeader *) _ptr) - 1;
            h->caps          = newCaps;
            h->size          = oldsize;

            ::memcpy(_ptr, oldptr, (oldsize + 1) * sizeof(CharType));

            deallocate(oldptr);
        }
    }

    ///
    /// return string length in character, not including ending zero
    ///
    size_t size() const {
        assert(_ptr);
        StringHeader * h = ((StringHeader *) _ptr) - 1;
        return h->size;
    }

    ///
    /// Get sub string. (0==length) means to the end of original string.
    ///
    void sub(Str & result, size_t offset, size_t length) const {
        if (offset >= size()) {
            result.clear();
            return;
        }
        if (0 == length) length = size();
        if (offset + length > size()) length = size() - offset;
        result.assign(_ptr + offset, length);
    }

    ///
    /// Return sub string
    ///
    Str sub(size_t offset, size_t length) const {
        Str ret;
        sub(ret, offset, length);
        return ret;
    }

    ///
    /// convert to all lower case
    ///
    Str & lower() {
        CHAR * p = _ptr;
        CHAR * e = _ptr + size();
        for (; p < e; ++p) {
            if ('A' <= *p && *p <= 'Z') *p = (*p) - 'A' + 'a';
        }
        return *this;
    }
    ///
    /// convert to all upper case
    ///
    Str & upper() {
        CHAR * p = _ptr;
        CHAR * e = _ptr + size();
        for (; p < e; ++p) {
            if ('a' <= *p && *p <= 'z') *p = (*p) - 'a' + 'A';
        }
        return *this;
    }

    ///
    /// Trim characters for both side
    ///
    Str & trim(const CharType * ch, size_t len = 0) {
        if (0 == ch) return *this;
        if (0 == len) len = length(ch);
        trimRight(ch, len);
        trimLeft(ch, len);
        return *this;
    }

    ///
    /// Trim characters for both side
    ///
    Str & trim(CharType ch) { return trim(&ch, 1); }

    ///
    /// Trim left characters
    ///
    Str & trimLeft(const CharType * ch, size_t len = 0) {
        if (0 == ch) return *this;
        if (0 == len) len = length(ch);
        if (0 == len) return *this;
        CharType * p = _ptr;
        CharType * e = _ptr + size();
        while (p < e) {
            bool equal = false;
            for (size_t i = 0; i < len; ++i) {
                if (*p == ch[i]) {
                    equal = true;
                    break;
                }
            }
            if (!equal) break;
            ++p;
        }
        setSize(e - p);
        for (size_t i = 0; i < size(); ++i) { _ptr[i] = p[i]; }
        _ptr[size()] = 0;
        return *this;
    }

    ///
    /// Trim left characters
    ///
    Str & trimLeft(CharType ch) { return trimLeft(&ch, 1); }

    ///
    /// Trim right characters
    ///
    Str & trimRight(const CharType * ch, size_t len = 0) {
        if (0 == size()) return *this;
        if (0 == ch) return *this;
        if (0 == len) len = length(ch);
        if (0 == len) return *this;
        CharType * p = _ptr + size() - 1;
        while (p >= _ptr) {
            bool equal = false;
            for (size_t i = 0; i < len; ++i) {
                if (*p == ch[i]) {
                    equal = true;
                    break;
                }
            }
            if (!equal) break;
            *p = 0;
            --p;
        }
        setSize(p - _ptr + 1);
        return *this;
    }

    ///
    /// Trim right characters
    ///
    Str & trimRight(CharType ch) { return trimRight(&ch, 1); }

    ///
    /// Trim right characters until meet the predication condition.
    ///
    template<typename PRED>
    Str & trimRightUntil(PRED pred) {
        if (0 == size()) return *this;
        CharType * p = _ptr + size() - 1;
        while (p >= _ptr && !pred(*p)) {
            *p = 0;
            --p;
        }
        setSize(p - _ptr + 1);
        return *this;
    }

    ///
    /// type cast to C string
    ///
    operator const CharType *() const { return _ptr; }

    ///
    /// type cast to C string
    ///
    operator CharType *() { return _ptr; }

    ///
    /// assign operator
    ///
    Str & operator=(const Str & s) {
        assign(s._ptr, s.size());
        return *this;
    }

    ///
    /// assign operator
    ///
    Str & operator=(const CharType * s) {
        assign(s, length(s));
        return *this;
    }

    ///
    /// move operator
    ///
    Str & operator=(Str && s) {
        if (&s != this) {
            deallocate(_ptr);
            _ptr   = s._ptr;
            s._ptr = emptyStr();
        }
        return *this;
    }

    ///
    /// += operator (1)
    ///
    Str & operator+=(const Str & s) {
        append(s);
        return *this;
    }

    ///
    /// += operator (2)
    ///
    Str & operator+=(const CharType * s) {
        append(s, 0);
        return *this;
    }

    ///
    /// += operator (3)
    ///
    Str & operator+=(CharType ch) {
        append(ch);
        return *this;
    }

    ///
    /// += operator (4)
    ///
    Str & operator+=(std::basic_string<CharType> & s) {
        append(s.c_str(), 0);
        return *this;
    }

    ///
    /// equality operator(1)
    ///
    friend bool operator==(const CharType * s1, const Str & s2) { return 0 == compare(s1, s2._ptr); }

    ///
    /// equality operator(2)
    ///
    friend bool operator==(const Str & s1, const CharType * s2) { return 0 == compare(s1._ptr, s2); }

    ///
    /// equality operator(3)
    ///
    friend bool operator==(const Str & s1, const Str & s2) { return 0 == compare(s1._ptr, s2._ptr); }

    ///
    /// equality operator(4)
    ///
    friend bool operator==(const Str & s1, const std::basic_string<CharType> & s2) { return 0 == compare(s1._ptr, s2.c_str()); }

    ///
    /// equality operator(5)
    ///
    friend bool operator==(const std::basic_string<CharType> & s1, const Str & s2) { return s2 == s1; }

    ///
    /// inequality operator(1)
    ///
    friend bool operator!=(const CharType * s1, const Str & s2) { return 0 != compare(s1, s2._ptr); }

    ///
    /// inequality operator(2)
    ///
    friend bool operator!=(const Str & s1, const CharType * s2) { return 0 != compare(s1._ptr, s2); }

    ///
    /// inequality operator(3)
    ///
    friend bool operator!=(const Str & s1, const Str & s2) { return 0 != compare(s1._ptr, s2._ptr); }

    ///
    /// less operator(1)
    ///
    friend bool operator<(const CharType * s1, const Str & s2) { return -1 == compare(s1, s2._ptr); }

    ///
    /// less operator(2)
    ///
    friend bool operator<(const Str & s1, const CharType * s2) { return -1 == compare(s1._ptr, s2); }

    ///
    /// less operator(3)
    ///
    friend bool operator<(const Str & s1, const Str & s2) { return -1 == compare(s1._ptr, s2._ptr); }

    ///
    /// concatenate operator(1)
    ///
    friend Str operator+(const CharType * s1, const Str & s2) {
        Str r(s1);
        r.append(s2);
        return r;
    }

    ///
    /// concatenate operator(2)
    ///
    friend Str operator+(const Str & s1, const CharType * s2) {
        Str r(s1);
        r.append(s2);
        return r;
    }

    ///
    /// concatenate operator(3)
    ///
    friend Str operator+(const Str & s1, const Str & s2) {
        Str r(s1);
        r.append(s2);
        return r;
    }

    ///
    /// concatenate operator(4)
    ///
    friend Str operator+(const Str & s1, const std::basic_string<CharType> & s2) {
        Str r(s1);
        r.append(s2.c_str());
        return r;
    }

    ///
    /// concatenate operator(5)
    ///
    friend Str operator+(const std::basic_string<CharType> & s1, const Str & s2) {
        Str r(s2);
        r.append(s1.c_str());
        return r;
    }

    ///
    /// Output to ostream
    ///
    friend std::ostream & operator<<(std::ostream & os, const Str & str) {
        os << str.data();
        return os;
    }

private:
    struct StringHeader {
        size_t tag;
        size_t caps; //< How many characters can the string hold, not including the null end.
        size_t size; //< How many characters in the string, not including the null end.

        StringHeader(): tag(0), caps(0), size(0) {}
    };

    void setSize(size_t count) {
        assert(_ptr);
        StringHeader * h = ((StringHeader *) _ptr) - 1;
        h->size          = count;
    }

    // align caps to 2^n-1
    size_t calcCaps(size_t count) {
#if defined(_WIN64) || defined(__x86_64__) || defined(__ppc64__)
        count |= count >> 32;
#endif
        count |= count >> 16;
        count |= count >> 8;
        count |= count >> 4;
        count |= count >> 2;
        count |= count >> 1;
        return count;
    }

    // Returns a static pointer for empty string
    static CharType * emptyStr() {
        struct EmptyString : StringHeader {
            CharType terminator;
            EmptyString(): terminator(0) {}
        };
        static EmptyString s;
        assert(0 == s.tag && 0 == s.caps && 0 == s.size);
        return &s.terminator;
    }

    // Allocate a memory buffer that can hold at least 'count' characters, and one extra '\0'.
    static CharType * allocate(size_t count) {
        assert(count > 0);
        if (1 == count) return emptyStr();
        // only allocates raw memory buffer. No constructors are invoked.
        // This is safe, as long as CharType is POD type.
        StringHeader * header = (StringHeader *) ::malloc(sizeof(StringHeader) + sizeof(CharType) * (count + 1));
        header->tag           = 1; // mark it as neee-to-be-freed.
        return (CharType *) (header + 1);
    }

    static void deallocate(CharType * ptr) {
        if (!ptr) return;
        StringHeader * header = ((StringHeader *) ptr) - 1;
        if (0 == header->tag) return;
#ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wfree-nonheap-object"
#endif
        // Freeing memory without calling destructors is safe, as long as CharType is POD type.
        ::free(header);
#ifdef __GNUC__
    #pragma GCC diagnostic pop
#endif
    }

    ///
    /// string comparison (case sensitive)
    ///
    static inline int compare(const CharType * s1, const CharType * s2) {
        if (s1 == s2) return 0;
        if (0 == s1) return -1;
        if (0 == s2) return 1;
        while (*s1 && *s2) {
            if (*s1 < *s2) return -1;
            if (*s1 > *s2) return 1;
            ++s1;
            ++s2;
        }
        if (0 != *s1) return 1;
        if (0 != *s2) return -1;
        return 0;
    }

    ///
    /// our version of strlen() with maxLen limitation.
    ///
    /// if maxLen > 0, then return math::getmin(maxLen,realLength).
    ///
    static inline size_t length(const CharType * s, size_t maxLen = 0) {
        if (0 == s) return 0;
        size_t l = 0;
        if (maxLen > 0) {
            while (0 != *s && l < maxLen) {
                ++l;
                ++s;
            }
        } else {
            while (0 != *s) {
                ++l;
                ++s;
            }
        }
        return l;
    }
};

///
/// multi-byte string class
///
typedef Str<char> StrA;
static_assert(sizeof(StrA) == sizeof(void *), ""); // make sure our string class is the same size of a raw pointer.

///
/// wide-char string class
///
typedef Str<wchar_t> StrW;
static_assert(sizeof(StrW) == sizeof(void *), ""); // make sure our string class is the same size of a raw pointer.

} // namespace rt
} // namespace ph