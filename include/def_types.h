/// @file       def_types.h
/// @brief      Provides macros and structures for bit manipulation, data extraction,
///             and endian handling. See Doxygen docs in docs/ for details.
/// @author     KIM JEONGGI (jgkim12@digitech.kr)

#ifndef _DEF_TYPES_H_
#define _DEF_TYPES_H_

#include <stdint.h>
#include <string.h>

// -----------------------------------------------------------------------------
// Constants
// -----------------------------------------------------------------------------

/// @defgroup constants Constants
/// @brief General-purpose constants

/// Boolean true value
#define F_TRUE    1
#define F_SUCCESS 1

/// Boolean false value
#define F_FALSE   0
#define F_FAILURE 0

/// floating-point epsilon values for comparisons
#define C_FLOAT_EPSILON  0.000001f      /// Epsilon for floating-point comparisons
#define C_DOUBLE_EPSILON 0.000000000001 ///< Epsilon for double comparisons

#if !defined(USE_FLOAT_EPS) && !defined(USE_DOUBLE_EPS)
#define USE_FLOAT_EPS
#endif

#ifdef USE_FLOAT_EPS
#include <math.h>       /// For fabsf() function
static inline int FECMP_E(float a, float b) { return fabsf(a - b) < C_FLOAT_EPSILON; }  /// Compare two floats for equality with epsilon
static inline int FECMP_G(float a, float b) { return a > (b + C_FLOAT_EPSILON); } /// Compare if a is greater than b with epsilon
static inline int FECMP_L(float a, float b) { return a < (b - C_FLOAT_EPSILON); } /// Compare if a is less than b with epsilon
static inline int FECMP_GE(float a, float b) { return a >= (b - C_FLOAT_EPSILON); } /// Compare if a is greater than or equal to b with epsilon
static inline int FECMP_LE(float a, float b) { return a <= (b + C_FLOAT_EPSILON); } /// Compare if a is less than or equal to b with epsilon
#else
#include <math.h>       /// For fabsf() function
static inline int FECMP_E(float a, float b) { return fabsf(a - b) < C_DOUBLE_EPSILON; } /// Compare two floats for equality with epsilon
static inline int FECMP_G(float a, float b) { return a > (b + C_DOUBLE_EPSILON); } /// Compare if a is greater than b with epsilon
static inline int FECMP_L(float a, float b) { return a < (b - C_DOUBLE_EPSILON); } /// Compare if a is less than b with epsilon
static inline int FECMP_GE(float a, float b) { return a >= (b - C_DOUBLE_EPSILON); } /// Compare if a is greater than or equal to b with epsilon
static inline int FECMP_LE(float a, float b) { return a <= (b + C_DOUBLE_EPSILON); } /// Compare if a is less than or equal to b with epsilon
#endif

// -----------------------------------------------------------------------------
// Bit Manipulation Macros
// -----------------------------------------------------------------------------

/// @defgroup bit_manipulation Bit Manipulation Macros
/// @brief Macros for bit operations on integers and floating-point types
///       Supports uint8_t, uint16_t, uint32_t, uint64_t, float, and double
          
/// Generate bit mask for n-th bit
/// @param n Bit position (0-based)
#define BIT(n) (1ULL << (n))

/// Set n-th bit in integer variable
/// @param var Variable to modify (integer types)
/// @param bit_num Bit position
#define SET_BIT(var, bit_num) ((var) |= (1ULL << (bit_num)))

/// Clear n-th bit in integer variable
/// @param var Variable to modify (integer types)
/// @param bit_num Bit position
#define CLR_BIT(var, bit_num) ((var) &= ~(1ULL << (bit_num)))

/// Toggle n-th bit in integer variable
/// @param var Variable to modify (integer types)
/// @param bit_num Bit position
#define TOGGLE_BIT(var, bit_num) ((var) ^= (1ULL << (bit_num)))

/// Get n-th bit from integer variable
/// @param var Variable to read (integer types)
/// @param bit_num Bit position
/// @return Bit value (0 or 1)
#define GET_BIT(var, bit_num) (((var) >> (bit_num)) & 1ULL)

/// Set bits specified by mask in integer variable
/// @param var Variable to modify (integer types)
/// @param mask Bit mask
#define SET_BITS(var, mask) ((var) |= (mask))

/// Clear bits specified by mask in integer variable
/// @param var Variable to modify (integer types)
/// @param mask Bit mask
#define CLR_BITS(var, mask) ((var) &= ~(mask))

