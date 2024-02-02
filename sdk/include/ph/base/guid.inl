/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

namespace ph {

/// A simple 128-bit integer class. This can also be used to hold a GUID.
/// Can't use full capital name here, since it'll conflict with GUID definition in windows.h
union Guid {
    struct {
        uint64_t lo;
        uint64_t hi;
    };
    uint64_t u64[2];
    uint32_t u32[4];
    uint16_t u16[8];
    uint8_t  u8[16];

    bool operator==(const Guid & rhs) const { return lo == rhs.lo && hi == rhs.hi; }

    bool operator!=(const Guid & rhs) const { return lo != rhs.lo || hi != rhs.hi; }

    bool operator<(const Guid & rhs) const { return (hi != rhs.hi) ? (hi < rhs.hi) : (lo < rhs.lo); }

    static Guid make(uint64_t lo, uint64_t hi) {
        Guid r;
        r.lo = lo;
        r.hi = hi;
        return r;
    }

    /// Make a 128 bit integer from GUID in form of: {aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee}
    static Guid make(uint32_t a, uint16_t b, uint16_t c, uint16_t d, uint64_t e) {
        Guid r;
        r.lo     = e;
        r.u16[3] = d;
        r.u16[4] = c;
        r.u16[5] = b;
        r.u32[3] = a;
        return r;
    }
};

} // namespace ph