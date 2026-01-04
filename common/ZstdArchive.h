/**
 * @file ZstdArchive.h
 * @brief ZSTA 归档格式 - 基于 Zstd 的轻量级压缩模块
 * @version 1.0
 * 
 * 特性:
 * - 支持单个或多个文件、文件夹压缩
 * - 保留目录结构
 * - UTF-8 路径支持
 * - 固定大小头部和条目表，便于快速索引
 * - 版本控制，支持向后兼容
 */

#ifndef ZSTD_ARCHIVE_H
#define ZSTD_ARCHIVE_H

#include <zstd/zstd.h>
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <ctime>
#endif

/*
=============================================================================
ZSTA 归档格式规范 v1.0
=============================================================================

文件结构:
+---------------------------+
| 文件头 (ZstaHeader)        |  32 字节
+---------------------------+
| 文件条目表 (ZstaEntry[])   |  每条 288 字节
+---------------------------+
| 压缩数据块                  |  连续存放
+---------------------------+

文件头 (32 字节):
  偏移  大小  描述
  0     4    魔数 "ZSTA"
  4     2    主版本号 (当前: 1)
  6     2    次版本号 (当前: 0)
  8     4    文件条目数量
  12    4    压缩级别 (1-22)
  16    8    创建时间戳 (Unix时间戳, 秒)
  24    4    校验和 (CRC32, 保留)
  28    4    保留

文件条目 (288 字节):
  偏移  大小  描述
  0     256  相对路径 (UTF-8, null 结尾)
  256   1    类型: 0=文件, 1=目录
  257   1    压缩方法: 0=存储, 1=zstd
  258   2    文件属性 (保留)
  260   4    CRC32 (保留)
  264   8    原始大小
  272   8    压缩大小 (目录为0)
  280   8    修改时间戳 (Unix时间戳)

数据块:
  按条目顺序连续存放每个文件的压缩数据
  
注意事项:
  - 路径使用 '/' 作为分隔符
  - 目录条目的路径以 '/' 结尾
  - 所有多字节整数使用小端序
=============================================================================
*/

