namespace ph {

///
/// color format structure
///
union ColorFormat {
    /// color format data members
    //@{
    struct {
#if PH_LITTLE_ENDIAN
        unsigned int layout    : 7;
        unsigned int reserved0 : 1;
        unsigned int sign012   : 4; ///< sign for R/G/B channels
        unsigned int sign3     : 4; ///< sign for alpha channel
        unsigned int swizzle0  : 3;
        unsigned int swizzle1  : 3;
        unsigned int swizzle2  : 3;
        unsigned int swizzle3  : 3;
        unsigned int reserved1 : 4; ///< reserved, must be zero
#else
        unsigned int reserved1 : 4; ///< reserved, must be zero
        unsigned int swizzle3  : 3;
        unsigned int swizzle2  : 3;
        unsigned int swizzle1  : 3;
        unsigned int swizzle0  : 3;
        unsigned int sign3     : 4; ///< sign for alpha channel
        unsigned int sign012   : 4; ///< sign for R/G/B channels
        unsigned int reserved0 : 1;
        unsigned int layout    : 7;
#endif
    };
    uint32_t u32; ///< color format as unsigned integer
    //@}

    ///
    /// color layout
    ///
    enum Layout {
        LAYOUT_UNKNOWN = 0,
        LAYOUT_1,
        LAYOUT_2_2_2_2,
        LAYOUT_3_3_2,
        LAYOUT_4_4,
        LAYOUT_4_4_4_4,
        LAYOUT_5_5_5_1,
        LAYOUT_5_6_5,
        LAYOUT_8,
        LAYOUT_8_8,
        LAYOUT_8_8_8,
        LAYOUT_8_8_8_8,
        LAYOUT_10_11_11,
        LAYOUT_11_11_10,
        LAYOUT_10_10_10_2,
        LAYOUT_16,
        LAYOUT_16_16,
        LAYOUT_16_16_16_16,
        LAYOUT_32,
        LAYOUT_32_32,
        LAYOUT_32_32_32,
        LAYOUT_32_32_32_32,
        LAYOUT_24,
        LAYOUT_8_24,
        LAYOUT_24_8,
        LAYOUT_4_4_24,
        LAYOUT_32_8_24,
        LAYOUT_DXT1,
        LAYOUT_DXT3,
        LAYOUT_DXT3A,
        LAYOUT_DXT5,
        LAYOUT_DXT5A,
        LAYOUT_DXN,
        LAYOUT_CTX1,
        LAYOUT_DXT3A_AS_1_1_1_1,
        LAYOUT_GRGB,
        LAYOUT_RGBG,
        FIRST_ASTC_LAYOUT,

        // all ASTC layout are having 4 channels: RGB + A
        LAYOUT_ASTC_4x4 = FIRST_ASTC_LAYOUT,
        LAYOUT_ASTC_5x4,
        LAYOUT_ASTC_5x5,
        LAYOUT_ASTC_6x5,
        LAYOUT_ASTC_6x6,
        LAYOUT_ASTC_8x5,
        LAYOUT_ASTC_8x6,
        LAYOUT_ASTC_8x8,
        LAYOUT_ASTC_10x5,
        LAYOUT_ASTC_10x6,
        LAYOUT_ASTC_10x8,
        LAYOUT_ASTC_10x10,
        LAYOUT_ASTC_12x10,
        LAYOUT_ASTC_12x12,
        FIRST_ASTC_SFLOAT_LAYOUT,
        LAYOUT_ASTC_4x4_SFLOAT = FIRST_ASTC_SFLOAT_LAYOUT,
        LAYOUT_ASTC_5x4_SFLOAT,
        LAYOUT_ASTC_5x5_SFLOAT,
        LAYOUT_ASTC_6x5_SFLOAT,
        LAYOUT_ASTC_6x6_SFLOAT,
        LAYOUT_ASTC_8x5_SFLOAT,
        LAYOUT_ASTC_8x6_SFLOAT,
        LAYOUT_ASTC_8x8_SFLOAT,
        LAYOUT_ASTC_10x5_SFLOAT,
        LAYOUT_ASTC_10x6_SFLOAT,
        LAYOUT_ASTC_10x8_SFLOAT,
        LAYOUT_ASTC_10x10_SFLOAT,
        LAYOUT_ASTC_12x10_SFLOAT,
        LAYOUT_ASTC_12x12_SFLOAT,
        LAST_ASTC_LAYOUT = LAYOUT_ASTC_12x12_SFLOAT,
        NUM_COLOR_LAYOUTS,
    };

    ///
    /// color channel desc
    ///
    struct ChannelDesc {
        uint8_t shift; ///< bit offset in the pixel
        uint8_t bits;  ///< number of bits of the channel.
    };

    ///
    /// color layout descriptor
    ///
    struct LayoutDesc {
        uint8_t     blockWidth  : 4; ///< width of color block
        uint8_t     blockHeight : 4; ///< heiht of color block
        uint8_t     blockBytes;      ///< bytes of one color block
        uint8_t     numChannels;     ///< number of channels
        ChannelDesc channels[4];     ///< channel descriptors
    };

