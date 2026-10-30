#include "love_filesystem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include "miniz.h"

std::string LoveFilesystem::s_baseDir = ".";
std::string LoveFilesystem::s_saveDir = "./saves";
void *LoveFilesystem::s_zipArchive = NULL;
bool LoveFilesystem::s_isZip = false;

static mz_zip_archive g_zip;

bool LoveFilesystem::init(const char *game_path) {
    if (!game_path) game_path = ".";
    
    // Check if game_path is a .love or .zip file
    size_t len = strlen(game_path);
    if ((len > 5 && strcmp(game_path + len - 5, ".love") == 0) ||
        (len > 4 && strcmp(game_path + len - 4, ".zip") == 0)) {
        
        memset(&g_zip, 0, sizeof(g_zip));
        if (mz_zip_reader_init_file(&g_zip, game_path, 0)) {
            s_zipArchive = &g_zip;
            s_isZip = true;
            s_baseDir = game_path;
            printf("[LOVE Filesystem] Mounted zip archive: %s (%u files)\n", game_path, mz_zip_reader_get_num_files(&g_zip));
            return true;
        } else {
            printf("[LOVE Filesystem] Failed to open zip archive: %s\n", game_path);
        }
    }

    // Otherwise standard directory
    s_baseDir = game_path;
    s_isZip = false;
    printf("[LOVE Filesystem] Mounted directory: %s\n", game_path);
    return true;
}

void LoveFilesystem::deinit() {
    if (s_isZip && s_zipArchive) {
        mz_zip_reader_end(&g_zip);
        s_zipArchive = NULL;
        s_isZip = false;
    }
}

const char *LoveFilesystem::getSourceBaseDirectory() {
    return s_baseDir.c_str();
}

std::string LoveFilesystem::resolvePath(const char *path) {
    if (!path || path[0] == '\0') return s_baseDir;
    if (path[0] == '/') path++;
    return s_baseDir + "/" + path;
}

bool LoveFilesystem::read(const char *path, void **out_data, size_t *out_size) {
    if (!path || !out_data || !out_size) return false;
    if (path[0] == '/') path++;

    if (s_isZip && s_zipArchive) {
        // Find in zip
        int file_index = mz_zip_reader_locate_file(&g_zip, path, NULL, 0);
        if (file_index < 0) {
            // Try with leading slash stripped or lowercase
            return false;
        }

        size_t uncomp_size = 0;
        void *p = mz_zip_reader_extract_to_heap(&g_zip, file_index, &uncomp_size, 0);
        if (!p) return false;

        *out_data = p;
        *out_size = uncomp_size;
        return true;
    }

    std::string full = resolvePath(path);
    FILE *f = fopen(full.c_str(), "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz < 0) {
        fclose(f);
        return false;
    }

    void *buf = malloc(sz + 1);
    if (!buf) {
        fclose(f);
        return false;
    }

    size_t read_bytes = fread(buf, 1, sz, f);
    fclose(f);

    ((char*)buf)[read_bytes] = '\0';
    *out_data = buf;
    *out_size = read_bytes;
    return true;
}

void LoveFilesystem::freeData(void *data) {
    if (data) free(data);
}

bool LoveFilesystem::write(const char *path, const void *data, size_t size) {
    if (!path) return false;
    std::string full = resolvePath(path);
    FILE *f = fopen(full.c_str(), "wb");
    if (!f) return false;
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    return (written == size);
}

bool LoveFilesystem::append(const char *path, const void *data, size_t size) {
    if (!path) return false;
    std::string full = resolvePath(path);
    FILE *f = fopen(full.c_str(), "ab");
    if (!f) return false;
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    return (written == size);
}

bool LoveFilesystem::remove(const char *path) {
    if (!path) return false;
    std::string full = resolvePath(path);
    return (::remove(full.c_str()) == 0);
}

bool LoveFilesystem::createDirectory(const char *path) {
    if (!path) return false;
    std::string full = resolvePath(path);
    return (mkdir(full.c_str(), 0755) == 0);
}

bool LoveFilesystem::getInfo(const char *path, FileInfo *info) {
    if (!path || !info) return false;
    if (path[0] == '/') path++;

    if (s_isZip && s_zipArchive) {
        int file_index = mz_zip_reader_locate_file(&g_zip, path, NULL, 0);
        if (file_index >= 0) {
            mz_zip_archive_file_stat st;
            if (mz_zip_reader_file_stat(&g_zip, file_index, &st)) {
                info->type = st.m_is_directory ? "directory" : "file";
                info->size = (size_t)st.m_uncomp_size;
                info->modtime = (int64_t)st.m_time;
                return true;
            }
        }
    }

    std::string full = resolvePath(path);
    FILE *f = fopen(full.c_str(), "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        info->size = (size_t)ftell(f);
        fclose(f);
        info->type = "file";
        info->modtime = 0;
        return true;
    }
    struct stat st;
    if (stat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        info->type = "directory";
        info->size = 0;
        info->modtime = 0;
        return true;
    }
    return false;
}

bool LoveFilesystem::getDirectoryItems(const char *path, std::vector<std::string> &items) {
    items.clear();
    if (!path) path = "";
    if (path[0] == '/') path++;

    if (s_isZip && s_zipArchive) {
        std::string prefix = path;
        if (!prefix.empty() && prefix.back() != '/') prefix += '/';
        size_t prefix_len = prefix.length();

        mz_uint num_files = mz_zip_reader_get_num_files(&g_zip);
        for (mz_uint i = 0; i < num_files; i++) {
            mz_zip_archive_file_stat st;
            if (mz_zip_reader_file_stat(&g_zip, i, &st)) {
                const char *fn = st.m_filename;
                if (prefix_len == 0 || strncmp(fn, prefix.c_str(), prefix_len) == 0) {
                    const char *sub = fn + prefix_len;
                    // Skip nested directories
                    const char *slash = strchr(sub, '/');
                    if (!slash || *(slash + 1) == '\0') {
                        std::string item_name = sub;
                        if (!item_name.empty() && item_name.back() == '/') item_name.pop_back();
                        if (!item_name.empty()) {
                            items.push_back(item_name);
                        }
                    }
                }
            }
        }
        return true;
    }

    std::string full = resolvePath(path);
    DIR *dir = opendir(full.c_str());
    if (!dir) return false;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;
        items.push_back(ent->d_name);
    }
    closedir(dir);
    return true;
}
