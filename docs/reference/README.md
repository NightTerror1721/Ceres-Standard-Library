# The Ceres standard library: reference

Generated from the headers by `node tools/gendocs.js` - one page each, their own comments and declarations.

## The C library

| Header | What it is |
| --- | --- |
| [`<assert.h>`](assert.md) | assert(expr) stops the program with "file:line: function: assertion 'expr' failed" when expr is 0. |
| [`<ceres.h>`](ceres.md) | Ceres stdlib - low-level device access. |
| [`<ctype.h>`](ctype.md) | Every one looks at its argument alone (the "C" locale), so each is `const`: a call whose result nothing reads may go. |
| [`<errno.h>`](errno.md) | One variable for the whole program: the machine has a single thread of control. |
| [`<float.h>`](float.md) | Ceres has one floating-point format: IEEE 754 binary32. |
| [`<interrupts.h>`](interrupts.md) | Interrupt number space. |
| [`<inttypes.h>`](inttypes.md) | printf/scanf conversion specifiers for the fixed-width types, e.g. |
| [`<iso646.h>`](iso646.md) | The alternative spellings of the operators (ISO C, Amendment 1). |
| [`<limits.h>`](limits.md) | Ceres is a 32-bit machine: `long` is `int` (32 bits). |
| [`<locale.h>`](locale.md) | Locales. |
| [`<math.h>`](math.md) | The machine's floating point is binary32, and these functions are float: every one has ONE implementation and the f-suffixed C99 names (sinf, powf, ...) are... |
| [`<setjmp.h>`](setjmp.md) | Non-local jumps (asm/setjmp.casm). |
| [`<signal.h>`](signal.md) | Signals. |
| [`<stdalign.h>`](stdalign.md) | C11 <stdalign.h>. |
| [`<stdarg.h>`](stdarg.md) | Variable arguments. |
| [`<stdbit.h>`](stdbit.md) | C23 <stdbit.h>: counting and finding bits, on the machine's own clz, ctz and popcnt instructions (through the compiler's builtins, so a call is a few... |
| [`<stdbool.h>`](stdbool.md) | bool, true and false are part of the language here (ceresc knows them as keywords), so this header adds nothing but the macro C99 says it defines, and _Bool... |
| [`<stdckdint.h>`](stdckdint.md) | C23 <stdckdint.h>: checked integer arithmetic. |
| [`<stddef.h>`](stddef.md) |  |
| [`<stdint.h>`](stdint.md) | Fixed-width integers for a 32-bit machine. |
| [`<stdio.h>`](stdio.md) | Console I/O over the terminal device (see ceres/terminal.h), formatted input and output, and streams (FILE) over the terminal, over memory, and over files... |
| [`<stdlib.h>`](stdlib.md) | General utilities. |
| [`<stdnoreturn.h>`](stdnoreturn.md) | C11 <stdnoreturn.h>: `noreturn` for the _Noreturn function specifier (which Ceres-C predefines as __attribute__((__noreturn__)), since 558335b). |
| [`<string.h>`](string.md) | Memory (asm/memory.casm): word-at-a-time when both pointers are aligned, bytes otherwise. |
| [`<strings.h>`](strings.md) | BSD string utilities (src/string.c). |
| [`<time.h>`](time.md) | Calendar time. |
| [`<uchar.h>`](uchar.md) | C11/C23 <uchar.h>: the character types of the prefixed literals, and the conversions between them and the multibyte strings of the rest of the library,... |
| [`<wchar.h>`](wchar.md) | Wide characters. |

## Ceres: the machine and the extras

