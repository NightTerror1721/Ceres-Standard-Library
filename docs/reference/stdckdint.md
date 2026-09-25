# `<stdckdint.h>`

C23 <stdckdint.h>: checked integer arithmetic.

```c
bool ckd_add(T* result, a, b);   ckd_sub, ckd_mul the same
```

Stores a op b into *result, wrapped to T the way a two's-complement machine wraps it, and returns true when the mathematically exact a op b does not fit T. The operands may be of any integer type and different ones - ckd_add(&u, -1, 5) is 4 and fits an unsigned - and T any signed or unsigned integer type but char and bool; all of it up to 64 bits, exactly: the operands are carried as a sign and a 64-bit magnitude, and a product as wide as 128 bits is still told apart from one that fits.

Exactness costs a few calls. For a hot loop on int alone, __builtin_add_overflow(a, b, &r) (and _sub, _mul) is a handful of instructions - but it judges overflow in the operands' type, not in r's.

```c
_Static_assert(sizeof(long) == 4 && sizeof(long long) == 8, "stdckdint.h: the bounds below assume a 32-bit long");

#define __STDC_VERSION_STDCKDINT_H__ 202311L      // the l suffix: Ceres-C 6a4ef5a

// A value as a sign and a magnitude; `big` says the magnitude is 2^64 or more (only mag's low 64 bits
// are kept, which is what the wrapped result needs).
struct __ckd_value
{
    unsigned long long mag;
    bool neg;
    bool big;
};

static inline struct __ckd_value __ckd_from_signed(long long x)
{
    struct __ckd_value v;
    v.neg = x < 0;
    v.mag = v.neg ? 0ull - (unsigned long long)x : (unsigned long long)x;
    v.big = false;
    return v;
}

static inline struct __ckd_value __ckd_from_unsigned(unsigned long long x)
{
    struct __ckd_value v;
    v.neg = false;
    v.mag = x;
    v.big = false;
    return v;
}

// Every integer type but unsigned long long converts to long long without changing its value.
#define __CKD_VALUE(x) _Generic((x), unsigned long long: __ckd_from_unsigned, default: __ckd_from_signed)(x)

static inline struct __ckd_value __ckd_add(struct __ckd_value a, struct __ckd_value b)
{
    struct __ckd_value r;
    r.big = false;
    if (a.neg == b.neg)
    {
        r.mag = a.mag + b.mag;
        r.big = r.mag < a.mag;                        // the carry out of 64 bits
        r.neg = a.neg;
    }
    else if (a.mag >= b.mag)
    {
        r.mag = a.mag - b.mag;
        r.neg = a.neg;
    }
    else
    {
        r.mag = b.mag - a.mag;
        r.neg = b.neg;
    }
    if (r.mag == 0ull && !r.big)
        r.neg = false;
    return r;
}

static inline struct __ckd_value __ckd_sub(struct __ckd_value a, struct __ckd_value b)
{
    if (b.mag != 0ull)
        b.neg = !b.neg;
    return __ckd_add(a, b);
}

static inline struct __ckd_value __ckd_mul(struct __ckd_value a, struct __ckd_value b)
{
    // (ah*2^32 + al)(bh*2^32 + bl): ah*bh lands at 2^64 and up, the cross terms at 2^32.
    unsigned long long al = a.mag & 0xFFFFFFFFull;
    unsigned long long ah = a.mag >> 32;
    unsigned long long bl = b.mag & 0xFFFFFFFFull;
    unsigned long long bh = b.mag >> 32;
    struct __ckd_value r;
    r.big = ah != 0ull && bh != 0ull;
    unsigned long long cross = ah * bl;
    unsigned long long other = al * bh;
    unsigned long long mid = cross + other;
    if (mid < cross || (mid >> 32) != 0ull)
        r.big = true;
    unsigned long long low = al * bl;
    r.mag = low + (mid << 32);
    if (r.mag < low)
        r.big = true;
    r.neg = a.neg != b.neg && (r.mag != 0ull || r.big);
    return r;
}

static inline unsigned long long __ckd_wrapped(struct __ckd_value v)
{
    return v.neg ? 0ull - v.mag : v.mag;
}

// Whether v lies in [-neg_max, pos_max].
static inline bool __ckd_fits(struct __ckd_value v, unsigned long long pos_max, unsigned long long neg_max)
{
    return !v.big && (v.neg ? v.mag <= neg_max : v.mag <= pos_max);
}

static inline bool __ckd_store_sc(signed char* r, struct __ckd_value v)         { *r = (signed char)__ckd_wrapped(v); return !__ckd_fits(v, 0x7Full, 0x80ull); }
static inline bool __ckd_store_s(short* r, struct __ckd_value v)                { *r = (short)__ckd_wrapped(v); return !__ckd_fits(v, 0x7FFFull, 0x8000ull); }
static inline bool __ckd_store_i(int* r, struct __ckd_value v)                  { *r = (int)__ckd_wrapped(v); return !__ckd_fits(v, 0x7FFFFFFFull, 0x80000000ull); }
static inline bool __ckd_store_l(long* r, struct __ckd_value v)                 { *r = (long)__ckd_wrapped(v); return !__ckd_fits(v, 0x7FFFFFFFull, 0x80000000ull); }
static inline bool __ckd_store_ll(long long* r, struct __ckd_value v)           { *r = (long long)__ckd_wrapped(v); return !__ckd_fits(v, 0x7FFFFFFFFFFFFFFFull, 0x8000000000000000ull); }
static inline bool __ckd_store_uc(unsigned char* r, struct __ckd_value v)       { *r = (unsigned char)__ckd_wrapped(v); return !__ckd_fits(v, 0xFFull, 0ull); }
static inline bool __ckd_store_us(unsigned short* r, struct __ckd_value v)      { *r = (unsigned short)__ckd_wrapped(v); return !__ckd_fits(v, 0xFFFFull, 0ull); }
static inline bool __ckd_store_ui(unsigned int* r, struct __ckd_value v)        { *r = (unsigned int)__ckd_wrapped(v); return !__ckd_fits(v, 0xFFFFFFFFull, 0ull); }
static inline bool __ckd_store_ul(unsigned long* r, struct __ckd_value v)       { *r = (unsigned long)__ckd_wrapped(v); return !__ckd_fits(v, 0xFFFFFFFFull, 0ull); }
static inline bool __ckd_store_ull(unsigned long long* r, struct __ckd_value v) { *r = __ckd_wrapped(v); return !__ckd_fits(v, 0xFFFFFFFFFFFFFFFFull, 0ull); }

#define __CKD_STORE(r, v) _Generic((r),                                                               \
    signed char*: __ckd_store_sc, short*: __ckd_store_s, int*: __ckd_store_i, long*: __ckd_store_l,  \
    long long*: __ckd_store_ll, unsigned char*: __ckd_store_uc, unsigned short*: __ckd_store_us,     \
    unsigned int*: __ckd_store_ui, unsigned long*: __ckd_store_ul,                                  \
    unsigned long long*: __ckd_store_ull)((r), (v))

#define ckd_add(r, a, b) __CKD_STORE((r), __ckd_add(__CKD_VALUE(a), __CKD_VALUE(b)))
#define ckd_sub(r, a, b) __CKD_STORE((r), __ckd_sub(__CKD_VALUE(a), __CKD_VALUE(b)))
#define ckd_mul(r, a, b) __CKD_STORE((r), __ckd_mul(__CKD_VALUE(a), __CKD_VALUE(b)))
```
