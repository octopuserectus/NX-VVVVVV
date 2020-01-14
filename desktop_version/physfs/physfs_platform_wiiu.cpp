#define __PHYSICSFS_INTERNAL__
#include "physfs_platforms.h"

#ifdef PHYSFS_PLATFORM_WIIU

#include <dirent.h>
#include <errno.h>
#include <mutex>
#include <stdio.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

#include "physfs_internal.h"

static inline PHYSFS_ErrorCode errcodeFromErrno(void)
{
    switch (errno)
    {
        case 0: return PHYSFS_ERR_OK;
        case EACCES: return PHYSFS_ERR_PERMISSION;
        case EPERM: return PHYSFS_ERR_PERMISSION;
        case EDQUOT: return PHYSFS_ERR_NO_SPACE;
        case EIO: return PHYSFS_ERR_IO;
        case ELOOP: return PHYSFS_ERR_SYMLINK_LOOP;
        case EMLINK: return PHYSFS_ERR_NO_SPACE;
        case ENAMETOOLONG: return PHYSFS_ERR_BAD_FILENAME;
        case ENOENT: return PHYSFS_ERR_NOT_FOUND;
        case ENOSPC: return PHYSFS_ERR_NO_SPACE;
        case ENOTDIR: return PHYSFS_ERR_NOT_FOUND;
        case EISDIR: return PHYSFS_ERR_NOT_A_FILE;
        case EROFS: return PHYSFS_ERR_READ_ONLY;
        case ETXTBSY: return PHYSFS_ERR_BUSY;
        case EBUSY: return PHYSFS_ERR_BUSY;
        case ENOMEM: return PHYSFS_ERR_OUT_OF_MEMORY;
        case ENOTEMPTY: return PHYSFS_ERR_DIR_NOT_EMPTY;
        default: return PHYSFS_ERR_OS_ERROR;
    }
}

char * __PHYSFS_platformCalcUserDir(void)
{
    char * retval = (char *) allocator.Malloc(20);
    strcpy(retval, "fs:/vol/external01/");
    return retval;
}

PHYSFS_EnumerateCallbackResult __PHYSFS_platformEnumerate(
    const char * dirname,
    PHYSFS_EnumerateCallback callback,
    const char * origdir,
    void * callbackdata)
{
    DIR * dir;
    struct dirent * ent;
    PHYSFS_EnumerateCallbackResult retval = PHYSFS_ENUM_OK;

    dir = opendir(dirname);
    BAIL_IF(dir == NULL, errcodeFromErrno(), PHYSFS_ENUM_ERROR);

    while ((retval == PHYSFS_ENUM_OK) && ((ent = readdir(dir)) != NULL))
    {
        const char * name = ent->d_name;
        if (name[0] == '.')  /* ignore "." and ".." */
        {
            if ((name[1] == '\0') || ((name[1] == '.') && (name[2] == '\0')))
                continue;
        }

        retval = callback(callbackdata, origdir, name);
        if (retval == PHYSFS_ENUM_ERROR)
            PHYSFS_setErrorCode(PHYSFS_ERR_APP_CALLBACK);
    }

    closedir(dir);

    return retval;
}

int __PHYSFS_platformMkDir(const char * path)
{
    const int rc = mkdir(path, S_IRWXU);
    BAIL_IF(rc == -1, errcodeFromErrno(), 0);
    return 1;
}

void * __PHYSFS_platformOpenRead(const char *filename)
{
    return fopen(filename, "r");
}

void * __PHYSFS_platformOpenWrite(const char *filename)
{
    return fopen(filename, "w+");
}

void * __PHYSFS_platformOpenAppend(const char *filename)
{
    return fopen(filename, "a+");
}

PHYSFS_sint64 __PHYSFS_platformRead(
    void * opaque,
    void * buffer,
    PHYSFS_uint64 len)
{
    FILE * fp = (FILE *) opaque;

    if (!__PHYSFS_ui64FitsAddressSpace(len))
        BAIL(PHYSFS_ERR_INVALID_ARGUMENT, -1);

    size_t rc = fread(buffer, 1, len, fp);
    BAIL_IF(rc < 0, errcodeFromErrno(), -1);
    assert(rc >= 0);
    assert(rc <= len);
    return (PHYSFS_sint64) rc;
}

