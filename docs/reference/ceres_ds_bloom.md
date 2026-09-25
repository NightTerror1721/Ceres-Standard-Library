# `<ceres/ds/bloom.h>`

A Bloom filter: approximate set membership in a fraction of a real set's memory, at the cost of occasional false positives (never false negatives) - "have I possibly seen this?" over millions of world-generation coordinates or asset keys, where ceres/ds/hset.h would hold every key in full but this only ever holds k bits per insertion. There is no bloom_remove: a plain Bloom filter cannot forget one item without risking a false negative for another that happens to share a bit.

```c
unsigned int words[bitset_words(4096)];
struct bloom b;  bloom_init(&b, words, 4096, 5);
bloom_add(&b, "player_42", 9);
if (bloom_maybe_has(&b, "player_42", 9)) { ... }   // true; a miss is always a real miss
```

```c
struct bloom
{
    struct bitset bits;
    unsigned int k;       // how many bits each item sets - 3 to 7 is enough for most sizes
};

// `words` needs bitset_words(nbits) words, every bit clear.
void bloom_init(struct bloom* b, unsigned int* words, unsigned int nbits, unsigned int k);
void bloom_clear(struct bloom* b);                                                    // every bit clear again
void bloom_add(struct bloom* b, const void* item, unsigned int len);
int  bloom_maybe_has(const struct bloom* b, const void* item, unsigned int len);      // 0: definitely not; 1: probably
```
