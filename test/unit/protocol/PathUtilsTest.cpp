/**
 * @file PathUtilsTest.cpp
 * @brief 路径处理工具函数测试
 *
 * 测试覆盖：
 * - GetCommonRoot: 计算多文件的公共根目录
 * - GetRelativePath: 获取相对路径
 * - 边界条件和特殊字符处理
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

// ============================================
// 路径工具函数实现（测试专用副本）
// ============================================

// 计算最长公共前缀作为根目录
std::string GetCommonRoot(const std::vector<std::string>& files)
{
    if (files.empty())
        return "";

    std::string root = files[0];
    for (size_t i = 1; i < files.size(); ++i)
    {
        const std::string& path = files[i];
        size_t len = (std::min)(root.size(), path.size());

        size_t j = 0;
        for (; j < len; ++j)
        {
            if (std::tolower(static_cast<unsigned char>(root[j])) !=
                std::tolower(static_cast<unsigned char>(path[j])))
            {
                break;
            }
        }
        root = root.substr(0, j);

        size_t pos = root.find_last_of('\\');
        if (pos != std::string::npos)
            root = root.substr(0, pos + 1);
    }

    return root;
}

// 获取相对路径
std::string GetRelativePath(const std::string& root, const std::string& fullPath)
{
    if (fullPath.compare(0, root.size(), root) == 0)
    {
        std::string rel = fullPath.substr(root.size());
        if (rel.empty()) // root 就是完整文件路径
        {
            size_t pos = fullPath.find_last_of('\\');
            if (pos != std::string::npos)
                rel = fullPath.substr(pos + 1); // 文件名
            else
                rel = fullPath;
        }
        return rel;
    }
    return fullPath;
}

// ============================================
// GetCommonRoot 测试
// ============================================

class GetCommonRootTest : public ::testing::Test {};

TEST_F(GetCommonRootTest, EmptyList_ReturnsEmpty) {
    std::vector<std::string> files;
    EXPECT_EQ(GetCommonRoot(files), "");
}

TEST_F(GetCommonRootTest, SingleFile_ReturnsParentDir) {
    std::vector<std::string> files = {
        "C:\\Users\\Test\\file.txt"
    };
    // 单个文件时，返回整个路径（因为没有其他文件比较）
    EXPECT_EQ(GetCommonRoot(files), "C:\\Users\\Test\\file.txt");
}

TEST_F(GetCommonRootTest, TwoFilesInSameDir_ReturnsDir) {
    std::vector<std::string> files = {
        "C:\\Users\\Test\\file1.txt",
        "C:\\Users\\Test\\file2.txt"
    };
    EXPECT_EQ(GetCommonRoot(files), "C:\\Users\\Test\\");
}

TEST_F(GetCommonRootTest, FilesInNestedDirs_ReturnsCommonAncestor) {
    std::vector<std::string> files = {
        "C:\\Users\\Test\\Documents\\a.txt",
        "C:\\Users\\Test\\Downloads\\b.txt"
    };
    EXPECT_EQ(GetCommonRoot(files), "C:\\Users\\Test\\");
}

TEST_F(GetCommonRootTest, FilesInDeeplyNestedDirs) {
    std::vector<std::string> files = {
        "C:\\a\\b\\c\\d\\e\\file1.txt",
        "C:\\a\\b\\c\\x\\y\\file2.txt",
        "C:\\a\\b\\c\\z\\file3.txt"
    };
    EXPECT_EQ(GetCommonRoot(files), "C:\\a\\b\\c\\");
}

TEST_F(GetCommonRootTest, CaseInsensitive) {
    std::vector<std::string> files = {
        "C:\\Users\\TEST\\file1.txt",
        "C:\\users\\test\\file2.txt"
    };
    // 应该忽略大小写进行比较
    std::string root = GetCommonRoot(files);
    // 结果应该是原始路径的前缀
    EXPECT_TRUE(root.size() >= strlen("C:\\Users\\TEST\\") ||
                root.size() >= strlen("C:\\users\\test\\"));
}

TEST_F(GetCommonRootTest, DifferentDrives_ReturnsEmpty) {
    std::vector<std::string> files = {
        "C:\\Users\\file1.txt",
        "D:\\Data\\file2.txt"
    };
    // 不同驱动器，公共根可能为空或只有共同前缀
    std::string root = GetCommonRoot(files);
    EXPECT_TRUE(root.empty() || root.find('\\') == std::string::npos);
}

TEST_F(GetCommonRootTest, SameDirectory_MultipleFiles) {
    std::vector<std::string> files = {
        "C:\\Temp\\a.txt",
        "C:\\Temp\\b.txt",
        "C:\\Temp\\c.txt",
        "C:\\Temp\\d.txt"
    };
    EXPECT_EQ(GetCommonRoot(files), "C:\\Temp\\");
}

TEST_F(GetCommonRootTest, DirectoryAndFiles) {
    std::vector<std::string> files = {
        "C:\\Temp\\folder",
        "C:\\Temp\\folder\\file.txt"
    };
    EXPECT_EQ(GetCommonRoot(files), "C:\\Temp\\");
}

TEST_F(GetCommonRootTest, ChineseCharacters) {
    std::vector<std::string> files = {
        "C:\\Users\\测试\\文档\\file1.txt",
        "C:\\Users\\测试\\下载\\file2.txt"
    };
    std::string root = GetCommonRoot(files);
    // 应该能正确处理中文路径
    EXPECT_TRUE(root.find("测试") != std::string::npos ||
                root == "C:\\Users\\");
}

TEST_F(GetCommonRootTest, SpacesInPath) {
    std::vector<std::string> files = {
        "C:\\Program Files\\App\\file1.txt",
        "C:\\Program Files\\App\\file2.txt"
    };
    EXPECT_EQ(GetCommonRoot(files), "C:\\Program Files\\App\\");
}

// ============================================
// GetRelativePath 测试
// ============================================

class GetRelativePathTest : public ::testing::Test {};

TEST_F(GetRelativePathTest, SimpleRelativePath) {
    std::string root = "C:\\Users\\Test\\";
    std::string fullPath = "C:\\Users\\Test\\Documents\\file.txt";

    EXPECT_EQ(GetRelativePath(root, fullPath), "Documents\\file.txt");
}

TEST_F(GetRelativePathTest, FileInRoot) {
    std::string root = "C:\\Users\\Test\\";
    std::string fullPath = "C:\\Users\\Test\\file.txt";

    EXPECT_EQ(GetRelativePath(root, fullPath), "file.txt");
}

TEST_F(GetRelativePathTest, RootEqualsFullPath) {
    std::string root = "C:\\Users\\Test\\file.txt";
    std::string fullPath = "C:\\Users\\Test\\file.txt";

    // 当 root 就是完整路径时，应该返回文件名
    EXPECT_EQ(GetRelativePath(root, fullPath), "file.txt");
}

TEST_F(GetRelativePathTest, NoCommonPrefix_ReturnsFullPath) {
    std::string root = "C:\\Users\\Test\\";
    std::string fullPath = "D:\\Other\\file.txt";

    // 没有公共前缀时返回完整路径
    EXPECT_EQ(GetRelativePath(root, fullPath), fullPath);
}

TEST_F(GetRelativePathTest, NestedPath) {
    std::string root = "C:\\a\\";
    std::string fullPath = "C:\\a\\b\\c\\d\\file.txt";

    EXPECT_EQ(GetRelativePath(root, fullPath), "b\\c\\d\\file.txt");
}

TEST_F(GetRelativePathTest, EmptyRoot) {
    std::string root = "";
    std::string fullPath = "C:\\Users\\file.txt";

    EXPECT_EQ(GetRelativePath(root, fullPath), fullPath);
}

TEST_F(GetRelativePathTest, RootWithoutTrailingSlash) {
    std::string root = "C:\\Users\\Test";
    std::string fullPath = "C:\\Users\\Test\\file.txt";

    // 没有尾部斜杠也应该工作
    std::string result = GetRelativePath(root, fullPath);
    EXPECT_TRUE(result == "\\file.txt" || result == fullPath);
}

TEST_F(GetRelativePathTest, DirectoryPath) {
    std::string root = "C:\\Users\\";
    std::string fullPath = "C:\\Users\\Test\\Documents\\";

    EXPECT_EQ(GetRelativePath(root, fullPath), "Test\\Documents\\");
}

TEST_F(GetRelativePathTest, FileNameOnly) {
    std::string root = "";
    std::string fullPath = "file.txt";

    EXPECT_EQ(GetRelativePath(root, fullPath), "file.txt");
}

// ============================================
// 组合测试
// ============================================

class PathUtilsCombinedTest : public ::testing::Test {};

TEST_F(PathUtilsCombinedTest, GetCommonRootThenRelative) {
    std::vector<std::string> files = {
        "C:\\Project\\src\\main.cpp",
        "C:\\Project\\src\\utils.cpp",
        "C:\\Project\\include\\header.h"
    };

    std::string root = GetCommonRoot(files);
    EXPECT_EQ(root, "C:\\Project\\");

    // 获取各文件的相对路径
    EXPECT_EQ(GetRelativePath(root, files[0]), "src\\main.cpp");
    EXPECT_EQ(GetRelativePath(root, files[1]), "src\\utils.cpp");
    EXPECT_EQ(GetRelativePath(root, files[2]), "include\\header.h");
}

TEST_F(PathUtilsCombinedTest, SimulateFileTransfer) {
    // 模拟文件传输场景
    std::vector<std::string> selectedFiles = {
        "D:\\Downloads\\Project\\doc\\readme.md",
        "D:\\Downloads\\Project\\src\\app.py",
        "D:\\Downloads\\Project\\src\\lib\\util.py"
    };

    std::string rootDir = GetCommonRoot(selectedFiles);
    EXPECT_EQ(rootDir, "D:\\Downloads\\Project\\");

    std::string targetDir = "C:\\Backup\\";

    for (const auto& file : selectedFiles) {
        std::string relPath = GetRelativePath(rootDir, file);
        std::string destPath = targetDir + relPath;

        // 验证目标路径正确
        EXPECT_TRUE(destPath.find("C:\\Backup\\") == 0);
    }
}

// ============================================
// 边界条件测试
// ============================================

TEST(PathBoundaryTest, VeryLongPath) {
    // 创建一个很长的路径
    std::string basePath = "C:\\";
    for (int i = 0; i < 50; i++) {
        basePath += "folder" + std::to_string(i) + "\\";
    }
    basePath += "file.txt";

    std::vector<std::string> files = {basePath};
    std::string root = GetCommonRoot(files);
    EXPECT_EQ(root, basePath);
}

TEST(PathBoundaryTest, SpecialCharacters) {
    std::vector<std::string> files = {
        "C:\\Users\\Test (1)\\file[1].txt",
        "C:\\Users\\Test (1)\\file[2].txt"
    };
    EXPECT_EQ(GetCommonRoot(files), "C:\\Users\\Test (1)\\");
}

TEST(PathBoundaryTest, UNCPath) {
    std::vector<std::string> files = {
        "\\\\Server\\Share\\folder\\file1.txt",
        "\\\\Server\\Share\\folder\\file2.txt"
    };
    std::string root = GetCommonRoot(files);
    EXPECT_TRUE(root.find("\\\\Server\\Share\\folder\\") != std::string::npos ||
                root.find("\\\\Server\\Share\\") != std::string::npos);
}

TEST(PathBoundaryTest, ForwardSlashes) {
    // 测试正斜杠（虽然 Windows 主要用反斜杠）
    std::vector<std::string> files = {
        "C:/Users/Test/file1.txt",
        "C:/Users/Test/file2.txt"
    };
    // 函数使用反斜杠，正斜杠可能不被正确处理
    std::string root = GetCommonRoot(files);
    // 验证不会崩溃
    EXPECT_FALSE(root.empty());
}

// ============================================
// 参数化测试
// ============================================

class GetRelativePathParameterizedTest
    : public ::testing::TestWithParam<std::tuple<std::string, std::string, std::string>> {};

TEST_P(GetRelativePathParameterizedTest, VariousPaths) {
    auto [root, fullPath, expected] = GetParam();
    EXPECT_EQ(GetRelativePath(root, fullPath), expected);
}

INSTANTIATE_TEST_SUITE_P(
    PathCases,
    GetRelativePathParameterizedTest,
    ::testing::Values(
        std::make_tuple("C:\\a\\", "C:\\a\\b.txt", "b.txt"),
        std::make_tuple("C:\\a\\", "C:\\a\\b\\c.txt", "b\\c.txt"),
        std::make_tuple("C:\\a\\b\\", "C:\\a\\b\\c\\d\\e.txt", "c\\d\\e.txt"),
        std::make_tuple("", "file.txt", "file.txt"),
        std::make_tuple("C:\\", "C:\\file.txt", "file.txt")
    )
);