/// Get bits specified by mask from integer variable
/// @param var Variable to read (integer types)
/// @param mask Bit mask
/// @return Masked bit values
#define GET_BITS(var, mask) ((var) & (mask))

/// Check if n-th bit is set in integer variable
/// @param var Variable to check (integer types)
/// @param bit_num Bit position
/// @return Non-zero if set, 0 if clear
#define IS_BIT_SET(var, bit_num) (((var) & (1ULL << (bit_num))) != 0)

/// Check if n-th bit is clear in integer variable
/// @param var Variable to check (integer types)
/// @param bit_num Bit position
/// @return Non-zero if clear, 0 if set
#define IS_BIT_CLR(var, bit_num) (((var) & (1ULL << (bit_num))) == 0)

/// Check if all bits in mask are set in integer variable
/// @param var Variable to check (integer types)
/// @param mask Bit mask
/// @return Non-zero if all set, 0 otherwise
#define IS_BITS_SET(var, mask) (((var) & (mask)) == (mask))

/// Check if all bits in mask are clear in integer variable
/// @param var Variable to check (integer types)
/// @param mask Bit mask
/// @return Non-zero if all clear, 0 otherwise
#define IS_BITS_CLR(var, mask) (((var) & (mask)) == 0)

/// Set n-th bit in float variable
/// @param fl Float variable to modify
/// @param bit_num Bit position (0-31)
#define SET_FLOAT_BIT(fl, bit_num) do { \
        uint32_t tmp; \
        memcpy(&tmp, &(fl), sizeof(float)); \
        tmp |= (1U << (bit_num)); \
        memcpy(&(fl), &tmp, sizeof(float)); \
} while (0)

/// Set n-th bit in double variable
/// @param db Double variable to modify
/// @param bit_num Bit position (0-63)
#define SET_DOUBLE_BIT(db, bit_num) do { \
        uint64_t tmp; \
        memcpy(&tmp, &(db), sizeof(double)); \
        tmp |= (1ULL << (bit_num)); \
        memcpy(&(db), &tmp, sizeof(double)); \
} while (0)

/// Clear n-th bit in float variable
/// @param fl Float variable to modify
/// @param bit_num Bit position (0-31)
#define CLR_FLOAT_BIT(fl, bit_num) do { \
        uint32_t tmp; \
        memcpy(&tmp, &(fl), sizeof(float)); \
        tmp &= ~(1U << (bit_num)); \
        memcpy(&(fl), &tmp, sizeof(float)); \
} while (0)

/// Clear n-th bit in double variable
/// @param db Double variable to modify
/// @param bit_num Bit position (0-63)
#define CLR_DOUBLE_BIT(db, bit_num) do { \
        uint64_t tmp; \
        memcpy(&tmp, &(db), sizeof(double)); \
        tmp &= ~(1ULL << (bit_num)); \
        memcpy(&(db), &tmp, sizeof(double)); \
} while (0)

/// Get n-th bit from float variable
/// @param fl Float variable to read
/// @param bit_num Bit position (0-31)
/// @return Bit value (0 or 1)
#define GET_FLOAT_BIT(fl, bit_num) ({ \
        uint32_t tmp; \
        memcpy(&tmp, &(fl), sizeof(float)); \
        ((tmp >> (bit_num)) & 1U); \
})

/// Get n-th bit from double variable
/// @param db Double variable to read
/// @param bit_num Bit position (0-63)
/// @return Bit value (0 or 1)
#define GET_DOUBLE_BIT(db, bit_num) ({ \
        uint64_t tmp; \
        memcpy(&tmp, &(db), sizeof(double)); \
        ((tmp >> (bit_num)) & 1ULL); \
})

// -----------------------------------------------------------------------------
// Data Extraction Macros
// -----------------------------------------------------------------------------

/// @defgroup data_extraction Data Extraction Macros
/// @brief Macros for extracting bit ranges from integers

/// Extract upper 4 bits from 8-bit value
/// @param data 8-bit integer
/// @return Bits 7-4
#define GET_UPPER_4BIT(data) (((data) >> 4) & 0xF)

/// Extract lower 4 bits from 8-bit value
/// @param data 8-bit integer
/// @return Bits 3-0
#define GET_LOWER_4BIT(data) ((data) & 0xF)

