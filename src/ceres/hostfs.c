// The host file device (ceres/hostfs.h): each call writes the registers an operation reads, then the command,
// and returns the result register - a count, a handle or a size, or minus an errno.
#include "ceres/hostfs.h"
#include "ceres.h"
#include "errno.h"

#define HOST_STATUS    (HOSTFS_BASE + 0x00)
#define HOST_COMMAND   (HOSTFS_BASE + 0x04)
#define HOST_HANDLE    (HOSTFS_BASE + 0x08)
#define HOST_ADDRESS   (HOSTFS_BASE + 0x0C)
#define HOST_LENGTH    (HOSTFS_BASE + 0x10)
#define HOST_ARGUMENT  (HOSTFS_BASE + 0x14)
#define HOST_OFFSET    (HOSTFS_BASE + 0x18)
#define HOST_RESULT    (HOSTFS_BASE + 0x1C)

#define CMD_OPEN     1
#define CMD_CLOSE    2
#define CMD_READ     3
#define CMD_WRITE    4
#define CMD_SEEK     5
#define CMD_SIZE     6
#define CMD_REMOVE   7
#define CMD_RENAME   8
#define CMD_STAT     9
#define CMD_LIST     10
#define CMD_MKDIR    11

static void reg(unsigned int address, unsigned int value)
{
    *(volatile unsigned int*)address = value;
}

static int command(unsigned int cmd)
{
    reg(HOST_COMMAND, cmd);
    return (int)*(volatile unsigned int*)HOST_RESULT;
}

int host_available(void)
{
    // An empty slot reads all ones: no device at all.
    unsigned int status = *(volatile unsigned int*)HOST_STATUS;
    return status != 0xFFFFFFFFu && (status & 1u) != 0;
}

// A machine without the device ignores the command and reads the result as all ones, which is -1 (-EPERM):
// report that as -ENODEV, like a device with no directory attached.
static int checked(int result)
{
    if (result == -1 && *(volatile unsigned int*)HOST_STATUS == 0xFFFFFFFFu)
        return -ENODEV;
    return result;
}

int host_open(const char* path, unsigned int flags)
{
    reg(HOST_ADDRESS, (unsigned int)path);
    reg(HOST_ARGUMENT, flags);
    return checked(command(CMD_OPEN));
}

int host_close(int handle)
{
    reg(HOST_HANDLE, (unsigned int)handle);
    return checked(command(CMD_CLOSE));
}

int host_read(int handle, void* buf, unsigned int n)
{
    reg(HOST_HANDLE, (unsigned int)handle);
    reg(HOST_ADDRESS, (unsigned int)buf);
    reg(HOST_LENGTH, n);
    return checked(command(CMD_READ));
}

int host_write(int handle, const void* buf, unsigned int n)
{
    reg(HOST_HANDLE, (unsigned int)handle);
    reg(HOST_ADDRESS, (unsigned int)buf);
    reg(HOST_LENGTH, n);
    return checked(command(CMD_WRITE));
}

int host_seek(int handle, int offset, int whence)
{
    reg(HOST_HANDLE, (unsigned int)handle);
    reg(HOST_OFFSET, (unsigned int)offset);
    reg(HOST_ARGUMENT, (unsigned int)whence);
    return checked(command(CMD_SEEK));
}

int host_size(int handle)
{
    reg(HOST_HANDLE, (unsigned int)handle);
    return checked(command(CMD_SIZE));
}

static int on_path(unsigned int cmd, const char* path)
{
    reg(HOST_ADDRESS, (unsigned int)path);
    return checked(command(cmd));
}

int host_stat(const char* path)   { return on_path(CMD_STAT, path); }
int host_remove(const char* path) { return on_path(CMD_REMOVE, path); }
int host_mkdir(const char* path)  { return on_path(CMD_MKDIR, path); }

int host_rename(const char* from, const char* to)
{
    reg(HOST_ARGUMENT, (unsigned int)to);
    return on_path(CMD_RENAME, from);
}

int host_list(const char* dir, unsigned int index, char* name, unsigned int size)
{
    reg(HOST_ARGUMENT, index);
    reg(HOST_OFFSET, (unsigned int)name);
    reg(HOST_LENGTH, size);
    return on_path(CMD_LIST, dir);
}
