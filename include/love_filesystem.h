#ifndef LOVE_FILESYSTEM_H
#define LOVE_FILESYSTEM_H

#include <string>
#include <vector>
#include <stdint.h>
#include <stddef.h>

struct FileInfo {
    std::string type; // "file" or "directory"
    size_t size;
    int64_t modtime;
};

class LoveFilesystem {
public:
    static bool init(const char *game_path);
    static void deinit();

    static bool mount(const char *path, const char *mount_point);
    static const char *getSourceBaseDirectory();

    static bool read(const char *path, void **out_data, size_t *out_size);
    static void freeData(void *data);

    static bool write(const char *path, const void *data, size_t size);
    static bool append(const char *path, const void *data, size_t size);
    static bool remove(const char *path);
    static bool createDirectory(const char *path);

    static bool getInfo(const char *path, FileInfo *info);
    static bool getDirectoryItems(const char *path, std::vector<std::string> &items);

private:
    static std::string s_baseDir;
    static std::string s_saveDir;
    static void *s_zipArchive; // mz_zip_archive* if mounted from .love
    static bool s_isZip;

    static std::string resolvePath(const char *path);
};

#endif // LOVE_FILESYSTEM_H