/// Extract upper 16 bits from 32-bit value
/// @param data 32-bit integer
/// @return Bits 31-16
#define GET_UPPER_16BIT(data) ((data) >> 16)

/// Extract lower 16 bits from 32-bit value
/// @param data 32-bit integer
/// @return Bits 15-0
#define GET_LOWER_16BIT(data) ((data) & 0xFFFF)

/// Extract upper 32 bits from 64-bit value
/// @param data 64-bit integer
/// @return Bits 63-32
#define GET_UPPER_32BIT(data) ((data) >> 32)

/// Extract lower 32 bits from 64-bit value
/// @param data 64-bit integer
/// @return Bits 31-0
#define GET_LOWER_32BIT(data) ((data) & 0xFFFFFFFFULL)

// -----------------------------------------------------------------------------
// Byte Swap Macros
// -----------------------------------------------------------------------------

/// @defgroup byte_swap Byte Swap Macros
/// @brief Macros for swapping byte order in integers

/// Swap bytes in 16-bit value
/// @param x 16-bit integer
/// @return Swapped value (e.g., 0x1234 -> 0x3412)
#define SWAP_UINT16(x) (((x) >> 8) | (((x) & 0xFF) << 8))

/// Swap bytes in 32-bit value
/// @param x 32-bit integer
/// @return Swapped value (e.g., 0x12345678 -> 0x78563412)
#define SWAP_UINT32(x) ((((x) >> 24) & 0xFF) | (((x) >> 8) & 0xFF00) | \
                        (((x) & 0xFF00) << 8) | (((x) & 0xFF) << 24))

/// Swap bytes in 64-bit value
/// @param x 64-bit integer
/// @return Swapped value (e.g., 0x123456789ABCDEF0 -> 0xF0DEBC9A78563412)
#define SWAP_UINT64(x) ((((x) >> 56) & 0xFF) | (((x) >> 40) & 0xFF00) | \
                        (((x) >> 24) & 0xFF0000) | (((x) >> 8) & 0xFF000000) | \
                        (((x) & 0xFF000000) << 8) | (((x) & 0xFF0000) << 24) | \
                        (((x) & 0xFF00) << 40) | (((x) & 0xFF) << 56))

// -----------------------------------------------------------------------------
// Endianness Handling
// -----------------------------------------------------------------------------

/// @defgroup endianness Endianness Handling
/// @brief Types and macros for endian conversion

/// Endianness enumeration
typedef enum {
        ENDIAN_ABCD,    ///< Big-endian (ABCD order)
        ENDIAN_DCBA,    ///< Little-endian (DCBA order)
        ENDIAN_BADC,    ///< Big-endian word swap (BADC order)
        ENDIAN_CDAB     ///< Little-endian word swap (CDAB order)
} enEndianness;

