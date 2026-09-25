// An image straight out of a pack. Apart from pack.c, so that a program that reads packs and no images does not
// take the image decoders with it. See ceres/pack.h.
#include "ceres/pack.h"
#include "ceres/image.h"
#include "stdlib.h"
#include "errno.h"

struct gfx_surface* pack_image(struct pack* p, const char* name, unsigned int transparent)
{
    size_t size;
    void* data = pack_load(p, name, &size);
    if (data == 0)
        return 0;
    struct gfx_surface* s = image_decode(data, size, transparent);
    int saved = errno;
    free(data);
    errno = saved;
    return s;
}