    ///
    /// Layout descriptors. Indexed by the layout enum value.
    ///
    static constexpr LayoutDesc LAYOUTS[] = {
        // clang-format off
        //BW  BH  BB   CH      CH0        CH1          CH2          CH3
        { 0 , 0 , 0  , 0 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_UNKNOWN,
        { 8 , 1 , 1  , 1 , { { 0 , 1  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_1,
        { 1 , 1 , 1  , 4 , { { 0 , 2  }, { 2  , 2  }, { 4  , 2  }, { 6  , 2  } } }, //LAYOUT_2_2_2_2,
        { 1 , 1 , 1  , 3 , { { 0 , 3  }, { 3  , 3  }, { 8  , 2  }, { 0  , 0  } } }, //LAYOUT_3_3_2,
        { 1 , 1 , 1  , 2 , { { 0 , 4  }, { 4  , 4  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_4_4,
        { 1 , 1 , 2  , 4 , { { 0 , 4  }, { 4  , 4  }, { 8  , 4  }, { 12 , 4  } } }, //LAYOUT_4_4_4_4,
        { 1 , 1 , 2  , 4 , { { 0 , 5  }, { 10 , 5  }, { 15 , 5  }, { 15 , 1  } } }, //LAYOUT_5_5_5_1,
        { 1 , 1 , 2  , 3 , { { 0 , 5  }, { 5  , 6  }, { 11 , 5  }, { 0  , 0  } } }, //LAYOUT_5_6_5,
        { 1 , 1 , 1  , 1 , { { 0 , 8  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_8,
        { 1 , 1 , 2  , 2 , { { 0 , 8  }, { 8  , 8  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_8_8,
        { 1 , 1 , 3  , 3 , { { 0 , 8  }, { 8  , 8  }, { 16 , 8  }, { 0  , 0  } } }, //LAYOUT_8_8_8,
        { 1 , 1 , 4  , 4 , { { 0 , 8  }, { 8  , 8  }, { 16 , 8  }, { 24 , 8  } } }, //LAYOUT_8_8_8_8,
        { 1 , 1 , 4  , 3 , { { 0 , 10 }, { 10 , 11 }, { 21 , 11 }, { 0  , 0  } } }, //LAYOUT_10_11_11,
        { 1 , 1 , 4  , 3 , { { 0 , 11 }, { 11 , 11 }, { 22 , 10 }, { 0  , 0  } } }, //LAYOUT_11_11_10,
        { 1 , 1 , 4  , 4 , { { 0 , 10 }, { 10 , 10 }, { 20 , 10 }, { 30 , 2  } } }, //LAYOUT_10_10_10_2,
        { 1 , 1 , 2  , 1 , { { 0 , 16 }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_16,
        { 1 , 1 , 4  , 2 , { { 0 , 16 }, { 16 , 16 }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_16_16,
        { 1 , 1 , 8  , 4 , { { 0 , 16 }, { 16 , 16 }, { 32 , 16 }, { 48 , 1  } } }, //LAYOUT_16_16_16_16,
        { 1 , 1 , 4  , 1 , { { 0 , 32 }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_32,
        { 1 , 1 , 8  , 2 , { { 0 , 32 }, { 32 , 32 }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_32_32,
        { 1 , 1 , 12 , 3 , { { 0 , 32 }, { 32 , 32 }, { 64 , 32 }, { 0  , 0  } } }, //LAYOUT_32_32_32,
        { 1 , 1 , 16 , 4 , { { 0 , 32 }, { 32 , 32 }, { 64 , 32 }, { 96 , 32 } } }, //LAYOUT_32_32_32_32,
        { 1 , 1 , 3  , 1 , { { 0 , 24 }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_24,
        { 1 , 1 , 4  , 2 , { { 0 , 8  }, { 8  , 24 }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_8_24,
        { 1 , 1 , 4  , 2 , { { 0 , 24 }, { 24 , 8  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_24_8,
        { 1 , 1 , 4  , 4 , { { 0 , 4  }, { 4  , 4  }, { 8  , 24 }, { 0  , 0  } } }, //LAYOUT_4_4_24,
        { 1 , 1 , 8  , 3 , { { 0 , 32 }, { 32 , 8  }, { 40 , 24 }, { 0  , 0  } } }, //LAYOUT_32_8_24,
        { 4 , 4 , 8  , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_DXT1,
        { 4 , 4 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_DXT3,
        { 4 , 4 , 8  , 1 , { { 0 , 4  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_DXT3A,
        { 4 , 4 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_DXT5,
        { 4 , 4 , 8  , 1 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_DXT5A,
        { 4 , 4 , 16 , 2 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_DXN,
        { 4 , 4 , 8  , 2 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_CTX1,
        { 4 , 4 , 8  , 4 , { { 0 , 1  }, { 1  , 1  }, { 2  , 1  }, { 3  , 1  } } }, //LAYOUT_DXT3A_AS_1_1_1_1,
        { 2 , 1 , 4  , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_GRGB,
        { 2 , 1 , 4  , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_RGBG,

        { 4 , 4 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_4x4,
        { 5 , 4 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_5x4,
        { 5 , 5 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_5x5,
        { 6 , 5 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_6x5,
        { 6 , 6 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_6x6,
        { 8 , 5 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_8x5,
        { 8 , 6 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_8x6,
        { 8 , 8 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_8x8,
        { 10, 5 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_10x5,
        { 10, 6 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_10x6,
        { 10, 8 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_10x8,
        { 10, 10, 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_10x10,
        { 12, 10, 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_12x10,
        { 12, 12, 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_12x12,
        { 4 , 4 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_4x4_SFLOAT,
        { 5 , 4 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_5x4_SFLOAT,
        { 5 , 5 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_5x5_SFLOAT,
        { 6 , 5 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_6x5_SFLOAT,
        { 6 , 6 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_6x6_SFLOAT,
        { 8 , 5 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_8x5_SFLOAT,
        { 8 , 6 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_8x6_SFLOAT,
        { 8 , 8 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_8x8_SFLOAT,
        { 10, 5 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_10x5_SFLOAT,
        { 10, 6 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_10x6_SFLOAT,
        { 10, 8 , 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_10x8_SFLOAT,
        { 10, 10, 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_10x10_SFLOAT,
        { 12, 10, 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_12x10_SFLOAT,
        { 12, 12, 16 , 4 , { { 0 , 0  }, { 0  , 0  }, { 0  , 0  }, { 0  , 0  } } }, //LAYOUT_ASTC_12x12_SFLOAT,
        // clang-format on
    };
    static_assert(std::size(LAYOUTS) == NUM_COLOR_LAYOUTS, "LAYOUTs array size is wrong.");
    static_assert(LAYOUTS[LAYOUT_UNKNOWN].blockWidth == 0, "LAYOUT_UNKNOWN's blockWidth must be 0.");

    ///
    /// color sign
    ///
    enum Sign {
        SIGN_UNORM, ///< normalized unsigned integer
        SIGN_SNORM, ///< normalized signed integer
        SIGN_GNORM, ///< normalized gamma integer
        SIGN_SRGB = SIGN_GNORM,
        SIGN_BNORM, ///< normalized bias integer
        SIGN_UINT,  ///< unsigned integer
        SIGN_SINT,  ///< signed integer
        SIGN_GINT,  ///< gamma integer
        SIGN_BINT,  ///< bias integer
        SIGN_FLOAT, ///< float
    };

    ///
    /// color swizzle
    ///
    enum Swizzle {
        SWIZZLE_X = 0,
        SWIZZLE_Y = 1,
        SWIZZLE_Z = 2,
        SWIZZLE_W = 3,
        SWIZZLE_0 = 4,
        SWIZZLE_1 = 5,
    };

    ///
    /// color swizzle for 4 channels
    ///
    enum Swizzle4 {
        SWIZZLE_XYZW = (0 << 0) | (1 << 3) | (2 << 6) | (3 << 9),
        SWIZZLE_ZYXW = (2 << 0) | (1 << 3) | (0 << 6) | (3 << 9),
        SWIZZLE_XYZ1 = (0 << 0) | (1 << 3) | (2 << 6) | (5 << 9),
        SWIZZLE_ZYX1 = (2 << 0) | (1 << 3) | (0 << 6) | (5 << 9),
        SWIZZLE_XXXY = (0 << 0) | (0 << 3) | (0 << 6) | (1 << 9),
        SWIZZLE_XY00 = (0 << 0) | (1 << 3) | (4 << 6) | (4 << 9),
        SWIZZLE_XY01 = (0 << 0) | (1 << 3) | (4 << 6) | (5 << 9),
        SWIZZLE_X000 = (0 << 0) | (4 << 3) | (4 << 6) | (4 << 9),
        SWIZZLE_X001 = (0 << 0) | (4 << 3) | (4 << 6) | (5 << 9),
        SWIZZLE_XXX1 = (0 << 0) | (0 << 3) | (0 << 6) | (5 << 9),
        SWIZZLE_111X = (5 << 0) | (5 << 3) | (5 << 6) | (0 << 9),
    };

    ///
    /// construct from individual properties
    ///
    static constexpr ColorFormat make(Layout l, Sign si012, Sign si3, Swizzle sw0, Swizzle sw1, Swizzle sw2, Swizzle sw3) {
        // clang-format off
        return {{
            (unsigned int)l     & 0x7f,
            0,
            (unsigned int)si012 & 0xf,
            (unsigned int)si3   & 0xf,
            (unsigned int)sw0   & 0x7,
            (unsigned int)sw1   & 0x7,
            (unsigned int)sw2   & 0x7,
            (unsigned int)sw3   & 0x7,
            0,
        }};
        // clang-format on
    }

    ///
    /// construct from individual properties
    ///
    static constexpr ColorFormat make(Layout l, Sign si012, Sign si3, Swizzle4 sw0123) {
        // clang-format off
        return make(
            l,
            si012,
            si3,
            (Swizzle)(((int)sw0123>>0)&3),
            (Swizzle)(((int)sw0123>>3)&3),
            (Swizzle)(((int)sw0123>>6)&3),
            (Swizzle)(((int)sw0123>>9)&3));
        // clang-format on
    }

    ///
    /// construct from individual properties
    ///
    static constexpr ColorFormat make(Layout l, Sign si0123, Swizzle4 sw0123) { return make(l, si0123, si0123, sw0123); }

    ///
    /// check for empty format
    ///
    constexpr bool empty() const { return 0 == layout; }

    ///
    /// self validity check
    ///
    constexpr bool valid() const {
        // clang-format off
        return
            0 < layout && layout < NUM_COLOR_LAYOUTS &&
            sign012  <= SIGN_FLOAT &&
            sign3    <= SIGN_FLOAT &&
            swizzle0 <= SWIZZLE_1 &&
            swizzle1 <= SWIZZLE_1 &&
            swizzle2 <= SWIZZLE_1 &&
            swizzle3 <= SWIZZLE_1 &&
            0 == reserved0 &&
            0 == reserved1;
        // clang-format off
    }

    /// @param pixel The start of the pixel.
    /// @param channel The channel of the pixel we want the value for.
    /// @return The value of the desired channel, normalized to a float between 0 and 1.
    float getPixelChannelFloat(const uint8_t* pixel, size_t channel);

    /// @param pixel The start of the pixel.
    /// @param channel The channel of the pixel we want the value for.
    /// @return The value of the desired channel, normalized to an unsigned byte.
    uint8_t getPixelChannelByte(const uint8_t* pixel, size_t channel);

    ///
    /// get layout descriptor
    ///
    constexpr const LayoutDesc & layoutDesc() const { return LAYOUTS[layout]; }

    ///
    /// Get bytes-per-pixel-block
    ///
    constexpr uint8_t bytesPerBlock() const { return LAYOUTS[layout].blockBytes; }

    ///
    /// convert to bool
    ///
    constexpr operator bool () const { return !empty(); }

    ///
    /// equality check
    ///
    constexpr bool operator==( const ColorFormat & c ) const { return u32 == c.u32; }

    ///
    /// equality check
    ///
    constexpr bool operator!=( const ColorFormat & c ) const { return u32 != c.u32; }

    ///
    /// less operator
    ///
    constexpr bool operator<( const ColorFormat & c ) const { return u32 < c.u32; }

    /// alias of commonly used color formats
    ///
    static constexpr ColorFormat UNKNOWN() { return {}; }

    // clang-format off

    // 8 bits
    static constexpr ColorFormat R_8_UNORM()                   { return make(LAYOUT_8, SIGN_UNORM, SWIZZLE_X001); }
    static constexpr ColorFormat R_8_SNORM()                   { return make(LAYOUT_8, SIGN_SNORM, SWIZZLE_X001); }
    static constexpr ColorFormat L_8_UNORM()                   { return make(LAYOUT_8, SIGN_UNORM, SWIZZLE_XXX1); }
    static constexpr ColorFormat A_8_UNORM()                   { return make(LAYOUT_8, SIGN_UNORM, SWIZZLE_111X); }
    static constexpr ColorFormat RGB_3_3_2_UNORM()             { return make(LAYOUT_3_3_2, SIGN_UNORM, SWIZZLE_XYZ1); }

    // 16 bits
    static constexpr ColorFormat BGRA_4_4_4_4_UNORM()          { return make(LAYOUT_4_4_4_4, SIGN_UNORM, SWIZZLE_ZYXW); }
    static constexpr ColorFormat BGRX_4_4_4_4_UNORM()          { return make(LAYOUT_4_4_4_4, SIGN_UNORM, SWIZZLE_ZYX1); }
    static constexpr ColorFormat BGR_5_6_5_UNORM()             { return make(LAYOUT_5_6_5, SIGN_UNORM, SWIZZLE_ZYX1); }
    static constexpr ColorFormat BGRA_5_5_5_1_UNORM()          { return make(LAYOUT_5_5_5_1, SIGN_UNORM, SWIZZLE_ZYXW); }
    static constexpr ColorFormat BGRX_5_5_5_1_UNORM()          { return make(LAYOUT_5_5_5_1, SIGN_UNORM, SWIZZLE_ZYX1); }

    static constexpr ColorFormat RG_8_8_UNORM()                { return make(LAYOUT_8_8, SIGN_UNORM, SWIZZLE_XY01); }
    static constexpr ColorFormat RG_8_8_SNORM()                { return make(LAYOUT_8_8, SIGN_SNORM, SWIZZLE_XY01); }
    static constexpr ColorFormat LA_8_8_UNORM()                { return make(LAYOUT_8_8, SIGN_UNORM, SWIZZLE_XXXY); }

    static constexpr ColorFormat R_16_UNORM()                  { return make(LAYOUT_16, SIGN_UNORM, SWIZZLE_X001); }
    static constexpr ColorFormat R_16_SNORM()                  { return make(LAYOUT_16, SIGN_SNORM, SWIZZLE_X001); }
    static constexpr ColorFormat R_16_UINT()                   { return make(LAYOUT_16, SIGN_UINT , SWIZZLE_X001); }
    static constexpr ColorFormat R_16_SINT()                   { return make(LAYOUT_16, SIGN_SINT , SWIZZLE_X001); }
    static constexpr ColorFormat R_16_FLOAT()                  { return make(LAYOUT_16, SIGN_FLOAT, SWIZZLE_X001); }

    static constexpr ColorFormat L_16_UNORM()                  { return make(LAYOUT_16, SIGN_UNORM, SWIZZLE_XXX1); }

    // 24 bits

    static constexpr ColorFormat RGB_8_8_8_UNORM()             { return make(LAYOUT_8_8_8, SIGN_UNORM, SWIZZLE_XYZ1); }
    static constexpr ColorFormat RGB_8_8_8_SNORM()             { return make(LAYOUT_8_8_8, SIGN_SNORM, SWIZZLE_XYZ1); }
    static constexpr ColorFormat BGR_8_8_8_UNORM()             { return make(LAYOUT_8_8_8, SIGN_UNORM, SWIZZLE_ZYX1); }
    static constexpr ColorFormat BGR_8_8_8_SNORM()             { return make(LAYOUT_8_8_8, SIGN_SNORM, SWIZZLE_ZYX1); }
    static constexpr ColorFormat R_24_FLOAT()                  { return make(LAYOUT_24, SIGN_FLOAT, SWIZZLE_X001); }

    // 32 bits
    static constexpr ColorFormat RGBA_8_8_8_8_UNORM()          { return make(LAYOUT_8_8_8_8, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat RGBA_8_8_8_8_SRGB()           { return make(LAYOUT_8_8_8_8, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat RGBA_8_8_8_8_SNORM()          { return make(LAYOUT_8_8_8_8, SIGN_SNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat RGBA8()                       { return RGBA_8_8_8_8_UNORM(); }
    static constexpr ColorFormat UBYTE4N()                     { return RGBA_8_8_8_8_UNORM(); }

    static constexpr ColorFormat RGBX_8_8_8_8_UNORM()          { return make(LAYOUT_8_8_8_8, SIGN_UNORM, SWIZZLE_XYZ1); }

    static constexpr ColorFormat BGRA_8_8_8_8_UNORM()          { return make(LAYOUT_8_8_8_8, SIGN_UNORM, SWIZZLE_ZYXW); }
    static constexpr ColorFormat BGRA8()                       { return BGRA_8_8_8_8_UNORM(); }

    static constexpr ColorFormat BGRX_8_8_8_8_UNORM()          { return make(LAYOUT_8_8_8_8, SIGN_UNORM, SWIZZLE_ZYX1); }

    static constexpr ColorFormat RGBA_10_10_10_2_UNORM()       { return make(LAYOUT_10_10_10_2, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat RGBA_10_10_10_2_UINT()        { return make(LAYOUT_10_10_10_2, SIGN_UINT , SWIZZLE_XYZW); }
    static constexpr ColorFormat RGBA_10_10_10_SNORM_2_UNORM() { return make(LAYOUT_10_10_10_2, SIGN_SNORM, SIGN_UNORM, SWIZZLE_XYZW); }

    static constexpr ColorFormat RG_16_16_UNORM()              { return make(LAYOUT_16_16, SIGN_UNORM, SWIZZLE_XY01); }
    static constexpr ColorFormat RG_16_16_SNORM()              { return make(LAYOUT_16_16, SIGN_SNORM, SWIZZLE_XY01); }
    static constexpr ColorFormat RG_16_16_UINT()               { return make(LAYOUT_16_16, SIGN_UINT, SWIZZLE_XY01); }
    static constexpr ColorFormat RG_16_16_SINT()               { return make(LAYOUT_16_16, SIGN_SINT, SWIZZLE_XY01); }
    static constexpr ColorFormat RG_16_16_FLOAT()              { return make(LAYOUT_16_16, SIGN_FLOAT, SWIZZLE_XY01); }
    static constexpr ColorFormat USHORT2N()                    { return RG_16_16_UNORM(); }
    static constexpr ColorFormat SHORT2N()                     { return RG_16_16_SNORM(); }
    static constexpr ColorFormat USHORT2()                     { return RG_16_16_UINT(); }
    static constexpr ColorFormat SHORT2()                      { return RG_16_16_SINT(); }
    static constexpr ColorFormat HALF2()                       { return RG_16_16_FLOAT(); }

    static constexpr ColorFormat LA_16_16_UNORM()              { return make(LAYOUT_16_16, SIGN_UNORM, SWIZZLE_XXXY); }

    static constexpr ColorFormat R_32_UNORM()                  { return make(LAYOUT_32, SIGN_UNORM, SWIZZLE_X001); }
    static constexpr ColorFormat R_32_SNORM()                  { return make(LAYOUT_32, SIGN_SNORM, SWIZZLE_X001); }
    static constexpr ColorFormat R_32_UINT()                   { return make(LAYOUT_32, SIGN_UINT, SWIZZLE_X001); }
    static constexpr ColorFormat R_32_SINT()                   { return make(LAYOUT_32, SIGN_SINT, SWIZZLE_X001); }
    static constexpr ColorFormat R_32_FLOAT()                  { return make(LAYOUT_32, SIGN_FLOAT, SWIZZLE_X001); }
    static constexpr ColorFormat UINT1N()                      { return R_32_UNORM(); }
    static constexpr ColorFormat INT1N()                       { return R_32_SNORM(); }
    static constexpr ColorFormat UINT1()                       { return R_32_UINT(); }
    static constexpr ColorFormat INT1()                        { return R_32_SINT(); }
    static constexpr ColorFormat FLOAT1()                      { return R_32_FLOAT(); }

    static constexpr ColorFormat GR_8_UINT_24_UNORM()          { return make(LAYOUT_8_24, SIGN_UINT, SIGN_UNORM, SWIZZLE_Y, SWIZZLE_X, SWIZZLE_0, SWIZZLE_1); }
    static constexpr ColorFormat GX_8_24_UNORM()               { return make(LAYOUT_8_24, SIGN_UINT, SIGN_UNORM, SWIZZLE_Y, SWIZZLE_0, SWIZZLE_0, SWIZZLE_1); }

    static constexpr ColorFormat RG_24_UNORM_8_UINT()          { return make(LAYOUT_24_8, SIGN_UNORM, SIGN_UINT, SWIZZLE_XY01); }
    static constexpr ColorFormat RX_24_8_UNORM()               { return make(LAYOUT_24_8, SIGN_UNORM, SIGN_UINT, SWIZZLE_XY01); }
    static constexpr ColorFormat XG_24_8_UINT()                { return make(LAYOUT_24_8, SIGN_UNORM, SIGN_UINT, SWIZZLE_0, SWIZZLE_Y, SWIZZLE_0, SWIZZLE_1); }

    static constexpr ColorFormat RG_24_FLOAT_8_UINT()          { return make(LAYOUT_24_8, SIGN_FLOAT, SIGN_UINT, SWIZZLE_XY01); }

    static constexpr ColorFormat GRGB_UNORM()                  { return make(LAYOUT_GRGB, SIGN_UNORM, SWIZZLE_XYZ1); }
    static constexpr ColorFormat RGBG_UNORM()                  { return make(LAYOUT_RGBG, SIGN_UNORM, SWIZZLE_XYZ1); }

    // 64 bits
    static constexpr ColorFormat RGBA_16_16_16_16_UNORM()      { return make(LAYOUT_16_16_16_16, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat RGBA_16_16_16_16_SNORM()      { return make(LAYOUT_16_16_16_16, SIGN_SNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat RGBA_16_16_16_16_UINT()       { return make(LAYOUT_16_16_16_16, SIGN_UINT , SWIZZLE_XYZW); }
    static constexpr ColorFormat RGBA_16_16_16_16_SINT()       { return make(LAYOUT_16_16_16_16, SIGN_SINT , SWIZZLE_XYZW); }
    static constexpr ColorFormat RGBA_16_16_16_16_FLOAT()      { return make(LAYOUT_16_16_16_16, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr ColorFormat USHORT4N()                    { return RGBA_16_16_16_16_UNORM(); }
    static constexpr ColorFormat SHORT4N()                     { return RGBA_16_16_16_16_SNORM(); }
    static constexpr ColorFormat USHORT4()                     { return RGBA_16_16_16_16_UINT(); }
    static constexpr ColorFormat SHORT4()                      { return RGBA_16_16_16_16_SINT(); }
    static constexpr ColorFormat HALF4()                       { return RGBA_16_16_16_16_FLOAT(); }

    static constexpr ColorFormat RGBX_16_16_16_16_UNORM()      { return make(LAYOUT_16_16_16_16, SIGN_UNORM, SWIZZLE_XYZ1); }

    static constexpr ColorFormat RG_32_32_UNORM()              { return make(LAYOUT_32_32, SIGN_UNORM, SWIZZLE_XY01); }
    static constexpr ColorFormat RG_32_32_SNORM()              { return make(LAYOUT_32_32, SIGN_SNORM, SWIZZLE_XY01); }
    static constexpr ColorFormat RG_32_32_UINT()               { return make(LAYOUT_32_32, SIGN_UINT, SWIZZLE_XY01); }
    static constexpr ColorFormat RG_32_32_SINT()               { return make(LAYOUT_32_32, SIGN_SINT, SWIZZLE_XY01); }
    static constexpr ColorFormat RG_32_32_FLOAT()              { return make(LAYOUT_32_32, SIGN_FLOAT, SWIZZLE_XY01); }
    static constexpr ColorFormat FLOAT2()                      { return RG_32_32_FLOAT(); }

    static constexpr ColorFormat RGX_32_FLOAT_8_UINT_24()      { return make(LAYOUT_32_8_24, SIGN_FLOAT, SIGN_UINT, SWIZZLE_XY01); }
    static constexpr ColorFormat RXX_32_8_24_FLOAT()           { return make(LAYOUT_32_8_24, SIGN_FLOAT, SIGN_UINT, SWIZZLE_X001); }
    static constexpr ColorFormat XGX_32_8_24_UINT()            { return make(LAYOUT_32_8_24, SIGN_UINT, SIGN_UINT, SWIZZLE_0, SWIZZLE_Y, SWIZZLE_0, SWIZZLE_1); }

    // 96 bits
    static constexpr ColorFormat RGB_32_32_32_UNORM()          { return make(LAYOUT_32_32_32, SIGN_UNORM, SWIZZLE_XYZ1); }
    static constexpr ColorFormat RGB_32_32_32_SNORM()          { return make(LAYOUT_32_32_32, SIGN_SNORM, SWIZZLE_XYZ1); }
    static constexpr ColorFormat RGB_32_32_32_UINT()           { return make(LAYOUT_32_32_32, SIGN_UINT , SWIZZLE_XYZ1); }
    static constexpr ColorFormat RGB_32_32_32_SINT()           { return make(LAYOUT_32_32_32, SIGN_SINT , SWIZZLE_XYZ1); }
    static constexpr ColorFormat RGB_32_32_32_FLOAT()          { return make(LAYOUT_32_32_32, SIGN_FLOAT, SWIZZLE_XYZ1); }
    static constexpr ColorFormat FLOAT3()                      { return RGB_32_32_32_FLOAT(); }

    // 128 bits
    static constexpr ColorFormat RGBA_32_32_32_32_UNORM()      { return make(LAYOUT_32_32_32_32, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat RGBA_32_32_32_32_SNORM()      { return make(LAYOUT_32_32_32_32, SIGN_SNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat RGBA_32_32_32_32_UINT()       { return make(LAYOUT_32_32_32_32, SIGN_UINT , SWIZZLE_XYZW); }
    static constexpr ColorFormat RGBA_32_32_32_32_SINT()       { return make(LAYOUT_32_32_32_32, SIGN_SINT , SWIZZLE_XYZW); }
    static constexpr ColorFormat RGBA_32_32_32_32_FLOAT()      { return make(LAYOUT_32_32_32_32, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr ColorFormat UINT4N()                      { return RGBA_32_32_32_32_UNORM(); }
    static constexpr ColorFormat SINT4N()                      { return RGBA_32_32_32_32_SNORM(); }
    static constexpr ColorFormat UINT4()                       { return RGBA_32_32_32_32_UINT(); }
    static constexpr ColorFormat SINT4()                       { return RGBA_32_32_32_32_SINT(); }
    static constexpr ColorFormat FLOAT4()                      { return RGBA_32_32_32_32_FLOAT(); }

    // compressed
    static constexpr ColorFormat DXT1_UNORM()                  { return make(LAYOUT_DXT1, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat DXT1_SRGB()                   { return make(LAYOUT_DXT1, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat DXT3_UNORM()                  { return make(LAYOUT_DXT3, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat DXT3_SRGB()                   { return make(LAYOUT_DXT3, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat DXT5_UNORM()                  { return make(LAYOUT_DXT5, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat DXT5_SRGB()                   { return make(LAYOUT_DXT5, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat DXT5A_UNORM()                 { return make(LAYOUT_DXT5A, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat DXT5A_SNORM()                 { return make(LAYOUT_DXT5A, SIGN_SNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat DXN_UNORM()                   { return make(LAYOUT_DXN, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat DXN_SNORM()                   { return make(LAYOUT_DXN, SIGN_SNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_4x4_UNORM()              { return make(LAYOUT_ASTC_4x4, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_4x4_SRGB()               { return make(LAYOUT_ASTC_4x4, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_5x4_UNORM()              { return make(LAYOUT_ASTC_5x4, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_5x4_SRGB()               { return make(LAYOUT_ASTC_5x4, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_5x5_UNORM()              { return make(LAYOUT_ASTC_5x5, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_5x5_SRGB()               { return make(LAYOUT_ASTC_5x5, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_6x5_UNORM()              { return make(LAYOUT_ASTC_6x5, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_6x5_SRGB()               { return make(LAYOUT_ASTC_6x5, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_6x6_UNORM()              { return make(LAYOUT_ASTC_6x6, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_6x6_SRGB()               { return make(LAYOUT_ASTC_6x6, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_8x5_UNORM()              { return make(LAYOUT_ASTC_8x5, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_8x5_SRGB()               { return make(LAYOUT_ASTC_8x5, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_8x6_UNORM()              { return make(LAYOUT_ASTC_8x6, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_8x6_SRGB()               { return make(LAYOUT_ASTC_8x6, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_8x8_UNORM()              { return make(LAYOUT_ASTC_8x8, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_8x8_SRGB()               { return make(LAYOUT_ASTC_8x8, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_10x5_UNORM()             { return make(LAYOUT_ASTC_10x5, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_10x5_SRGB()              { return make(LAYOUT_ASTC_10x5, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_10x6_UNORM()             { return make(LAYOUT_ASTC_10x6, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_10x6_SRGB()              { return make(LAYOUT_ASTC_10x6, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_10x8_UNORM()             { return make(LAYOUT_ASTC_10x8, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_10x8_SRGB()              { return make(LAYOUT_ASTC_10x8, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_10x10_UNORM()            { return make(LAYOUT_ASTC_10x10, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_10x10_SRGB()             { return make(LAYOUT_ASTC_10x10, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_12x10_UNORM()            { return make(LAYOUT_ASTC_12x10, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_12x10_SRGB()             { return make(LAYOUT_ASTC_12x10, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_12x12_UNORM()            { return make(LAYOUT_ASTC_12x12, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_12x12_SRGB()             { return make(LAYOUT_ASTC_12x12, SIGN_GNORM, SIGN_UNORM, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_4x4_SFLOAT()              { return make(LAYOUT_ASTC_4x4_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_5x4_SFLOAT()              { return make(LAYOUT_ASTC_5x4_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_5x5_SFLOAT()              { return make(LAYOUT_ASTC_5x5_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_6x5_SFLOAT()              { return make(LAYOUT_ASTC_6x5_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_6x6_SFLOAT()              { return make(LAYOUT_ASTC_6x6_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_8x5_SFLOAT()              { return make(LAYOUT_ASTC_8x5_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_8x6_SFLOAT()              { return make(LAYOUT_ASTC_8x6_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_8x8_SFLOAT()              { return make(LAYOUT_ASTC_8x8_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_10x5_SFLOAT()             { return make(LAYOUT_ASTC_10x5_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_10x6_SFLOAT()             { return make(LAYOUT_ASTC_10x6_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_10x8_SFLOAT()             { return make(LAYOUT_ASTC_10x8_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_10x10_SFLOAT()            { return make(LAYOUT_ASTC_10x10_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_12x10_SFLOAT()            { return make(LAYOUT_ASTC_12x10_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }
    static constexpr ColorFormat ASTC_12x12_SFLOAT()            { return make(LAYOUT_ASTC_12x12_SFLOAT, SIGN_FLOAT, SWIZZLE_XYZW); }

    // clang-format on
};
static_assert(4 == sizeof(ColorFormat), "size of ColorFormat must be 4");
static_assert(ColorFormat::UNKNOWN().layoutDesc().blockWidth == 0, "UNKNOWN format's block width must be 0");
static_assert(!ColorFormat::UNKNOWN().valid(), "UNKNOWN format must be invalid.");
static_assert(ColorFormat::UNKNOWN().empty(), "UNKNOWN format must be empty.");
static_assert(ColorFormat::RGBA8().valid(), "RGBA8 format must be valid.");
static_assert(!ColorFormat::RGBA8().empty(), "RGBA8 format must _NOT_ be empty.");
static_assert(ColorFormat::ASTC_12x12_SFLOAT().valid(), "ASTC_12x12_SFLOAT must be a valid format.");

// Helper constexpr functions
namespace BitFieldDetails {
template<typename T>
constexpr auto getBitsCount(T data, size_t startBit = 0) -> typename std::enable_if<std::is_unsigned<T>::value, size_t>::type {
    return (startBit == sizeof(T) * 8) ? 0 : getBitsCount(data, startBit + 1) + ((data & (1ull << startBit)) ? 1 : 0);
}

// We should support unsigned enums too
template<typename T>
constexpr auto getBitsCount(T data, size_t startBit = 0) -> typename std::enable_if<std::is_enum<T>::value, size_t>::type {
    return (startBit == sizeof(T) * 8)
               ? 0
               : getBitsCount(data, startBit + 1) + ((static_cast<typename std::underlying_type<T>::type>(data) & (1ull << startBit)) ? 1 : 0);
}

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wbitfield-constant-conversion"
    #pragma clang diagnostic ignored "-Wmissing-braces"
#endif
#ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Woverflow"
#endif
#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4463) // overflow
#endif
template<typename StructType, typename StructFieldsType, StructFieldsType... initVals>
constexpr StructType getFilledStructImpl(StructType s, ...) {
    return s;
}

template<typename StructType, typename StructFieldsType, StructFieldsType... initVals>
constexpr StructType getFilledStructImpl(StructType, decltype(StructType {initVals...}, int()) = 0) {
    // can you numeric_limits::max() here instead of -1, but it will require casting to and from underlying type for enum
    return getFilledStructImpl<StructType, StructFieldsType, initVals..., static_cast<StructFieldsType>(-1)>(StructType {initVals...}, 0);
}

template<typename StructType, typename StructFieldsType>
constexpr StructType getFilledStruct() {
    using UnderlyingStructType =
        typename std::conditional<std::is_enum<StructFieldsType>::value, std::underlying_type<StructFieldsType>, std::remove_cv<StructFieldsType>>::type::type;
    static_assert(std::is_unsigned<UnderlyingStructType>::value, "Bit field calculation only works with unsigned values");
    return getFilledStructImpl<StructType, StructFieldsType>(StructType {}, 0);
}
#ifdef __GNUC__
    #pragma GCC diagnostic pop
#endif
#ifdef __clang__
    #pragma clang diagnostic pop
#endif
#ifdef _MSC_VER
    #pragma warning(pop)
#endif
} // namespace BitFieldDetails

// Get bit field size at compile time
#define PH_GET_BIT_FIELD_WIDTH(structType, fieldName) \
    BitFieldDetails::getBitsCount(BitFieldDetails::getFilledStruct<structType, decltype(structType {}.fieldName)>().fieldName)

// Make sure that all layouts can fit into the ColorFormat::layout field. If this asset failed, then you have more layouts defined
// then the ColorFormat::layout field can hold. In this case, you either need to delete some layouts, or increase size of
// ColorFormat::layout field.
static_assert(ColorFormat::NUM_COLOR_LAYOUTS < (1u << PH_GET_BIT_FIELD_WIDTH(ColorFormat, layout)), "");

///
/// compose RGBA8 color constant
///
template<typename T>
inline constexpr uint32_t makeRGBA8(T r, T g, T b, T a) {
    // clang-format off
    return (
        ( ((uint32_t)(r)&0xFF) <<  0 ) |
        ( ((uint32_t)(g)&0xFF) <<  8 ) |
        ( ((uint32_t)(b)&0xFF) << 16 ) |
        ( ((uint32_t)(a)&0xFF) << 24 ) );
    // clang-format on
}

///
/// compose BGRA8 color constant
///
template<typename T>
inline constexpr uint32_t makeBGRA8(T r, T g, T b, T a) {
    // clang-format off
    return (
        ( ((uint32_t)(b)&0xFF) <<  0 ) |
        ( ((uint32_t)(g)&0xFF) <<  8 ) |
        ( ((uint32_t)(r)&0xFF) << 16 ) |
        ( ((uint32_t)(a)&0xFF) << 24 ) );
    // clang-format on
}

class RawImage;

union RGBA8 {
    struct {
        uint8_t x, y, z, w;
    };
    struct {
        uint8_t r, g, b, a;
    };
    uint32_t u32;
    uint8_t  u8[4];

    static RGBA8 make(uint8_t r, uint8_t g = 0, uint8_t b = 0, uint8_t a = 0) { return {{r, g, b, a}}; }

    static RGBA8 make(const uint8_t * p) { return {{p[0], p[1], p[2], p[3]}}; }

    static RGBA8 make(uint32_t u) {
        RGBA8 result;
        result.u32 = u;
        return result;
    }

    void set(uint8_t r_, uint8_t g_ = 0, uint8_t b_ = 0, uint8_t a_ = 0) {
        r = r_;
        g = g_;
        b = b_;
        a = a_;
    }
};
static_assert(sizeof(RGBA8) == 4, "");

union float4 {
    struct {
        float x, y, z, w;
    };
    struct {
        float r, g, b, a;
    };
    uint32_t u32[4];
    uint32_t f32[4];
    uint64_t u64[2];

    static float4 make(float r, float g, float b, float a) { return {{r, g, b, a}}; }

    static float4 make(const float * p) { return {{p[0], p[1], p[2], p[3]}}; }
};
static_assert(sizeof(float4) == 128 / 8, "");

/// This represents a single 1D/2D/3D image in an more complex image structure.
/// Note: avoid using size_t in this structure. So the size of the structure will never change,
/// regardless of compile platform.
struct ImagePlaneDesc {

    /// pixel format
    ColorFormat format = ColorFormat::UNKNOWN();

    /// Plane width in pixels
    uint32_t width = 0;

    /// Plane height in pixels
    uint32_t height = 0;

    /// Plane depth in pixels
    uint32_t depth = 0;

    /// bytes (not BITS) from one pixel or pixel block to the next. Minimal valid value is pixel or pixel block size.
    uint32_t step = 0;

    /// Bytes from one row to next. Minimal valid value is (width * step) and aligned to pixel boundary.
    /// For compressed format, this is the number of bytes in one block row.
    uint32_t pitch = 0;

    /// Bytes from one slice to next. Minimal valid value is (pitch * height)
    uint32_t slice = 0;

    /// Bytes of the whole plane. Minimal valid value is (slice * depth)
    uint32_t size = 0;

    /// Bytes between first pixel of the plane to the first pixel of the whole image.
    uint32_t offset = 0;

    // /// Memory alignment requirement of the plane. The value must be power of 2.
    // uint32_t alignment = 0;

    /// returns offset from start of data buffer for a particular pixel within the plane
    size_t pixel(size_t x, size_t y, size_t z = 0) const {
        PH_ASSERT(x < width && y < height && z < depth);
        auto & ld = format.layoutDesc();
        PH_ASSERT((x % ld.blockWidth) == 0 && (y % ld.blockHeight) == 0);
        size_t r = z * slice + y / ld.blockHeight * pitch + x / ld.blockWidth * step;
        PH_ASSERT(r < size);
        return r + offset;
    }

    /// check if this is a valid image plane descriptor. Note that valid descriptor is never empty.
    bool valid() const;

    /// check if this is an empty descriptor. Note that empty descriptor is never valid.
    bool empty() const { return ColorFormat::UNKNOWN() == format; }

    bool operator==(const ImagePlaneDesc & rhs) const {
        // clang-format off
        return format == rhs.format
            && width == rhs.width
            && height == rhs.height
            && depth == rhs.depth
            && step == rhs.step
            && pitch == rhs.pitch
            && slice == rhs.slice
            && size == rhs.size
            && offset == rhs.offset;
        // clang-format on
    }

    bool operator!=(const ImagePlaneDesc & rhs) const { return !operator==(rhs); }

    bool operator<(const ImagePlaneDesc & rhs) const {
        if (format != rhs.format) return format < rhs.format;
        if (width != rhs.width) return width < rhs.width;
        if (height != rhs.height) return height < rhs.height;
        if (depth != rhs.depth) return depth < rhs.depth;
        if (step != rhs.step) return step < rhs.step;
        if (pitch != rhs.pitch) return pitch < rhs.pitch;
        if (slice != rhs.slice) return slice < rhs.slice;
        if (size != rhs.size) return size < rhs.size;
        return offset < rhs.offset;
    }

    /// Create a new image plane descriptor
    static ImagePlaneDesc make(ColorFormat format, size_t width, size_t height = 1, size_t depth = 1, size_t step = 0, size_t pitch = 0, size_t slice = 0);

    /// Save the image plane to PNG stream. This method only supports 8-bit and 16-bit 2D image.
    /// \param stream  Target stream
    /// \param pixels  The pixel array. The buffer length should be no less than ImagePlaneDesc::size.
    ///                Or else, the behavior is undefined.
    /// \param z       Specify the z slice to save (only applied to 3D image)
    void saveToPNG(std::ostream & stream, const void * pixels) const;

    /// Save the image plane to PNG file.
    template<class... ARGS>
    void saveToPNG(const std::string & filename, ARGS... args) const {
        auto s = openFileStream(filename);
        saveToPNG(s, args...);
    }

    /// Save the image plane to JPG stream. Only supports 8-bit and 16-bit 2D image.
    /// \param stream   Target stream
    /// \param pixels   The pixel array The buffer length should be no less than ImagePlaneDesc::size.
    /// \param z       Specify the z slice to save (only applied to 3D image)
    /// \param quality  Compression quality. Valid range is [1, 100];
    void saveToJPG(std::ostream & stream, const void * pixels, int quality = 100) const;

    /// Save the image plane to JPB file. Only supports 8-bit and 16-bit 2D image.
    template<class... ARGS>
    void saveToJPG(std::string & filename, ARGS... args) const {
        auto s = openFileStream(filename);
        saveToJPG(s, args...);
    }

    /// Save the image to .HDR stream. This method will try convert everything to float4
    void saveToHDR(std::ostream & stream, const void * pixels) const;

    /// Save the image plane to .HDR file.
    template<class... ARGS>
    void saveToHDR(std::string & filename, ARGS... args) const {
        auto s = openFileStream(filename);
        saveToHDR(s, args...);
    }

    /// Save the image to .RAW stream.
    void saveToRAW(std::ostream & stream, const void * pixels) const;

    // Save the image as raw file
    template<class... ARGS>
    void saveToRAW(std::string & filename, ARGS... args) const {
        auto s = openFileStream(filename);
        saveToHDR(s, args...);
    }

    /// A general save function. Use extension to determine file format.
    void save(const std::string & filename, const void * pixels) const;

    /// Convert the specified slice of the image plane to float4 format.
    /// \return Pixel data in float4 format.
    std::vector<float4> toFloat4(const void * src) const;

    /// Convert the specified slice of the image plane to R8G8B8A8_UNORM format.
    /// \return Pixel data in R8G8B8A8_UNORM format.
    std::vector<RGBA8> toRGBA8(const void * src) const;

    /// Load data to specific slice of image plane from float4 data.
    void fromFloat4(void * dst, size_t dstSize, size_t dstZ, const void * src) const;

    RawImage generateMipmaps(const void * pixels) const;

private:
    static std::ofstream openFileStream(const std::string & filename) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            PH_LOGE("failed to open file %s for writing.", filename.c_str());
            return {};
        }
        return file;
    }
};

///
/// Represent a complex image with optional mipmap chain
///
struct ImageDesc {

    // ****************************
    /// \name member data
    // ****************************

    //@{

    std::vector<ImagePlaneDesc> planes;     ///< length of array = layers * mips;
    uint32_t                    layers = 0; ///< number of layers
    uint32_t                    levels = 0; ///< number of mipmap levels
    uint32_t                    size   = 0; ///< total size in bytes;

    //@}

    // ****************************
    /// \name ctor/dtor/copy/move
    // ****************************

    //@{

    /// @brief  Default constructor
    ImageDesc() = default;

    /// Defines how pixels packed in memory.
    /// Note that this only affects how plane offset are calculated. The 'planes' data member in this structure
    /// is always indexed in mip level major fashion.
    enum ConstructionOrder {
        /// In this mode, pixels from same mipmap level are packed together. For example, given a cubemap with
        /// 6 faces and 3 mipmap levels, the pixels are packed in memory in this order:
        ///
        ///     face 0, mip 0
        ///     face 1, mip 0
        ///     ...
        ///     face 5, mip 0
        ///
        ///     face 0, mip 1
        ///     face 1, mip 1
        ///     ...
        ///     face 5, mip 1
        ///
        ///     face 0, mip 2
        ///     face 1, mip 2
        ///     ...
        ///     face 5, mip 2
        MIP_MAJOR,

        /// In this mode, pixels from same face are packed together. For example, given a cubemap with 6 faces
        /// and 3 mipmap levels, the offsets are calculated in this order:
        ///
        ///     face 0, mip 0
        ///     face 0, mip 1
        ///     face 0, mip 2
        ///
        ///     face 1, mip 0
        ///     face 1, mip 1
        ///     face 1, mip 2
        ///     ...
        ///     face 5, mip 0
        ///     face 5, mip 1
        ///     face 5, mip 2
        ///
        /// Note: this is the order used by DDS file.
        FACE_MAJOR,
    };

    /// Construct image descriptor from base map and layer/level count. If anything goes wrong,
    /// construct an empty image descriptor.
    ///
    /// \param baseMap the base image
    /// \param layers number of layers. must be positive integer
    /// \param levels number of mipmap levels. set to 0 to automatically build full mipmap chain.
    ImageDesc(const ImagePlaneDesc & baseMap, size_t layers = 1, size_t levels = 1, ConstructionOrder order = MIP_MAJOR) {
        reset(baseMap, layers, levels, order);
    }

    // can copy.
    PH_DEFAULT_COPY(ImageDesc);

    /// move constructor
    ImageDesc(ImageDesc && rhs) {
        planes = std::move(rhs.planes);
        PH_ASSERT(rhs.planes.empty());
        layers     = rhs.layers;
        rhs.layers = 0;
        levels     = rhs.levels;
        rhs.levels = 0;
        size       = rhs.size;
        rhs.size   = 0;
        PH_ASSERT(rhs.empty());
    }

    /// move operator
    ImageDesc & operator=(ImageDesc && rhs) {
        if (this != &rhs) {
            planes = std::move(rhs.planes);
            PH_ASSERT(rhs.planes.empty());
            layers     = rhs.layers;
            rhs.layers = 0;
            levels     = rhs.levels;
            rhs.levels = 0;
            size       = rhs.size;
            rhs.size   = 0;
            PH_ASSERT(rhs.empty());
        }
        return *this;
    }

    /// Reset to empty image
    ImageDesc & clear() {
        planes.clear();
        layers = 0;
        levels = 0;
        size   = 0;
        return *this;
    }

    /// reset the descriptor
    ImageDesc & reset(const ImagePlaneDesc & baseMap, size_t layers, size_t levels, ConstructionOrder order);

    /// @brief Set to simple 2D Image
    /// @param format   Pixel format
    /// @param width    Width of the image in pixels
    /// @param height   Height of the image in pixels
    /// @param levels   Mipmap count. Set to 0 to automatically generate full mipmap chain.
    ImageDesc & set2D(ColorFormat format, size_t width, size_t height = 1, size_t levels = 1, ConstructionOrder order = MIP_MAJOR);

    /// @brief Set to simple 2D Image
    /// @param format   Pixel format
    /// @param width    Width and height of the image in pixels
    /// @param levels   Mipmap count. Set to 0 to automatically generate full mipmap chain.
    ImageDesc & setCube(ColorFormat format, size_t width, size_t levels = 1, ConstructionOrder order = MIP_MAJOR);

    //@}

    // ****************************
    /// \name public methods
    // ****************************

    //@{

    ///
    /// check if the image is empty or not
    ///
    bool empty() const { return planes.empty(); }

    ///
    /// make sure this is a meaningful image descriptor
    ///
    bool valid() const;

    /// methods to return properties of the specific plane.
    //@{
    // clang-format off
    const ImagePlaneDesc & plane (size_t layer = 0, size_t level = 0) const { return planes[index(layer, level)]; }
    ImagePlaneDesc       & plane (size_t layer = 0, size_t level = 0)       { return planes[index(layer, level)]; }
    ColorFormat            format(size_t layer = 0, size_t level = 0) const { return planes[index(layer, level)].format; }
    uint32_t               width (size_t layer = 0, size_t level = 0) const { return planes[index(layer, level)].width; }
    uint32_t               height(size_t layer = 0, size_t level = 0) const { return planes[index(layer, level)].height; }
    uint32_t               depth (size_t layer = 0, size_t level = 0) const { return planes[index(layer, level)].depth; }
    uint32_t               step  (size_t layer = 0, size_t level = 0) const { return planes[index(layer, level)].step; }
    uint32_t               pitch (size_t layer = 0, size_t level = 0) const { return planes[index(layer, level)].pitch; }
    uint32_t               slice (size_t layer = 0, size_t level = 0) const { return planes[index(layer, level)].slice; }
    // clang-format on
    //@}

    ///
    /// returns offset of particular pixel
    ///
    size_t pixel(size_t layer, size_t level, size_t x = 0, size_t y = 0, size_t z = 0) const {
        const auto & d = planes[index(layer, level)];
        auto         r = d.pixel(x, y, z);
        PH_ASSERT(r < size);
        return r;
    }

    // void vertFlipInpace(void * pixels, size_t sizeInBytes);

    bool operator==(const ImageDesc & rhs) const { return planes == rhs.planes && layers == rhs.layers && levels == rhs.levels && size == rhs.size; }

    bool operator!=(const ImageDesc & rhs) const { return !operator==(rhs); }

    bool operator<(const ImageDesc & rhs) const {
        if (layers != rhs.layers) return layers < rhs.layers;
        if (levels != rhs.levels) return levels < rhs.levels;
        if (size != rhs.size) return size < rhs.size;
        if (planes.size() != rhs.planes.size()) return planes.size() < rhs.planes.size();
        for (size_t i = 0; i < planes.size(); ++i) {
            const auto & a = planes[i];
            const auto & b = rhs.planes[i];
            if (a != b) return a < b;
        }
        return false;
    }

private:
    /// return plane index
    size_t index(size_t layer, size_t level) const {
        PH_ASSERT(layer < layers);
        PH_ASSERT(level < levels);
        return (level * layers) + layer;
    }
};

/// Image descriptor combined with a pointer to pixel array. This is a convenient helper class for passing image
/// data around w/o actually copying pixel data array.
struct ImageProxy {
    ImageDesc desc;           ///< the image descriptor
    uint8_t * data = nullptr; ///< the image data (pixel array)

    /// return size of the whole image in bytes.
    uint32_t size() const { return desc.size; }

    /// check if the image is empty or not.
    bool empty() const { return desc.empty(); }

    /// \name query properties of the specific plane.
    //@{
    // clang-format off
    ColorFormat format(size_t layer = 0, size_t level = 0) const { return desc.plane(layer, level).format; }
    uint32_t    width (size_t layer = 0, size_t level = 0) const { return desc.plane(layer, level).width; }
    uint32_t    height(size_t layer = 0, size_t level = 0) const { return desc.plane(layer, level).height; }
    uint32_t    depth (size_t layer = 0, size_t level = 0) const { return desc.plane(layer, level).depth; }
    uint32_t    step  (size_t layer = 0, size_t level = 0) const { return desc.plane(layer, level).step; }
    uint32_t    pitch (size_t layer = 0, size_t level = 0) const { return desc.plane(layer, level).pitch; }
    uint32_t    slice (size_t layer = 0, size_t level = 0) const { return desc.plane(layer, level).slice; }
    // clang-format on
    //@}

    /// return pointer to particular pixel
    const uint8_t * pixel(size_t layer, size_t level, size_t x = 0, size_t y = 0, size_t z = 0) const { return data + desc.pixel(layer, level, x, y, z); }

    /// return pointer to particular pixel
    uint8_t * pixel(size_t layer, size_t level, size_t x = 0, size_t y = 0, size_t z = 0) { return data + desc.pixel(layer, level, x, y, z); }
};

///
/// A basic image class
///
class RawImage {

public:
    /// \name ctor/dtor/copy/move
    //@{
    PH_NO_COPY(RawImage);
    RawImage() = default;
    RawImage(ImageDesc && desc, const void * initialContent = nullptr, size_t initialContentSizeInbytes = 0);
    RawImage(const ImageDesc & desc, const void * initialContent = nullptr, size_t initialContentSizeInbytes = 0);
    RawImage(const ImageDesc & desc, const ConstRange<uint8_t> & initialContent);
    RawImage(RawImage && rhs) {
        _proxy.desc = std::move(rhs._proxy.desc);
        PH_ASSERT(rhs._proxy.desc.empty());
        _proxy.data     = rhs._proxy.data;
        rhs._proxy.data = nullptr;
    }
    ~RawImage() { clear(); }
    RawImage & operator=(RawImage && rhs) {
        if (this != &rhs) {
            _proxy.desc = std::move(rhs._proxy.desc);
            PH_ASSERT(rhs._proxy.desc.empty());
            _proxy.data     = rhs._proxy.data;
            rhs._proxy.data = nullptr;
        }
        return *this;
    }
    //@}

    /// \name basic property query
    //@{

    /// return proxy of the image.
    const ImageProxy & proxy() const { return _proxy; }

    /// return descriptor of the whole image
    const ImageDesc & desc() const { return _proxy.desc; }

    /// return descriptor of a image plane
    const ImagePlaneDesc & desc(size_t layer, size_t level) const { return _proxy.desc.plane(layer, level); }

    /// return pointer to pixel buffer.
    const uint8_t * data() const { return _proxy.data; }

    /// return pointer to pixel buffer.
    uint8_t * data() { return _proxy.data; }

    /// return size of the whole image in bytes.
    uint32_t size() const { return _proxy.desc.size; }

    /// check if the image is empty or not.
    bool empty() const { return _proxy.desc.empty(); }

    //@}

    /// \name query properties of the specific plane.
    //@{
    // clang-format off
    ColorFormat format(size_t layer = 0, size_t level = 0) const { return _proxy.desc.plane(layer, level).format; }
    uint32_t    width (size_t layer = 0, size_t level = 0) const { return _proxy.desc.plane(layer, level).width; }
    uint32_t    height(size_t layer = 0, size_t level = 0) const { return _proxy.desc.plane(layer, level).height; }
    uint32_t    depth (size_t layer = 0, size_t level = 0) const { return _proxy.desc.plane(layer, level).depth; }
    uint32_t    step  (size_t layer = 0, size_t level = 0) const { return _proxy.desc.plane(layer, level).step; }
    uint32_t    pitch (size_t layer = 0, size_t level = 0) const { return _proxy.desc.plane(layer, level).pitch; }
    uint32_t    slice (size_t layer = 0, size_t level = 0) const { return _proxy.desc.plane(layer, level).slice; }
    // clang-format on
    //@}

    /// reset to an empty image
    void clear();

    /// make a clone of the current image.
    RawImage clone() const { return RawImage(desc(), data()); }

    /// Save certain plane to disk file.
    void save(const std::string & filename, size_t layer, size_t level) const { desc().plane(layer, level).save(filename, proxy().pixel(layer, level)); }

    /// \name Image loading utilities
    //@{
    /// Helper method to load from a binary stream.
    static RawImage load(std::istream &);

    /// Helper method to load from a binary byte arry in memory.
    static RawImage load(const ConstRange<uint8_t> &);

    /// Helper method to load from a file.
    static RawImage load(const std::string &);

private:
    ImageProxy _proxy;

private:
    void construct(const void * initialContent, size_t initialContentSizeInbytes);
};

} // end of namespace ph

namespace std {

/// A functor that allows for hashing image image plane desc.
template<>
struct hash<ph::ImagePlaneDesc> {
    size_t operator()(const ph::ImagePlaneDesc & key) const {
        // Records the calculated hash of the texture handle.
        size_t hash = 7;

        // Hash the members.
        hash = 79 * hash + (size_t) key.format;
        hash = 79 * hash + (size_t) key.width;
        hash = 79 * hash + (size_t) key.height;
        hash = 79 * hash + (size_t) key.depth;
        hash = 79 * hash + (size_t) key.step;
        hash = 79 * hash + (size_t) key.pitch;
        hash = 79 * hash + (size_t) key.slice;
        hash = 79 * hash + (size_t) key.size;
        hash = 79 * hash + (size_t) key.offset;

        return hash;
    }
};

/// A functor that allows for hashing image descriptions.
template<>
struct hash<ph::ImageDesc> {
    size_t operator()(const ph::ImageDesc & key) const {
        // Records the calculated hash of the texture handle.
        size_t hash = 7;

        // Hash the members.
        hash = 79 * hash + (size_t) key.layers;
        hash = 79 * hash + (size_t) key.levels;
        hash = 79 * hash + (size_t) key.size;

        /// Allows us to hash the planes.
        std::hash<ph::ImagePlaneDesc> planeHasher;

        // Calculate the hash of the planes.
        for (const ph::ImagePlaneDesc & plane : key.planes) { hash = 79 * hash + planeHasher(plane); }

        return hash;
    }
};

} // end of namespace std
