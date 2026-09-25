#pragma once

// mbstate_t, the one definition <uchar.h> and <wchar.h> share: where a multibyte (UTF-8) conversion is between
// calls - the bits of a character read so far, or the units still to hand out.
typedef struct
{
    unsigned int __bits;
    unsigned char __need;       // continuation bytes still to read
    unsigned char __lead;       // the lead byte, until the second byte has been checked against it
    unsigned char __pending;    // units still to hand out (mbrtoc16, mbrtoc8), or 1 when c16rtomb holds a lead
    unsigned char __unused;
} mbstate_t;