namespace zsta {

// 版本信息
constexpr uint16_t VERSION_MAJOR = 1;
constexpr uint16_t VERSION_MINOR = 0;
constexpr char MAGIC[4] = {'Z', 'S', 'T', 'A'};

// 条目类型
enum class EntryType : uint8_t {
    File = 0,
    Directory = 1
};

// 压缩方法
enum class CompressMethod : uint8_t {
    Store = 0,      // 不压缩
    Zstd = 1        // Zstd 压缩
};

#pragma pack(push, 1)

/**
 * @brief 文件头结构 (32 字节)
 */
struct ZstaHeader {
    char     magic[4];          // 魔数 "ZSTA"
    uint16_t versionMajor;      // 主版本号
    uint16_t versionMinor;      // 次版本号
    uint32_t entryCount;        // 条目数量
    uint32_t compressLevel;     // 压缩级别
    uint64_t createTime;        // 创建时间戳
    uint32_t checksum;          // 校验和 (保留)
    uint32_t reserved;          // 保留
};

/**
 * @brief 文件条目结构 (288 字节)
 */
struct ZstaEntry {
    char     path[256];         // UTF-8 路径
    uint8_t  type;              // 类型: 0=文件, 1=目录
    uint8_t  method;            // 压缩方法
    uint16_t attributes;        // 文件属性 (保留)
    uint32_t crc32;             // CRC32 (保留)
    uint64_t originalSize;      // 原始大小
    uint64_t compressedSize;    // 压缩大小
    uint64_t modifyTime;        // 修改时间戳
};

#pragma pack(pop)

// 静态断言确保结构大小
static_assert(sizeof(ZstaHeader) == 32, "ZstaHeader must be 32 bytes");
static_assert(sizeof(ZstaEntry) == 288, "ZstaEntry must be 288 bytes");

/**
 * @brief 归档信息
 */
struct ArchiveInfo {
    uint16_t versionMajor;
    uint16_t versionMinor;
    uint32_t entryCount;
    uint32_t compressLevel;
    uint64_t createTime;
    uint64_t totalOriginalSize;
    uint64_t totalCompressedSize;
};

/**
 * @brief 条目信息
 */
struct EntryInfo {
    std::string path;
    EntryType type;
    uint64_t originalSize;
    uint64_t compressedSize;
    uint64_t modifyTime;
};

/**
 * @brief 错误码
 */
enum class Error {
    Success = 0,
    FileOpenFailed,
    FileReadFailed,
    FileWriteFailed,
    InvalidFormat,
    UnsupportedVersion,
    DecompressFailed,
    CompressFailed,
    PathTooLong,
    EmptyInput
};

/**
 * @brief ZSTA 归档类
 */
class CZstdArchive {
public:
    /**
     * @brief 压缩多个文件/文件夹
     * @param srcPaths 源路径列表
     * @param outPath 输出文件路径
     * @param level 压缩级别 (1-22, 默认3)
     * @return 错误码
     */
    static Error Compress(const std::vector<std::string>& srcPaths,
                         const std::string& outPath,
                         int level = 3) {
        if (srcPaths.empty()) {
            return Error::EmptyInput;
        }

        // 收集所有文件
        std::vector<FileInfo> files;
        for (const auto& src : srcPaths) {
            std::string baseName = GetFileName(src);
            if (IsDirectory(src)) {
                CollectFiles(src, baseName, files);
            } else {
                FileInfo fi;
                fi.fullPath = src;
                fi.relPath = baseName;
                fi.isDir = false;
                fi.size = GetFileSize(src);
                fi.modTime = GetFileModTime(src);
                files.push_back(fi);
            }
        }

        if (files.empty()) {
            return Error::EmptyInput;
        }

        FILE* fOut = fopen(outPath.c_str(), "wb");
        if (!fOut) {
            return Error::FileOpenFailed;
        }

        // 写文件头
        ZstaHeader header = {};
        memcpy(header.magic, MAGIC, 4);
        header.versionMajor = VERSION_MAJOR;
        header.versionMinor = VERSION_MINOR;
        header.entryCount = static_cast<uint32_t>(files.size());
        header.compressLevel = level;
        header.createTime = static_cast<uint64_t>(time(nullptr));
        
        if (fwrite(&header, sizeof(header), 1, fOut) != 1) {
            fclose(fOut);
            return Error::FileWriteFailed;
        }

        // 预留条目表空间
        long entryTablePos = ftell(fOut);
        std::vector<ZstaEntry> entries(files.size());
        if (fwrite(entries.data(), sizeof(ZstaEntry), files.size(), fOut) != files.size()) {
            fclose(fOut);
            return Error::FileWriteFailed;
        }

        // 压缩并写入数据
        ZSTD_CCtx* cctx = ZSTD_createCCtx();
        Error result = Error::Success;

        for (size_t i = 0; i < files.size(); i++) {
            const auto& f = files[i];
            auto& e = entries[i];

            // 填充条目信息（路径转 UTF-8 存储）
            std::string entryPath = LocalToUtf8(f.relPath);
            if (f.isDir && !entryPath.empty() && entryPath.back() != '/') {
                entryPath += '/';
            }
            
            if (entryPath.length() >= sizeof(e.path)) {
                result = Error::PathTooLong;
                break;
            }
            
            strncpy(e.path, entryPath.c_str(), sizeof(e.path) - 1);
            e.path[sizeof(e.path) - 1] = '\0';
            e.type = f.isDir ? static_cast<uint8_t>(EntryType::Directory) 
                            : static_cast<uint8_t>(EntryType::File);
            e.modifyTime = f.modTime;

            if (f.isDir) {
                e.method = static_cast<uint8_t>(CompressMethod::Store);
                e.originalSize = 0;
                e.compressedSize = 0;
            } else {
                FILE* fIn = fopen(f.fullPath.c_str(), "rb");
                if (!fIn) {
                    e.method = static_cast<uint8_t>(CompressMethod::Store);
                    e.originalSize = 0;
                    e.compressedSize = 0;
                    continue;
                }

                // 获取文件大小
                fseek(fIn, 0, SEEK_END);
                e.originalSize = ftell(fIn);
                fseek(fIn, 0, SEEK_SET);

                if (e.originalSize > 0) {
                    std::vector<char> inBuf(e.originalSize);
                    if (fread(inBuf.data(), 1, e.originalSize, fIn) != e.originalSize) {
                        fclose(fIn);
                        continue;
                    }

                    size_t bound = ZSTD_compressBound(e.originalSize);
                    std::vector<char> outBuf(bound);
                    size_t compSize = ZSTD_compressCCtx(cctx, outBuf.data(), bound,
                                                        inBuf.data(), e.originalSize, level);

                    if (ZSTD_isError(compSize)) {
                        fclose(fIn);
                        result = Error::CompressFailed;
                        break;
                    }

                    // 如果压缩后更大，则存储原始数据
                    if (compSize >= e.originalSize) {
                        e.method = static_cast<uint8_t>(CompressMethod::Store);
                        e.compressedSize = e.originalSize;
                        fwrite(inBuf.data(), 1, e.originalSize, fOut);
                    } else {
                        e.method = static_cast<uint8_t>(CompressMethod::Zstd);
                        e.compressedSize = compSize;
                        fwrite(outBuf.data(), 1, compSize, fOut);
                    }
                } else {
                    e.method = static_cast<uint8_t>(CompressMethod::Store);
                    e.compressedSize = 0;
                }
                fclose(fIn);
            }
        }

        ZSTD_freeCCtx(cctx);

        if (result == Error::Success) {
            // 回写条目表
            fseek(fOut, entryTablePos, SEEK_SET);
            if (fwrite(entries.data(), sizeof(ZstaEntry), files.size(), fOut) != files.size()) {
                result = Error::FileWriteFailed;
            }
        }

        fclose(fOut);
        
        if (result != Error::Success) {
            remove(outPath.c_str());
        }
        
        return result;
    }

