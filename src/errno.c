#include "errno.h"
#include "string.h"

int errno = 0;

// The message for an error code, as strerror() returns it. The strings are the traditional POSIX ones
// where a code has one; the rest are named for what Ceres means by them.
char* strerror(int e)
{
    switch (e)
    {
    case 0:            return "Success";
    case EPERM:        return "Operation not permitted";
    case ENOENT:       return "No such file or directory";
    case EINTR:        return "Interrupted";
    case EIO:          return "Input/output error";
    case ENXIO:        return "No such device or address";
    case E2BIG:        return "Argument list too long";
    case EBADF:        return "Bad file descriptor";
    case EAGAIN:       return "Try again";
    case ENOMEM:       return "Out of memory";
    case EACCES:       return "Permission denied";
    case EBUSY:        return "Device or resource busy";
    case EEXIST:       return "File exists";
    case ENODEV:       return "No such device";
    case ENOTDIR:      return "Not a directory";
    case EISDIR:       return "Is a directory";
    case EINVAL:       return "Invalid argument";
    case ENFILE:       return "Too many open files in system";
    case EMFILE:       return "Too many open files";
    case EFBIG:        return "File too large";
    case ENOSPC:       return "No space left on device";
    case ESPIPE:       return "Illegal seek";
    case EROFS:        return "Read-only file system";
    case EPIPE:        return "Broken pipe";
    case EDOM:         return "Domain error";
    case ERANGE:       return "Result out of range";
    case ENAMETOOLONG: return "File name too long";
    case ENOSYS:       return "Function not implemented";
    case ENOTEMPTY:    return "Directory not empty";
    case EXDEV:        return "Invalid cross-device link";
    case EOVERFLOW:    return "Value too large for defined data type";
    case EILSEQ:       return "Illegal byte sequence";
    case ETIMEDOUT:    return "Timed out";
    }
    return "Unknown error";
}
