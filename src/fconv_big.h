#pragma once
// The big integers of the exact conversions (src/fconv.c for float, src/fconv64.c for double): a number of 32-bit
// limbs, least significant first. A file includes this after defining LIMBS, as many as its largest number needs;
// a result that would need more is cut at the top, which the files size LIMBS never to let happen. NOT installed.


struct big
{
    unsigned int w[LIMBS];
    int n;                           // limbs in use; 0 is zero
};

static void big_set(struct big* b, unsigned int x)
{
    b->w[0] = x;
    b->n = x != 0 ? 1 : 0;
}

static void big_copy(struct big* to, const struct big* from)
{
    to->n = from->n;
    for (int i = 0; i < from->n; i++)
        to->w[i] = from->w[i];
}

static void big_mul_small(struct big* b, unsigned int m)
{
    unsigned long long carry = 0;
    for (int i = 0; i < b->n; i++)
    {
        unsigned long long t = (unsigned long long)b->w[i] * m + carry;
        b->w[i] = (unsigned int)t;
        carry = t >> 32;
    }
    if (carry != 0 && b->n < LIMBS)
        b->w[b->n++] = (unsigned int)carry;
}

static void big_add_small(struct big* b, unsigned int a)
{
    for (int i = 0; a != 0; i++)
    {
        if (i == b->n)
        {
            if (b->n == LIMBS)
                return;
            b->w[b->n++] = 0;
        }
        unsigned int before = b->w[i];
        b->w[i] = before + a;
        a = b->w[i] < before ? 1u : 0u;
    }
}

static void big_shl(struct big* b, int k)
{
    if (b->n == 0 || k <= 0)
        return;
    int limbs = k / 32, bits = k % 32;
    int n = b->n + limbs + 1;
    if (n > LIMBS)
        n = LIMBS;
    for (int i = n - 1; i >= 0; i--)
    {
        int from = i - limbs;
        unsigned int hi = from >= 0 && from < b->n ? b->w[from] : 0u;
        unsigned int lo = from - 1 >= 0 && from - 1 < b->n ? b->w[from - 1] : 0u;
        b->w[i] = bits == 0 ? hi : (hi << bits) | (lo >> (32 - bits));
    }
    b->n = n;
    while (b->n > 0 && b->w[b->n - 1] == 0)
        b->n--;
}

static void big_shr1(struct big* b)
{
    for (int i = 0; i < b->n; i++)
    {
        unsigned int next = i + 1 < b->n ? b->w[i + 1] : 0u;
        b->w[i] = (b->w[i] >> 1) | (next << 31);
    }
    while (b->n > 0 && b->w[b->n - 1] == 0)
        b->n--;
}

static int big_cmp(const struct big* a, const struct big* b)
{
    if (a->n != b->n)
        return a->n < b->n ? -1 : 1;
    for (int i = a->n - 1; i >= 0; i--)
        if (a->w[i] != b->w[i])
            return a->w[i] < b->w[i] ? -1 : 1;
    return 0;
}

// a -= b, where a >= b.
static void big_sub(struct big* a, const struct big* b)
{
    unsigned int borrow = 0;
    for (int i = 0; i < a->n; i++)
    {
        unsigned int bw = i < b->n ? b->w[i] : 0u;
        unsigned int t = a->w[i] - bw - borrow;
        borrow = (a->w[i] < bw || (a->w[i] == bw && borrow)) ? 1u : 0u;
        a->w[i] = t;
    }
    while (a->n > 0 && a->w[a->n - 1] == 0)
        a->n--;
}

static int big_bits(const struct big* b)
{
    if (b->n == 0)
        return 0;
    return (b->n - 1) * 32 + 32 - __builtin_clz(b->w[b->n - 1]);
}

// b /= d, and the remainder.
static unsigned int big_divmod_small(struct big* b, unsigned int d)
{
    unsigned long long rem = 0;
    for (int i = b->n - 1; i >= 0; i--)
    {
        unsigned long long t = (rem << 32) | b->w[i];
        b->w[i] = (unsigned int)(t / d);
        rem = t % d;
    }
    while (b->n > 0 && b->w[b->n - 1] == 0)
        b->n--;
    return (unsigned int)rem;
}

static void big_mul_pow(struct big* b, unsigned int base_pow_chunk, int chunk, unsigned int base, int k)
{
    while (k >= chunk)
    {
        big_mul_small(b, base_pow_chunk);
        k -= chunk;
    }
    unsigned int rest = 1;
    for (int i = 0; i < k; i++)
        rest *= base;
    if (rest != 1)
        big_mul_small(b, rest);
}

static void big_mul_pow10(struct big* b, int k) { big_mul_pow(b, 1000000000u, 9, 10u, k); }
static void big_mul_pow5(struct big* b, int k)  { big_mul_pow(b, 1220703125u, 13, 5u, k); }

