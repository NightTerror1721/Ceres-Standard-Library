#pragma once

// The bit instructions, plus helpers built on them. Each one is a macro on the compiler's builtin for
// its instruction, so it costs that one instruction; the functions in asm/bits.casm remain for whoever
// takes their address or writes the name in parentheses.

unsigned int bit_clz(unsigned int x);        // leading zeros; 32 when x == 0
unsigned int bit_ctz(unsigned int x);        // trailing zeros; 32 when x == 0
unsigned int bit_popcount(unsigned int x);
unsigned int bit_bswap(unsigned int x);      // reverse the four bytes
unsigned int bit_rotl(unsigned int x, int n);   // the count uses its low five bits
unsigned int bit_rotr(unsigned int x, int n);
unsigned int umulhi(unsigned int a, unsigned int b);   // high 32 bits of the unsigned 64-bit product
int          imulhi(int a, int b);                     // ... of the signed one

#define bit_clz(x)       __builtin_clz((unsigned int)(x))
#define bit_ctz(x)       __builtin_ctz((unsigned int)(x))
#define bit_popcount(x)  __builtin_popcount((unsigned int)(x))
#define bit_bswap(x)     __builtin_bswap32((unsigned int)(x))
#define bit_rotl(x, n)   __builtin_rotl32((unsigned int)(x), (unsigned int)(n))
#define bit_rotr(x, n)   __builtin_rotr32((unsigned int)(x), (unsigned int)(n))
#define umulhi(a, b)     __builtin_mulhu((unsigned int)(a), (unsigned int)(b))
#define imulhi(a, b)     __builtin_mulhs((int)(a), (int)(b))

static inline int bit_is_pow2(unsigned int x) { return x != 0 && (x & (x - 1)) == 0; }
static inline unsigned int bit_next_pow2(unsigned int x) { return x <= 1 ? 1 : 1u << (32 - bit_clz(x - 1)); }
static inline unsigned int bit_log2(unsigned int x) { return 31 - bit_clz(x); }   // x != 0
static inline unsigned int align_up(unsigned int x, unsigned int a) { return (x + a - 1) & ~(a - 1); }     // a: power of 2
static inline unsigned int align_down(unsigned int x, unsigned int a) { return x & ~(a - 1); }

#define BIT(n)          (1u << (n))
#define BIT_SET(v, n)   ((v) |= BIT(n))
#define BIT_CLR(v, n)   ((v) &= ~BIT(n))
#define BIT_TEST(v, n)  (((v) >> (n)) & 1u)