/// Combine 4 bytes into 32-bit value based on endianness
/// @param a First byte
/// @param b Second byte
/// @param c Third byte
/// @param d Fourth byte
/// @param endianness Endianness type
/// @return Combined 32-bit value
#define COMBINE_32BIT(a, b, c, d, endianness) \
        ((endianness) == ENDIAN_ABCD ? \
        ((uint32_t)(((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | ((uint32_t)(d)))) : \
        ((endianness) == ENDIAN_DCBA ? \
        ((uint32_t)(((uint32_t)(d) << 24) | ((uint32_t)(c) << 16) | ((uint32_t)(b) << 8) | ((uint32_t)(a)))) : \
        ((endianness) == ENDIAN_BADC ? \
        ((uint32_t)(((uint32_t)(b) << 24) | ((uint32_t)(a) << 16) | ((uint32_t)(d) << 8) | ((uint32_t)(c)))) : \
        ((uint32_t)(((uint32_t)(c) << 24) | ((uint32_t)(d) << 16) | ((uint32_t)(a) << 8) | ((uint32_t)(b)))))))

/// Combine 8 bytes into 64-bit value based on endianness
/// @param a First byte
/// @param b Second byte
/// @param c Third byte
/// @param d Fourth byte
/// @param e Fifth byte
/// @param f Sixth byte
/// @param g Seventh byte
/// @param h Eighth byte
/// @param endianness Endianness type
/// @return Combined 64-bit value
#define COMBINE_64BIT(a, b, c, d, e, f, g, h, endianness) \
        ((endianness) == ENDIAN_ABCD ? \
        ((uint64_t)(((uint64_t)(a) << 56) | ((uint64_t)(b) << 48) | ((uint64_t)(c) << 40) | ((uint64_t)(d) << 32) | \
        ((uint64_t)(e) << 24) | ((uint64_t)(f) << 16) | ((uint64_t)(g) << 8) | ((uint64_t)(h)))) : \
        ((endianness) == ENDIAN_DCBA ? \
        ((uint64_t)(((uint64_t)(h) << 56) | ((uint64_t)(g) << 48) | ((uint64_t)(f) << 40) | ((uint64_t)(e) << 32) | \
        ((uint64_t)(d) << 24) | ((uint64_t)(c) << 16) | ((uint64_t)(b) << 8) | ((uint64_t)(a)))) : \
        ((endianness) == ENDIAN_BADC ? \
        ((uint64_t)(((uint64_t)(b) << 56) | ((uint64_t)(a) << 48) | ((uint64_t)(d) << 40) | ((uint64_t)(c) << 32) | \
        ((uint64_t)(f) << 24) | ((uint64_t)(e) << 16) | ((uint64_t)(h) << 8) | ((uint64_t)(g)))) : \
        ((uint64_t)(((uint64_t)(c) << 56) | ((uint64_t)(d) << 48) | ((uint64_t)(a) << 40) | ((uint64_t)(b) << 32) | \
        ((uint64_t)(e) << 24) | ((uint64_t)(f) << 16) | ((uint64_t)(g) << 8) | ((uint64_t)(h)))))))

/// Copy 32-bit value to destination
/// @param dst Destination pointer
/// @param src Source 32-bit value
#define SET_32BIT(dst, src) memcpy((dst), &(src), sizeof(uint32_t))

/// Copy 64-bit value to destination
/// @param dst Destination pointer
/// @param src Source 64-bit value
#define SET_64BIT(dst, src) memcpy((dst), &(src), sizeof(uint64_t))

// -----------------------------------------------------------------------------
// Utility Functions
// -----------------------------------------------------------------------------

/// @defgroup utilities Utility Functions
/// @brief Functions for data manipulation and conversion

/// Swap bytes in array of 16-bit values
/// @param src Source array of 16-bit values
/// @param dst Destination array for swapped values
/// @param len Number of elements
inline void swap_array_uint16(const uint16_t *src, uint16_t *dst, int len)
{
        int i;
        for (i = 0; i < len; i++) {
                dst[i] = SWAP_UINT16(src[i]);
        }
}

/// Swap bytes in array of 32-bit values
/// @param src Source array of 32-bit values
/// @param dst Destination array for swapped values
/// @param len Number of elements
inline void swap_array_uint32(const uint32_t *src, uint32_t *dst, int len)
{
        int i;
        for (i = 0; i < len; i++) {
                dst[i] = SWAP_UINT32(src[i]);
        }
}

/// Swap bytes in array of 64-bit values
/// @param src Source array of 64-bit values
/// @param dst Destination array for swapped values
/// @param len Number of elements
inline void swap_array_uint64(const uint64_t *src, uint64_t *dst, int len)
{
        int i;
        for (i = 0; i < len; i++) {
                dst[i] = SWAP_UINT64(src[i]);
        }
}

/// Convert 16-bit value array to float or double with endianness
/// @param src Source array of 16-bit values
/// @param dst Destination for float (4 bytes) or double (8 bytes)
/// @param size Size of destination type (4=float, 8=double)
/// @param endianness Endianness type
inline void get_endian_data(const uint16_t *src, void *dst, size_t size, enEndianness endianness)
{
        if (size == 4) { // 32-bit case
                uint8_t a = (src[0] >> 8) & 0xFF;
                uint8_t b = (src[0] >> 0) & 0xFF;
                uint8_t c = (src[1] >> 8) & 0xFF;
                uint8_t d = (src[1] >> 0) & 0xFF;
                uint32_t i = COMBINE_32BIT(a, b, c, d, endianness);
                SET_32BIT(dst, i);
        } else if (size == 8) { // 64-bit case
                uint8_t a = (src[0] >> 8) & 0xFF;
                uint8_t b = (src[0] >> 0) & 0xFF;
                uint8_t c = (src[1] >> 8) & 0xFF;
                uint8_t d = (src[1] >> 0) & 0xFF;
                uint8_t e = (src[2] >> 8) & 0xFF;
                uint8_t f = (src[2] >> 0) & 0xFF;
                uint8_t g = (src[3] >> 8) & 0xFF;
                uint8_t h = (src[3] >> 0) & 0xFF;
                uint64_t i = COMBINE_64BIT(a, b, c, d, e, f, g, h, endianness);
                SET_64BIT(dst, i);
        }
}

// -----------------------------------------------------------------------------
// Bit Field Structures
// -----------------------------------------------------------------------------

/// @defgroup bit_fields Bit Field Structures
/// @brief Structures for bit-level data access
///       Packed and aligned to 1 byte for memory efficiency

/// 8-bit bit field structure (bits 7-0)
struct tBBit {
        uint8_t bit7  : 1;
        uint8_t bit6  : 1;
        uint8_t bit5  : 1;
        uint8_t bit4  : 1;
        uint8_t bit3  : 1;
        uint8_t bit2  : 1;
        uint8_t bit1  : 1;
        uint8_t bit0  : 1;
} __attribute__((aligned(1), packed));

/// 16-bit bit field structure (bits 15-0)
struct tWBit {
        uint16_t bit15 : 1;
        uint16_t bit14 : 1;
        uint16_t bit13 : 1;
        uint16_t bit12 : 1;
        uint16_t bit11 : 1;
        uint16_t bit10 : 1;
        uint16_t bit9  : 1;
        uint16_t bit8  : 1;
        uint16_t bit7  : 1;
        uint16_t bit6  : 1;
        uint16_t bit5  : 1;
        uint16_t bit4  : 1;
        uint16_t bit3  : 1;
        uint16_t bit2  : 1;
        uint16_t bit1  : 1;
        uint16_t bit0  : 1;
} __attribute__((aligned(1), packed));

/// 32-bit bit field structure (bits 31-0)
struct tDWBit {
        uint32_t bit31 : 1;
        uint32_t bit30 : 1;
        uint32_t bit29 : 1;
        uint32_t bit28 : 1;
        uint32_t bit27 : 1;
        uint32_t bit26 : 1;
        uint32_t bit25 : 1;
        uint32_t bit24 : 1;
        uint32_t bit23 : 1;
        uint32_t bit22 : 1;
        uint32_t bit21 : 1;
        uint32_t bit20 : 1;
        uint32_t bit19 : 1;
        uint32_t bit18 : 1;
        uint32_t bit17 : 1;
        uint32_t bit16 : 1;
        uint32_t bit15 : 1;
        uint32_t bit14 : 1;
        uint32_t bit13 : 1;
        uint32_t bit12 : 1;
        uint32_t bit11 : 1;
        uint32_t bit10 : 1;
        uint32_t bit9  : 1;
        uint32_t bit8  : 1;
        uint32_t bit7  : 1;
        uint32_t bit6  : 1;
        uint32_t bit5  : 1;
        uint32_t bit4  : 1;
        uint32_t bit3  : 1;
        uint32_t bit2  : 1;
        uint32_t bit1  : 1;
        uint32_t bit0  : 1;
} __attribute__((aligned(1), packed));

/// 64-bit bit field structure (bits 63-0)
struct tQWBit {
        uint64_t bit63 : 1;
        uint64_t bit62 : 1;
        uint64_t bit61 : 1;
        uint64_t bit60 : 1;
        uint64_t bit59 : 1;
        uint64_t bit58 : 1;
        uint64_t bit57 : 1;
        uint64_t bit56 : 1;
        uint64_t bit55 : 1;
        uint64_t bit54 : 1;
        uint64_t bit53 : 1;
        uint64_t bit52 : 1;
        uint64_t bit51 : 1;
        uint64_t bit50 : 1;
        uint64_t bit49 : 1;
        uint64_t bit48 : 1;
        uint64_t bit47 : 1;
        uint64_t bit46 : 1;
        uint64_t bit45 : 1;
        uint64_t bit44 : 1;
        uint64_t bit43 : 1;
        uint64_t bit42 : 1;
        uint64_t bit41 : 1;
        uint64_t bit40 : 1;
        uint64_t bit39 : 1;
        uint64_t bit38 : 1;
        uint64_t bit37 : 1;
        uint64_t bit36 : 1;
        uint64_t bit35 : 1;
        uint64_t bit34 : 1;
        uint64_t bit33 : 1;
        uint64_t bit32 : 1;
        uint64_t bit31 : 1;
        uint64_t bit30 : 1;
        uint64_t bit29 : 1;
        uint64_t bit28 : 1;
        uint64_t bit27 : 1;
        uint64_t bit26 : 1;
        uint64_t bit25 : 1;
        uint64_t bit24 : 1;
        uint64_t bit23 : 1;
        uint64_t bit22 : 1;
        uint64_t bit21 : 1;
        uint64_t bit20 : 1;
        uint64_t bit19 : 1;
        uint64_t bit18 : 1;
        uint64_t bit17 : 1;
        uint64_t bit16 : 1;
        uint64_t bit15 : 1;
        uint64_t bit14 : 1;
        uint64_t bit13 : 1;
        uint64_t bit12 : 1;
        uint64_t bit11 : 1;
        uint64_t bit10 : 1;
        uint64_t bit9  : 1;
        uint64_t bit8  : 1;
        uint64_t bit7  : 1;
        uint64_t bit6  : 1;
        uint64_t bit5  : 1;
        uint64_t bit4  : 1;
        uint64_t bit3  : 1;
        uint64_t bit2  : 1;
        uint64_t bit1  : 1;
        uint64_t bit0  : 1;
} __attribute__((aligned(1), packed));

// -----------------------------------------------------------------------------
// Union Types
// -----------------------------------------------------------------------------

/// @defgroup unions Union Types
/// @brief Unions for flexible data access and conversion
///       t*BIT unions include bit fields for hardware register access.
///       t*BYTE unions include signed types for data parsing.

/// Union for 8-bit data with bit field access
typedef union __attribute__((aligned(1), packed)) {
        uint8_t         bt;     ///< 8-bit value
        struct tBBit    bit;    ///< Bit field access
} tBYTEBIT;

/// Union for 16-bit data with bit field access
typedef union __attribute__((aligned(1), packed)) {
        uint16_t        wd;     ///< 16-bit value
        uint8_t         bt[2];  ///< Byte array
        struct tWBit    bit;    ///< Bit field access
} tWORDBIT;

/// Union for 32-bit data with bit field and float access
typedef union __attribute__((aligned(1), packed)) {
        float           fl;      ///< 32-bit float
        uint32_t        dw;      ///< 32-bit double word
        uint16_t        wd[2];   ///< Word array
        uint8_t         bt[4];   ///< Byte array
        struct tWBit    wbit[2]; ///< 16-bit bit field array
        struct tDWBit   dwbit;   ///< 32-bit bit field
} tDWORDBIT;

/// Union for 64-bit data with bit field and double access
typedef union __attribute__((aligned(1), packed)) {
        double          db;      ///< 64-bit double
        uint64_t        qw;      ///< 64-bit quad word
        uint32_t        dw[2];   ///< Double word array
        uint16_t        wd[4];   ///< Word array
        uint8_t         bt[8];   ///< Byte array
        struct tWBit    wbit[4]; ///< 16-bit bit field array
        struct tDWBit   dwbit[2];///< 32-bit bit field array
        struct tQWBit   qwbit;   ///< 64-bit bit field
} tQWORDBIT;

/// Union for 16-bit data without bit fields
typedef union __attribute__((aligned(1), packed)) {
        uint16_t        wd;     ///< 16-bit value
        uint8_t         bt[2];  ///< Byte array
} tWORDBYTE;

/// Union for 32-bit data with signed and float access
typedef union __attribute__((aligned(1), packed)) {
        float           fl;      ///< 32-bit float
        int32_t         sdw;     ///< 32-bit signed double word
        int16_t         sw[2];   ///< Signed word array
        int8_t          sbt[4];  ///< Signed byte array
        uint32_t        dw;      ///< 32-bit unsigned double word
        uint16_t        wd[2];   ///< Unsigned word array
        uint8_t         bt[4];   ///< Unsigned byte array
} tDWORDBYTE;

/// Union for 64-bit data with signed and double access
typedef union __attribute__((aligned(1), packed)) {
        double          db;      ///< 64-bit double
        int64_t         sqw;     ///< 64-bit signed quad word
        int32_t         sdw[2];  ///< Signed double word array
        int16_t         sw[4];   ///< Signed word array
        int8_t          sbt[8];  ///< Signed byte array
        uint64_t        qw;      ///< 64-bit unsigned quad word
        uint32_t        dw[2];   ///< Unsigned double word array
        uint16_t        wd[4];   ///< Unsigned word array
        uint8_t         bt[8];   ///< Unsigned byte array
} tQWORDBYTE;

#endif /*_DEF_TYPES_H_*/