    /**
     * @brief 压缩单个文件/文件夹
     */
    static Error Compress(const std::string& srcPath, const std::string& outPath, int level = 3) {
        return Compress(std::vector<std::string>{srcPath}, outPath, level);
    }

    /**
     * @brief 解压归档
     * @param archivePath 归档文件路径
     * @param destDir 目标目录
     * @return 错误码
     */
    static Error Extract(const std::string& archivePath, const std::string& destDir) {
        FILE* fIn = fopen(archivePath.c_str(), "rb");
        if (!fIn) {
            return Error::FileOpenFailed;
        }

        // 读取并验证文件头
        ZstaHeader header;
        memset(&header, 0, sizeof(header));
        size_t bytesRead = fread(&header, 1, sizeof(header), fIn);
        
        // 首先检查魔数（即使文件太短也要先检查已读取的部分）
        if (bytesRead < 4 || memcmp(header.magic, MAGIC, 4) != 0) {
            fclose(fIn);
            return Error::InvalidFormat;
        }
        
        // 然后检查是否读取了完整的头部
        if (bytesRead != sizeof(header)) {
            fclose(fIn);
            return Error::InvalidFormat;
        }

        if (header.versionMajor > VERSION_MAJOR) {
            fclose(fIn);
            return Error::UnsupportedVersion;
        }

        // 读取条目表
        std::vector<ZstaEntry> entries(header.entryCount);
        if (fread(entries.data(), sizeof(ZstaEntry), header.entryCount, fIn) != header.entryCount) {
            fclose(fIn);
            return Error::FileReadFailed;
        }

        CreateDirectoryRecursive(destDir);
        ZSTD_DCtx* dctx = ZSTD_createDCtx();
        Error result = Error::Success;

        for (const auto& e : entries) {
            // 路径从 UTF-8 转为本地编码
            std::string localPath = Utf8ToLocal(e.path);
            std::string fullPath = destDir + "/" + localPath;
            NormalizePath(fullPath);

            if (e.type == static_cast<uint8_t>(EntryType::Directory)) {
                CreateDirectoryRecursive(fullPath);
            } else {
                // 创建父目录
                std::string parent = GetParentPath(fullPath);
                if (!parent.empty()) {
                    CreateDirectoryRecursive(parent);
                }

                if (e.compressedSize > 0) {
                    std::vector<char> compBuf(e.compressedSize);
                    if (fread(compBuf.data(), 1, e.compressedSize, fIn) != e.compressedSize) {
                        result = Error::FileReadFailed;
                        break;
                    }

                    std::vector<char> origBuf(e.originalSize);

                    if (e.method == static_cast<uint8_t>(CompressMethod::Zstd)) {
                        size_t ret = ZSTD_decompressDCtx(dctx, origBuf.data(), e.originalSize,
                                                         compBuf.data(), e.compressedSize);
                        if (ZSTD_isError(ret)) {
                            result = Error::DecompressFailed;
                            break;
                        }
                    } else {
                        // Store 方法，直接复制
                        memcpy(origBuf.data(), compBuf.data(), e.originalSize);
                    }

                    FILE* fOut = fopen(fullPath.c_str(), "wb");
                    if (fOut) {
                        fwrite(origBuf.data(), 1, e.originalSize, fOut);
                        fclose(fOut);
                    }
                } else {
                    // 空文件
                    FILE* fOut = fopen(fullPath.c_str(), "wb");
                    if (fOut) fclose(fOut);
                }
            }
        }

        ZSTD_freeDCtx(dctx);
        fclose(fIn);
        return result;
    }

