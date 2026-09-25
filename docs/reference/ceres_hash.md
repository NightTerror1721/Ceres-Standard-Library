# `<ceres/hash.h>`

Hashes and checksums, all in 32-bit arithmetic. None of them is cryptographic.

```c
hash_fnv1a("")            = 0x811C9DC5
hash_crc32("123456789")   = 0xCBF43926
hash_adler32("Wikipedia") = 0x11E60398
```

```c
unsigned int hash_fnv1a(const void* p, size_t n);
unsigned int hash_djb2(const char* s);                                  // NUL-terminated
unsigned int hash_murmur3(const void* p, size_t n, unsigned int seed);  // MurmurHash3, x86 32-bit
unsigned int hash_crc32(const void* p, size_t n);                       // IEEE 802.3
unsigned int hash_adler32(const void* p, size_t n);

// Feeding a CRC in pieces: start from 0 and pass each result to the next call. The value after the
// last piece equals hash_crc32 of all the bytes together.
unsigned int hash_crc32_update(unsigned int crc, const void* p, size_t n);

static inline unsigned int hash_str(const char* s) { return hash_djb2(s); }
```