| Header | What it is |
| --- | --- |
| [`<ceres/ansi.h>`](ceres_ansi.md) | ANSI escape sequences: cursor movement, colours, clearing. |
| [`<ceres/arena.h>`](ceres_arena.md) | A linear allocator: allocation is a pointer bump, and everything is released at once (or back to a mark). |
| [`<ceres/atomic.h>`](ceres_atomic.md) | Read-modify-write that an interrupt cannot cut in half. |
| [`<ceres/audio.h>`](ceres_audio.md) | Audio device (0xFF090000): a tone generator with one voice - a note of a given frequency, duration, volume and waveform. |
| [`<ceres/backtrace.h>`](ceres_backtrace.md) | The call stack as it stands, and names for code addresses. |
| [`<ceres/bits.h>`](ceres_bits.md) | The bit instructions, plus helpers built on them. |
| [`<ceres/blitter.h>`](ceres_blitter.md) | The 2D blitter (0xFF0C0000, CeresASM 3b2db3b): rectangle operations on RGB32 surfaces in RAM done by the host, at no cost in instructions. |
| [`<ceres/blockdev.h>`](ceres_blockdev.md) | Block devices: what CeresFS (ceres/fs.h) keeps its volumes on. |
| [`<ceres/color.h>`](ceres_color.md) | Colours are 0x00RRGGBB in an unsigned int, the format of the pixel display. |
| [`<ceres/config.h>`](ceres_config.md) | Compile-time configuration. |
| [`<ceres/debug.h>`](ceres_debug.md) | Logging and inspection for programs under development. |
| [`<ceres/disk.h>`](ceres_disk.md) | Disk (0xFF020000): sectors of 512 bytes. |
| [`<ceres/display.h>`](ceres_display.md) | Display device (0xFF070000): a pixel framebuffer of RGB32 pixels (0x00RRGGBB, top byte ignored). |
| [`<ceres/dma.h>`](ceres_dma.md) | DMA controller (0xFF040000): copies memory to memory without the program moving it word by word. |
| [`<ceres/ds/bitset.h>`](ceres_ds_bitset.md) | A set of small integers 0 .. |
| [`<ceres/ds/bloom.h>`](ceres_ds_bloom.md) | A Bloom filter: approximate set membership in a fraction of a real set's memory, at the cost of occasional false positives (never false negatives) - "have I... |
| [`<ceres/ds/cqueue.h>`](ceres_ds_cqueue.md) | A fixed-capacity circular queue of fixed-size elements, over a buffer the caller provides: the ringbuf.h idea generalized from bytes to a struct-sized item,... |
| [`<ceres/ds/deque.h>`](ceres_ds_deque.md) | A double-ended queue of fixed-size elements that grows: push and pop at EITHER end in O(1) amortized, and dq_at is random access by logical index. |
| [`<ceres/ds/dsu.h>`](ceres_ds_dsu.md) | A disjoint-set (union-find) over the integers 0 .. |
| [`<ceres/ds/flatmap.h>`](ceres_ds_flatmap.md) | A map kept as a vector of (key, value) pairs, sorted by key: for a few dozen to a few hundred entries - configuration, a lookup table built once and read... |
| [`<ceres/ds/flatset.h>`](ceres_ds_flatset.md) | A set kept as a sorted vector of keys, searched by bisection - ceres/ds/flatmap.h with no value, for the same reason ceres/ds/hset.h exists next to... |
| [`<ceres/ds/generic.h>`](ceres_ds_generic.md) | _Generic sugar over ten of the eighteen collections (hset, gmap, multimap, lru, rbtree, skiplist, omap, flatmap/flatset, iheap) that all take a hash/eq or a... |
| [`<ceres/ds/gmap.h>`](ceres_ds_gmap.md) | A hash map from a key of any fixed size to a value of any fixed size - ceres/ds/hashmap.h widened past "string to pointer". |
| [`<ceres/ds/hashmap.h>`](ceres_ds_hashmap.md) | A hash map from strings to pointers: open addressing with linear probing, growing before it gets crowded. |
| [`<ceres/ds/hset.h>`](ceres_ds_hset.md) | A set of keys of any fixed size - ceres/ds/gmap.h with no value, the hash-table counterpart to ceres/ds/flatset.h. |
| [`<ceres/ds/iheap.h>`](ceres_ds_iheap.md) | A binary heap like ceres/ds/pqueue.h, plus what pqueue.h cannot do: lower an element's priority after it is already inside, or take a specific element out... |
| [`<ceres/ds/list.h>`](ceres_ds_list.md) | A doubly linked list whose node lives INSIDE the object it links: no allocation, and an object can sit on several lists at once by carrying several nodes. |
| [`<ceres/ds/lru.h>`](ceres_ds_lru.md) | A cache of bounded size with least-recently-used eviction: ceres/ds/gmap.h for O(1) lookup by key, ceres/ds/list.h for O(1) "move to the front" and "the... |
| [`<ceres/ds/multimap.h>`](ceres_ds_multimap.md) | A map from one key to any number of values - an event's list of listeners, a name's list of adjacent nodes - which neither ceres/ds/hashmap.h nor... |
| [`<ceres/ds/omap.h>`](ceres_ds_omap.md) | An ordered map of copies, keyed and valued by anything of a fixed size: the convenience layer over ceres/ds/rbtree.h, the way ceres/ds/pqueue.h is the... |
| [`<ceres/ds/oset.h>`](ceres_ds_oset.md) | An ordered set of copies - ceres/ds/omap.h with no value, the tree-backed counterpart to ceres/ds/hset.h and ceres/ds/flatset.h. |
| [`<ceres/ds/pqueue.h>`](ceres_ds_pqueue.md) | A priority queue: a binary min-heap of fixed-size elements ordered by a comparison function. |
| [`<ceres/ds/rbtree.h>`](ceres_ds_rbtree.md) | A red-black tree whose node lives INSIDE the object it orders: no allocation, no copy, the same idea as ceres/ds/list.h taken to a tree - the style Linux's... |
| [`<ceres/ds/ringbuf.h>`](ceres_ds_ringbuf.md) | A first-in, first-out queue of bytes in a fixed buffer: the "input event buffer" - an interrupt handler pushes, the main loop drains. |
| [`<ceres/ds/skiplist.h>`](ceres_ds_skiplist.md) | A skip list: an ordered map by coin flips instead of rotations, documented next to ceres/ds/rbtree.h as the alternative to reach for if a rotation/color bug... |
| [`<ceres/ds/slist.h>`](ceres_ds_slist.md) | A singly linked list whose node lives INSIDE the object it links: no allocation, one pointer per node instead of list.h's two. |
| [`<ceres/ds/slotmap.h>`](ceres_ds_slotmap.md) | Stable handles into a growable array: removing one entity never invalidates another's handle, and a handle kept too long is detectably stale instead of... |
| [`<ceres/ds/stack.h>`](ceres_ds_stack.md) | A LIFO stack of fixed-size elements - built directly on ceres/ds/vector.h (push/pop/last are already exactly a stack's operations), named separately so that... |
| [`<ceres/ds/strbuf.h>`](ceres_ds_strbuf.md) | A string that grows as text is appended, so a message can be built piece by piece without strcat's repeated scans or a buffer size guessed in advance. |
| [`<ceres/ds/trie.h>`](ceres_ds_trie.md) | A trie over ASCII strings: "every word starting with this prefix" is a query neither ceres/ds/hashmap.h nor ceres/ds/omap.h answers well - the tree would... |
| [`<ceres/ds/vector.h>`](ceres_ds_vector.md) | A growable array of fixed-size elements. |
| [`<ceres/f64.h>`](ceres_f64.md) | IEEE 754 binary64 - a real double - in software, on the bits of one in an unsigned long long. |
| [`<ceres/fixed.h>`](ceres_fixed.md) | 16.16 fixed-point arithmetic, for games that want exact, repeatable numbers without the float unit. |
| [`<ceres/font.h>`](ceres_font.md) | An 8x8 bitmap font for the pixel surfaces: printable ASCII (32..126) and Latin-1 (0xA0..0xFF, the code points U+00A0..U+00FF: accented letters, the Spanish... |
| [`<ceres/fs.h>`](ceres_fs.md) | CeresFS: a small file system on a block device (ceres/blockdev.h): the internal disk, a memory stick or a cartridge in a peripheral port. |
| [`<ceres/game.h>`](ceres_game.md) | A fixed-step game loop: input -> logic -> drawing -> present -> wait for the next frame. |
| [`<ceres/gamepad.h>`](ceres_gamepad.md) | Gamepad device (0xFF080000). |
| [`<ceres/gfx.h>`](ceres_gfx.md) | 2D drawing on the pixel display. |
| [`<ceres/hash.h>`](ceres_hash.md) | Hashes and checksums, all in 32-bit arithmetic. |
| [`<ceres/heap.h>`](ceres_heap.md) | Dynamic memory. |
| [`<ceres/hostfs.h>`](ceres_hostfs.md) | Files of the host: semihosting through the machine's host file device (CeresASM 1c51ade). |
| [`<ceres/image.h>`](ceres_image.md) | Images from files and packs into surfaces (ceres/gfx.h), so sprites and tiles need not be compiled in as arrays. |
| [`<ceres/ini.h>`](ceres_ini.md) | INI files, for settings: sections of `key = value` lines. |
| [`<ceres/input.h>`](ceres_input.md) | Input for a frame loop: the state of the keyboard, the mouse and the gamepad as of the last input_update(), with "went down this frame" and "went up this... |
| [`<ceres/irq.h>`](ceres_irq.md) | Interrupt handlers attached at RUN time. |
| [`<ceres/json.h>`](ceres_json.md) | JSON, read and written without the heap. |
| [`<ceres/key.h>`](ceres_key.md) | Keystrokes: what a person types, one at a time, in the order they typed it - for a menu, a text field, a game's title screen. |
| [`<ceres/keyboard.h>`](ceres_keyboard.md) | Keyboard device (0xFF050000). |
| [`<ceres/keys.h>`](ceres_keys.md) | Key codes. |
| [`<ceres/line.h>`](ceres_line.md) | Text input for terminal programs: a whole line, a number, a yes/no, a numbered choice. |
| [`<ceres/lz.h>`](ceres_lz.md) | LZ4: compression that is fast to undo, for assets on a cartridge, saves and anything else worth keeping small. |
| [`<ceres/mmu.h>`](ceres_mmu.md) | The MMU (CeresASM docs/27-Virtual-Memory-and-Paging.md): two-level page tables over 4 KiB pages, off until a program turns it on. |
| [`<ceres/mouse.h>`](ceres_mouse.md) | Mouse device (0xFF060000). |
| [`<ceres/music.h>`](ceres_music.md) | Music and sound effects on the audio device's four channels (CeresASM 848fae2): each a waveform, a volume and an ADSR envelope, mixed by the host. |
| [`<ceres/ns64.h>`](ceres_ns64.md) | A 64-bit unsigned count, in two words: the machine is 32-bit, but `long long` is a real 8-byte type and the arithmetic below runs on it. |
| [`<ceres/pack.h>`](ceres_pack.md) | Resource packs: a program's assets - images, levels, text, music - in one file that is read a piece at a time, from a cartridge in a peripheral port, a... |
| [`<ceres/periph.h>`](ceres_periph.md) | Peripheral ports (0xFF0A0000): media that a person plugs in while the program runs - a memory stick, a game cartridge. |
| [`<ceres/pool.h>`](ceres_pool.md) | Fixed-size blocks with allocation and release in constant time: entities, bullets, particles. |
| [`<ceres/rand.h>`](ceres_rand.md) | Pseudo-random generators with explicit, reproducible state: two runs with the same seed give the same sequence (the VM is deterministic apart from its wall... |
| [`<ceres/save.h>`](ceres_save.md) | Saved games that survive the machine stopping half way through a save. |
| [`<ceres/sort.h>`](ceres_sort.md) | A stable sort: elements that compare equal keep the order they had, which qsort does not promise. |
| [`<ceres/sprite.h>`](ceres_sprite.md) | Sprites (images with one transparent colour), frame animation, tile maps with a camera, and the rectangle tests that games need for collisions. |
| [`<ceres/string_fast.h>`](ceres_string_fast.md) | The word-at-a-time strcpy, strcmp, strchr and memchr (asm/string_fast.casm) ARE the standard functions now: every program gets them through <string.h>. |
| [`<ceres/sys.h>`](ceres_sys.md) | The machine as a program sees it: how to stop it, and where its memory is. |
| [`<ceres/task.h>`](ceres_task.md) | Tasks: coroutines with a scheduler, cooperative. |
| [`<ceres/terminal.h>`](ceres_terminal.md) | Terminal device (0xFF000000). |
| [`<ceres/test.h>`](ceres_test.md) | A minimal test framework. |
| [`<ceres/textfb.h>`](ceres_textfb.md) | Text framebuffer (0xFF030000): a grid of characters a program draws into and then shows. |
| [`<ceres/timer.h>`](ceres_timer.md) | Timer device (0xFF010000). |
| [`<ceres/tui.h>`](ceres_tui.md) | A small text user interface, drawn into the text framebuffer (textfb.h): windows with a title, labels, buttons, a progress bar, a scrolling list and a menu... |
| [`<ceres/utf8.h>`](ceres_utf8.md) | UTF-8, the encoding of every string in this library: source files, the terminal, the files a program writes, and the multibyte strings of <stdlib.h>,... |
| [`<ceres/vecmath.h>`](ceres_vecmath.md) | 2D and 3D vectors passed and returned by value, and the small numeric helpers games keep rewriting. |