    /**
     * @brief 获取归档信息
     */
    static Error GetInfo(const std::string& archivePath, ArchiveInfo& info) {
        FILE* fIn = fopen(archivePath.c_str(), "rb");
        if (!fIn) {
            return Error::FileOpenFailed;
        }

        ZstaHeader header;
        if (fread(&header, sizeof(header), 1, fIn) != 1 ||
            memcmp(header.magic, MAGIC, 4) != 0) {
            fclose(fIn);
            return Error::InvalidFormat;
        }

        info.versionMajor = header.versionMajor;
        info.versionMinor = header.versionMinor;
        info.entryCount = header.entryCount;
        info.compressLevel = header.compressLevel;
        info.createTime = header.createTime;
        info.totalOriginalSize = 0;
        info.totalCompressedSize = 0;

        std::vector<ZstaEntry> entries(header.entryCount);
        if (fread(entries.data(), sizeof(ZstaEntry), header.entryCount, fIn) == header.entryCount) {
            for (const auto& e : entries) {
                info.totalOriginalSize += e.originalSize;
                info.totalCompressedSize += e.compressedSize;
            }
        }

        fclose(fIn);
        return Error::Success;
    }

    /**
     * @brief 列出归档内容
     */
    static Error List(const std::string& archivePath, std::vector<EntryInfo>& entries) {
        FILE* fIn = fopen(archivePath.c_str(), "rb");
        if (!fIn) {
            return Error::FileOpenFailed;
        }

        ZstaHeader header;
        if (fread(&header, sizeof(header), 1, fIn) != 1 ||
            memcmp(header.magic, MAGIC, 4) != 0) {
            fclose(fIn);
            return Error::InvalidFormat;
        }

        std::vector<ZstaEntry> rawEntries(header.entryCount);
        if (fread(rawEntries.data(), sizeof(ZstaEntry), header.entryCount, fIn) != header.entryCount) {
            fclose(fIn);
            return Error::FileReadFailed;
        }

        fclose(fIn);

        entries.clear();
        entries.reserve(header.entryCount);
        for (const auto& e : rawEntries) {
            EntryInfo info;
            info.path = Utf8ToLocal(e.path);  // 转为本地编码
            info.type = static_cast<EntryType>(e.type);
            info.originalSize = e.originalSize;
            info.compressedSize = e.compressedSize;
            info.modifyTime = e.modifyTime;
            entries.push_back(info);
        }

        return Error::Success;
    }