PHYSFS_sint64 __PHYSFS_platformWrite(
    void * opaque,
    const void * buffer,
    PHYSFS_uint64 len)
{
    FILE * fp = (FILE *) opaque;

    if (!__PHYSFS_ui64FitsAddressSpace(len))
        BAIL(PHYSFS_ERR_INVALID_ARGUMENT, -1);

    size_t rc = fwrite(buffer, 1, len, fp);
    BAIL_IF(rc < 0, errcodeFromErrno(), rc);
    assert(rc >= 0);
    assert(rc <= len);
    return (PHYSFS_sint64) rc;
}

int __PHYSFS_platformSeek(void * opaque, PHYSFS_uint64 pos)
{
    FILE * fp = (FILE *) opaque;
    int rc = fseek(fp, pos, SEEK_SET);
    BAIL_IF(rc != 0, errcodeFromErrno(), 0);
    return 1;
}

PHYSFS_sint64 __PHYSFS_platformTell(void * opaque)
{
    FILE * fp = (FILE *) opaque;
    int rc = ftell(fp);
    BAIL_IF(rc != 0, errcodeFromErrno(), 0);
    return rc;
}

PHYSFS_sint64 __PHYSFS_platformFileLength(void * opaque)
{
    FILE * fp = (FILE *) opaque;
    int pos = fseek(fp, 0, SEEK_CUR);
    fseek(fp, 0, SEEK_END);
    int rc = ftell(fp);
    fseek(fp, pos, SEEK_SET);
    return rc;
}

int __PHYSFS_platformFlush(void * opaque)
{
    FILE * fp = (FILE *) opaque;
    int rc = fflush(fp);
    BAIL_IF(rc != 0, errcodeFromErrno(), 0);
    return 1;
}

void __PHYSFS_platformClose(void * opaque)
{
    FILE * fp = (FILE *) opaque;
    fclose(fp);
}

int __PHYSFS_platformDelete(const char * path)
{
    BAIL_IF(remove(path) == -1, errcodeFromErrno(), 0);
    return 1;
}

int __PHYSFS_platformStat(const char *fname, PHYSFS_Stat *st, const int follow)
{
    struct stat statbuf;
    const int rc = follow ? stat(fname, &statbuf) : lstat(fname, &statbuf);
    BAIL_IF(rc == -1, errcodeFromErrno(), 0);

    if (S_ISREG(statbuf.st_mode))
    {
        st->filetype = PHYSFS_FILETYPE_REGULAR;
        st->filesize = statbuf.st_size;
    }
    else if(S_ISDIR(statbuf.st_mode))
    {
        st->filetype = PHYSFS_FILETYPE_DIRECTORY;
        st->filesize = 0;
    }
    else if(S_ISLNK(statbuf.st_mode))
    {
        st->filetype = PHYSFS_FILETYPE_SYMLINK;
        st->filesize = 0;
    }
    else
    {
        st->filetype = PHYSFS_FILETYPE_OTHER;
        st->filesize = statbuf.st_size;
    } 

    st->modtime = statbuf.st_mtime;
    st->createtime = statbuf.st_ctime;
    st->accesstime = statbuf.st_atime;

    st->readonly = (access(fname, W_OK) == -1);
    return 1;
}

void * __PHYSFS_platformGetThreadID(void)
{
    std::thread::id * retval = (std::thread::id *) allocator.Malloc(sizeof(std::thread::id));
    * retval = std::this_thread::get_id();
    return retval;
}

void * __PHYSFS_platformCreateMutex(void)
{
    return new std::mutex();
}

void __PHYSFS_platformDestroyMutex(void * mutex)
{
    delete (std::mutex *) mutex;
}

int __PHYSFS_platformGrabMutex(void * mutex)
{
    std::mutex * m = (std::mutex *) mutex;
    
    try {
        m->lock();
    }
    catch (const std::system_error & error)
    {
        return 0;
    }

    return 1;
}

void __PHYSFS_platformReleaseMutex(void * mutex)
{
    std::mutex * m = (std::mutex *) mutex;
    m->unlock();
}

#endif