    /**
     * @brief 获取错误描述
     */
    static const char* GetErrorString(Error err) {
        switch (err) {
            case Error::Success:            return "Success";
            case Error::FileOpenFailed:     return "Failed to open file";
            case Error::FileReadFailed:     return "Failed to read file";
            case Error::FileWriteFailed:    return "Failed to write file";
            case Error::InvalidFormat:      return "Invalid archive format";
            case Error::UnsupportedVersion: return "Unsupported archive version";
            case Error::DecompressFailed:   return "Decompression failed";
            case Error::CompressFailed:     return "Compression failed";
            case Error::PathTooLong:        return "Path too long";
            case Error::EmptyInput:         return "Empty input";
            default:                        return "Unknown error";
        }
    }

private:
    struct FileInfo {
        std::string fullPath;
        std::string relPath;
        bool isDir;
        uint64_t size;
        uint64_t modTime;
    };

#ifdef _WIN32
    // MBCS -> UTF-8 (压缩时用，存入归档)
    static std::string LocalToUtf8(const std::string& local) {
        if (local.empty()) return "";
        int wlen = MultiByteToWideChar(CP_ACP, 0, local.c_str(), -1, NULL, 0);
        if (wlen <= 0) return local;
        std::wstring wide(wlen - 1, 0);
        MultiByteToWideChar(CP_ACP, 0, local.c_str(), -1, &wide[0], wlen);
        int ulen = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, NULL, 0, NULL, NULL);
        if (ulen <= 0) return local;
        std::string utf8(ulen - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &utf8[0], ulen, NULL, NULL);
        return utf8;
    }

    // UTF-8 -> MBCS (解压时用，写入文件系统)
    static std::string Utf8ToLocal(const std::string& utf8) {
        if (utf8.empty()) return "";
        int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, NULL, 0);
        if (wlen <= 0) return utf8;
        std::wstring wide(wlen - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wide[0], wlen);
        int llen = WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, NULL, 0, NULL, NULL);
        if (llen <= 0) return utf8;
        std::string local(llen - 1, 0);
        WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, &local[0], llen, NULL, NULL);
        return local;
    }
#else
    // Linux/macOS 默认 UTF-8
    static std::string LocalToUtf8(const std::string& s) { return s; }
    static std::string Utf8ToLocal(const std::string& s) { return s; }
#endif

    static bool IsDirectory(const std::string& path) {
#ifdef _WIN32
        DWORD attr = GetFileAttributesA(path.c_str());
        return (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
#else
        struct stat st;
        if (stat(path.c_str(), &st) != 0) return false;
        return S_ISDIR(st.st_mode);
#endif
    }

    static uint64_t GetFileSize(const std::string& path) {
#ifdef _WIN32
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fad)) return 0;
        return (static_cast<uint64_t>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
#else
        struct stat st;
        if (stat(path.c_str(), &st) != 0) return 0;
        return st.st_size;
#endif
    }

    static uint64_t GetFileModTime(const std::string& path) {
#ifdef _WIN32
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fad)) return 0;
        // 转换 FILETIME 到 Unix 时间戳
        ULARGE_INTEGER ull;
        ull.LowPart = fad.ftLastWriteTime.dwLowDateTime;
        ull.HighPart = fad.ftLastWriteTime.dwHighDateTime;
        return (ull.QuadPart - 116444736000000000ULL) / 10000000ULL;
#else
        struct stat st;
        if (stat(path.c_str(), &st) != 0) return 0;
        return st.st_mtime;
#endif
    }

    static std::string GetFileName(const std::string& path) {
        // 先移除末尾的斜杠
        std::string p = path;
        while (!p.empty() && (p.back() == '/' || p.back() == '\\')) {
            p.pop_back();
        }
        if (p.empty()) return "";
        
        size_t pos = p.find_last_of("/\\");
        if (pos != std::string::npos) {
            return p.substr(pos + 1);
        }
        return p;
    }

    static std::string GetParentPath(const std::string& path) {
        size_t pos = path.find_last_of("/\\");
        if (pos != std::string::npos && pos > 0) {
            return path.substr(0, pos);
        }
        return "";
    }

    static void NormalizePath(std::string& path) {
        for (char& c : path) {
#ifdef _WIN32
            if (c == '/') c = '\\';
#else
            if (c == '\\') c = '/';
#endif
        }
        // 移除末尾的分隔符（除非是根目录）
        while (path.size() > 1 && (path.back() == '/' || path.back() == '\\')) {
            path.pop_back();
        }
    }

    static void CollectFiles(const std::string& dir, const std::string& rel,
                            std::vector<FileInfo>& files) {
        // 添加目录本身
        FileInfo dirInfo;
        dirInfo.fullPath = dir;
        dirInfo.relPath = rel;
        dirInfo.isDir = true;
        dirInfo.size = 0;
        dirInfo.modTime = GetFileModTime(dir);
        files.push_back(dirInfo);

#ifdef _WIN32
        WIN32_FIND_DATAA fd;
        std::string pattern = dir + "\\*";
        HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) return;

        do {
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
                continue;

            std::string fullPath = dir + "\\" + fd.cFileName;
            std::string relPath = rel + "/" + fd.cFileName;

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                CollectFiles(fullPath, relPath, files);
            } else {
                FileInfo fi;
                fi.fullPath = fullPath;
                fi.relPath = relPath;
                fi.isDir = false;
                fi.size = (static_cast<uint64_t>(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
                // 转换 FILETIME
                ULARGE_INTEGER ull;
                ull.LowPart = fd.ftLastWriteTime.dwLowDateTime;
                ull.HighPart = fd.ftLastWriteTime.dwHighDateTime;
                fi.modTime = (ull.QuadPart - 116444736000000000ULL) / 10000000ULL;
                files.push_back(fi);
            }
        } while (FindNextFileA(h, &fd));

        FindClose(h);
#else
        DIR* d = opendir(dir.c_str());
        if (!d) return;

        struct dirent* entry;
        while ((entry = readdir(d)) != nullptr) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            std::string fullPath = dir + "/" + entry->d_name;
            std::string relPath = rel + "/" + entry->d_name;

            struct stat st;
            if (stat(fullPath.c_str(), &st) != 0) continue;

            if (S_ISDIR(st.st_mode)) {
                CollectFiles(fullPath, relPath, files);
            } else {
                FileInfo fi;
                fi.fullPath = fullPath;
                fi.relPath = relPath;
                fi.isDir = false;
                fi.size = st.st_size;
                fi.modTime = st.st_mtime;
                files.push_back(fi);
            }
        }

        closedir(d);
#endif
    }

    static void CreateDirectoryRecursive(const std::string& path) {
        if (path.empty()) return;
        
        std::string normalized = path;
        NormalizePath(normalized);
        
        size_t pos = 0;
        while ((pos = normalized.find_first_of("/\\", pos + 1)) != std::string::npos) {
            std::string sub = normalized.substr(0, pos);
#ifdef _WIN32
            CreateDirectoryA(sub.c_str(), nullptr);
#else
            mkdir(sub.c_str(), 0755);
#endif
        }
#ifdef _WIN32
        CreateDirectoryA(normalized.c_str(), nullptr);
#else
        mkdir(normalized.c_str(), 0755);
#endif
    }
};

} // namespace zsta

#endif // ZSTD_ARCHIVE_H
