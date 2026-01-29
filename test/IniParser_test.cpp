// IniParser_test.cpp - CIniParser 单元测试
// 编译: cl /EHsc /W4 IniParser_test.cpp /Fe:IniParser_test.exe
// 运行: IniParser_test.exe

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "../common/IniParser.h"

static int g_total = 0;
static int g_passed = 0;
static int g_failed = 0;

#define TEST_ASSERT(expr, msg) do { \
    g_total++; \
    if (expr) { g_passed++; } \
    else { g_failed++; printf("  FAIL: %s\n    %s:%d\n", msg, __FILE__, __LINE__); } \
} while(0)

#define TEST_STR_EQ(actual, expected, msg) do { \
    g_total++; \
    if (std::string(actual) == std::string(expected)) { g_passed++; } \
    else { g_failed++; printf("  FAIL: %s\n    expected: \"%s\"\n    actual:   \"%s\"\n    %s:%d\n", \
        msg, expected, actual, __FILE__, __LINE__); } \
} while(0)

// 辅助：写入临时文件
static std::string WriteTempFile(const char* name, const char* content)
{
    std::string path = std::string("_test_") + name + ".ini";
    FILE* f = nullptr;
#ifdef _MSC_VER
    fopen_s(&f, path.c_str(), "w");
#else
    f = fopen(path.c_str(), "w");
#endif
    if (f) {
        fputs(content, f);
        fclose(f);
    }
    return path;
}

static void CleanupFile(const std::string& path)
{
    remove(path.c_str());
}

// ============================================
// Test 1: 基本 key=value 解析
// ============================================
void Test_BasicKeyValue()
{
    printf("[Test 1] Basic key=value parsing\n");
    std::string path = WriteTempFile("basic",
        "[Strings]\n"
        "hello=world\n"
        "foo=bar\n"
    );

    CIniParser ini;
    TEST_ASSERT(ini.LoadFile(path.c_str()), "LoadFile should succeed");
    TEST_STR_EQ(ini.GetValue("Strings", "hello"), "world", "hello -> world");
    TEST_STR_EQ(ini.GetValue("Strings", "foo"), "bar", "foo -> bar");
    TEST_ASSERT(ini.GetSectionSize("Strings") == 2, "Section size should be 2");

    CleanupFile(path);
}

// ============================================
// Test 2: key 尾部空格保留（核心特性）
// ============================================
void Test_KeyTrailingSpace()
{
    printf("[Test 2] Key trailing space preserved\n");
    // 模拟: "请输入主机备注: =Enter host note:"
    // key 是 "请输入主机备注: "（冒号+空格），不能被 trim
    std::string path = WriteTempFile("trailing_space",
        "[Strings]\n"
        "key_no_space=value1\n"
        "key_with_space =value2\n"
        "key_with_2spaces  =value3\n"
    );

    CIniParser ini;
    ini.LoadFile(path.c_str());

    TEST_STR_EQ(ini.GetValue("Strings", "key_no_space"), "value1",
                "key without trailing space");
    TEST_STR_EQ(ini.GetValue("Strings", "key_with_space "), "value2",
                "key with 1 trailing space (must preserve)");
    TEST_STR_EQ(ini.GetValue("Strings", "key_with_2spaces  "), "value3",
                "key with 2 trailing spaces (must preserve)");

    // 不带空格的查找应该找不到
    TEST_STR_EQ(ini.GetValue("Strings", "key_with_space", "NOT_FOUND"), "NOT_FOUND",
                "key without trailing space should NOT match");

    CleanupFile(path);
}

// ============================================
// Test 3: value 中含特殊字符
// ============================================
void Test_SpecialCharsInValue()
{
    printf("[Test 3] Special characters in value\n");
    std::string path = WriteTempFile("special_chars",
        "[Strings]\n"
        "menu=Menu(&F)\n"
        "addr=<IP:PORT>\n"
        "fmt=%s connected %d times\n"
        "paren=(auto-restore on expiry)\n"
    );

    CIniParser ini;
    ini.LoadFile(path.c_str());

    TEST_STR_EQ(ini.GetValue("Strings", "menu"), "Menu(&F)", "value with (&F)");
    TEST_STR_EQ(ini.GetValue("Strings", "addr"), "<IP:PORT>", "value with <IP:PORT>");
    TEST_STR_EQ(ini.GetValue("Strings", "fmt"), "%s connected %d times", "value with %s %d");
    TEST_STR_EQ(ini.GetValue("Strings", "paren"), "(auto-restore on expiry)", "value with parens");

    CleanupFile(path);
}

// ============================================
// Test 4: 注释行跳过
// ============================================
void Test_Comments()
{
    printf("[Test 4] Comment lines skipped\n");
    std::string path = WriteTempFile("comments",
        "; This is a comment\n"
        "# This is also a comment\n"
        "[Strings]\n"
        "; ============================================\n"
        "# Section header comment\n"
        "key1=value1\n"
        "; key2=should_not_exist\n"
        "key3=value3\n"
    );

    CIniParser ini;
    ini.LoadFile(path.c_str());

    TEST_STR_EQ(ini.GetValue("Strings", "key1"), "value1", "key1 exists");
    TEST_STR_EQ(ini.GetValue("Strings", "key3"), "value3", "key3 exists");
    TEST_STR_EQ(ini.GetValue("Strings", "key2", "NOT_FOUND"), "NOT_FOUND",
                "commented key2 should not exist");
    TEST_ASSERT(ini.GetSectionSize("Strings") == 2, "Only 2 keys (comments excluded)");

    CleanupFile(path);
}

// ============================================
// Test 5: 空行跳过
// ============================================
void Test_EmptyLines()
{
    printf("[Test 5] Empty lines skipped\n");
    std::string path = WriteTempFile("empty_lines",
        "\n"
        "\n"
        "[Strings]\n"
        "\n"
        "key1=value1\n"
        "\n"
        "\n"
        "key2=value2\n"
        "\n"
    );

    CIniParser ini;
    ini.LoadFile(path.c_str());

    TEST_ASSERT(ini.GetSectionSize("Strings") == 2, "2 keys despite empty lines");
    TEST_STR_EQ(ini.GetValue("Strings", "key1"), "value1", "key1");
    TEST_STR_EQ(ini.GetValue("Strings", "key2"), "value2", "key2");

    CleanupFile(path);
}

// ============================================
// Test 6: section 切换
// ============================================
void Test_MultipleSections()
{
    printf("[Test 6] Multiple sections\n");
    std::string path = WriteTempFile("sections",
        "[Strings]\n"
        "key1=value1\n"
        "key2=value2\n"
        "[Other]\n"
        "key1=other_value1\n"
        "key3=other_value3\n"
        "[Strings2]\n"
        "keyA=valueA\n"
    );

    CIniParser ini;
    ini.LoadFile(path.c_str());

    TEST_STR_EQ(ini.GetValue("Strings", "key1"), "value1", "Strings.key1");
    TEST_STR_EQ(ini.GetValue("Strings", "key2"), "value2", "Strings.key2");
    TEST_STR_EQ(ini.GetValue("Other", "key1"), "other_value1", "Other.key1");
    TEST_STR_EQ(ini.GetValue("Other", "key3"), "other_value3", "Other.key3");
    TEST_STR_EQ(ini.GetValue("Strings2", "keyA"), "valueA", "Strings2.keyA");

    // Strings section should not contain Other section's keys
    TEST_STR_EQ(ini.GetValue("Strings", "key3", "NOT_FOUND"), "NOT_FOUND",
                "Strings should not have Other's key3");

    TEST_ASSERT(ini.GetSectionSize("Strings") == 2, "Strings has 2 keys");
    TEST_ASSERT(ini.GetSectionSize("Other") == 2, "Other has 2 keys");
    TEST_ASSERT(ini.GetSectionSize("Strings2") == 1, "Strings2 has 1 key");

    CleanupFile(path);
}

// ============================================
// Test 7: 大文件（超过 32KB）
// ============================================
void Test_LargeFile()
{
    printf("[Test 7] Large file (>32KB)\n");
    std::string path = std::string("_test_large.ini");
    FILE* f = nullptr;
#ifdef _MSC_VER
    fopen_s(&f, path.c_str(), "w");
#else
    f = fopen(path.c_str(), "w");
#endif
    if (!f) {
        printf("  SKIP: Cannot create temp file\n");
        return;
    }

    fputs("[Strings]\n", f);

    // 写入大量条目使文件超过 32KB
    const int entryCount = 2000;
    for (int i = 0; i < entryCount; i++) {
        fprintf(f, "key_%04d=value_for_entry_number_%04d_padding_text_here\n", i, i);
    }

    // 在文件末尾写一个特殊条目
    fputs("last_key=last_value\n", f);
    fclose(f);

    CIniParser ini;
    TEST_ASSERT(ini.LoadFile(path.c_str()), "LoadFile should succeed for large file");

    // 验证首尾和中间的条目
    TEST_STR_EQ(ini.GetValue("Strings", "key_0000"),
                "value_for_entry_number_0000_padding_text_here",
                "First entry");
    TEST_STR_EQ(ini.GetValue("Strings", "key_0999"),
                "value_for_entry_number_0999_padding_text_here",
                "Middle entry");
    TEST_STR_EQ(ini.GetValue("Strings", "key_1999"),
                "value_for_entry_number_1999_padding_text_here",
                "Last numbered entry");
    TEST_STR_EQ(ini.GetValue("Strings", "last_key"), "last_value",
                "Entry at very end of large file");

    size_t size = ini.GetSectionSize("Strings");
    TEST_ASSERT(size == entryCount + 1,
                "Section size should be entryCount + 1 (last_key)");

    printf("  File has %d entries, all readable\n", (int)size);

    CleanupFile(path);
}

// ============================================
// Test 8: 文件不存在
// ============================================
void Test_FileNotExist()
{
    printf("[Test 8] File not exist\n");

    CIniParser ini;
    TEST_ASSERT(!ini.LoadFile("_nonexistent_file_12345.ini"), "LoadFile should return false");
    TEST_ASSERT(!ini.LoadFile(nullptr), "LoadFile(nullptr) should return false");
    TEST_ASSERT(!ini.LoadFile(""), "LoadFile('') should return false");
    TEST_ASSERT(ini.GetSection("Strings") == nullptr, "No sections after failed load");
}

// ============================================
// Test 9: 空文件
// ============================================
void Test_EmptyFile()
{
    printf("[Test 9] Empty file\n");
    std::string path = WriteTempFile("empty", "");

    CIniParser ini;
    TEST_ASSERT(ini.LoadFile(path.c_str()), "LoadFile should succeed for empty file");
    TEST_ASSERT(ini.GetSection("Strings") == nullptr, "No Strings section in empty file");
    TEST_ASSERT(ini.GetSectionSize("Strings") == 0, "Section size is 0");

    CleanupFile(path);
}

// ============================================
// Test 10: value 中含 '='（只按第一个 '=' 分割）
// ============================================
void Test_EqualsInValue()
{
    printf("[Test 10] Equals sign in value\n");
    std::string path = WriteTempFile("equals",
        "[Strings]\n"
        "formula=a=b+c\n"
        "equation=x=1=2=3\n"
        "normal=hello\n"
    );

    CIniParser ini;
    ini.LoadFile(path.c_str());

    TEST_STR_EQ(ini.GetValue("Strings", "formula"), "a=b+c",
                "value with one '=' should keep it");
    TEST_STR_EQ(ini.GetValue("Strings", "equation"), "x=1=2=3",
                "value with multiple '=' should keep all");
    TEST_STR_EQ(ini.GetValue("Strings", "normal"), "hello",
                "normal value unaffected");

    CleanupFile(path);
}

// ============================================
// Test 11: key 中含 \r\n 转义序列
// ============================================
void Test_EscapeCRLF_InKey()
{
    printf("[Test 11] Escape \\r\\n in key\n");
    // INI 文件中写字面量 \r\n，解析器应转为真正的 0x0D 0x0A
    // 模拟代码中: _TR("\n编译日期: ") 和 _TR("操作失败\r\n请重试")
    std::string path = WriteTempFile("escape_key",
        "[Strings]\n"
        "\\n compile date: =\\n Build Date: \n"
        "fail\\r\\nretry=Fail\\r\\nRetry\n"
        "line1\\nline2\\nline3=L1\\nL2\\nL3\n"
    );

    CIniParser ini;
    ini.LoadFile(path.c_str());

    // key "\n compile date: " (真正的换行 + 文本)
    TEST_STR_EQ(ini.GetValue("Strings", "\n compile date: "), "\n Build Date: ",
                "key with \\n at start");

    // key "fail\r\nretry" (真正的 CR+LF)
    TEST_STR_EQ(ini.GetValue("Strings", "fail\r\nretry"), "Fail\r\nRetry",
                "key with \\r\\n in middle");

    // key 含多个 \n
    TEST_STR_EQ(ini.GetValue("Strings", "line1\nline2\nline3"), "L1\nL2\nL3",
                "key with multiple \\n");

    CleanupFile(path);
}

// ============================================
// Test 12: value 中含 \r\n 转义序列
// ============================================
void Test_EscapeCRLF_InValue()
{
    printf("[Test 12] Escape \\r\\n in value\n");
    std::string path = WriteTempFile("escape_value",
        "[Strings]\n"
        "msg=hello\\r\\nworld\n"
        "multiline=line1\\nline2\\nline3\n"
    );

    CIniParser ini;
    ini.LoadFile(path.c_str());

    TEST_STR_EQ(ini.GetValue("Strings", "msg"), "hello\r\nworld",
                "value with \\r\\n");
    TEST_STR_EQ(ini.GetValue("Strings", "multiline"), "line1\nline2\nline3",
                "value with multiple \\n");

    CleanupFile(path);
}

// ============================================
// Test 13: \\ 和 \" 转义
// ============================================
void Test_EscapeBackslashAndQuote()
{
    printf("[Test 13] Escape \\\\ and \\\" sequences\n");
    std::string path = WriteTempFile("escape_bsq",
        "[Strings]\n"
        "path=C:\\\\Users\\\\test\n"
        "quoted=say \\\"hello\\\"\n"
        "mixed=\\\"line1\\n line2\\\"\n"
    );

    CIniParser ini;
    ini.LoadFile(path.c_str());

    TEST_STR_EQ(ini.GetValue("Strings", "path"), "C:\\Users\\test",
                "double backslash -> single backslash");
    TEST_STR_EQ(ini.GetValue("Strings", "quoted"), "say \"hello\"",
                "escaped quotes");
    TEST_STR_EQ(ini.GetValue("Strings", "mixed"), "\"line1\n line2\"",
                "mixed \\\" and \\n");

    CleanupFile(path);
}

// ============================================
// Test 14: \t 转义
// ============================================
void Test_EscapeTab()
{
    printf("[Test 14] Escape \\t sequence\n");
    std::string path = WriteTempFile("escape_tab",
        "[Strings]\n"
        "col=name\\tvalue\n"
        "header=ID\\tName\\tStatus\n"
    );

    CIniParser ini;
    ini.LoadFile(path.c_str());

    TEST_STR_EQ(ini.GetValue("Strings", "col"), "name\tvalue",
                "\\t -> tab");
    TEST_STR_EQ(ini.GetValue("Strings", "header"), "ID\tName\tStatus",
                "multiple \\t");

    CleanupFile(path);
}

// ============================================
// Test 15: 未知转义保留原样
// ============================================
void Test_UnknownEscapePassthrough()
{
    printf("[Test 15] Unknown escape passthrough\n");
    std::string path = WriteTempFile("escape_unknown",
        "[Strings]\n"
        "unknown=hello\\xworld\n"
        "trailing_bs=end\\\n"
    );

    CIniParser ini;
    ini.LoadFile(path.c_str());

    // \x 不是已知转义，应保留反斜杠
    TEST_STR_EQ(ini.GetValue("Strings", "unknown"), "hello\\xworld",
                "unknown \\x keeps backslash");
    // 行尾的孤立反斜杠（fgets 去掉换行后，最后一个字符是 \）
    TEST_STR_EQ(ini.GetValue("Strings", "trailing_bs"), "end\\",
                "trailing backslash preserved");

    CleanupFile(path);
}

// ============================================
// Test 16: key 中转义与尾部空格组合
// ============================================
void Test_EscapeWithTrailingSpace()
{
    printf("[Test 16] Escape + trailing space in key\n");
    // 模拟: _TR("\n编译日期: ") — key 以 \n 开头，以冒号+空格结尾
    std::string path = WriteTempFile("escape_trail",
        "[Strings]\n"
        "\\n date: =\\n Date: \n"
    );

    CIniParser ini;
    ini.LoadFile(path.c_str());

    // key 是 "\n date: "（真正换行 + 文本 + 尾部空格）
    TEST_STR_EQ(ini.GetValue("Strings", "\n date: "), "\n Date: ",
                "escape \\n + trailing space in key");

    // 不带尾部空格应找不到
    TEST_STR_EQ(ini.GetValue("Strings", "\n date:", "NOT_FOUND"), "NOT_FOUND",
                "without trailing space should not match");

    CleanupFile(path);
}

// ============================================
// Test 17: key 以 '[' 开头（不是 section 头）
// ============================================
void Test_BracketKey()
{
    printf("[Test 17] Key starting with '[' (not a section header)\n");
    // 模拟: _TR("[使用FRP]") 和 _TR("[未使用FRP]")
    std::string path = WriteTempFile("bracket_key",
        "[Strings]\n"
        "normal=value1\n"
        "[tag1]=[Tag One]\n"
        "[tag2]=[Tag Two]\n"
        "after=value2\n"
    );

    CIniParser ini;
    ini.LoadFile(path.c_str());

    // [tag1]=[Tag One] 应该是 key=value，不是 section 头
    TEST_STR_EQ(ini.GetValue("Strings", "[tag1]"), "[Tag One]",
                "[tag1] parsed as key, not section");
    TEST_STR_EQ(ini.GetValue("Strings", "[tag2]"), "[Tag Two]",
                "[tag2] parsed as key, not section");

    // 前后的普通 key 应仍在 Strings section
    TEST_STR_EQ(ini.GetValue("Strings", "normal"), "value1",
                "normal key before bracket keys");
    TEST_STR_EQ(ini.GetValue("Strings", "after"), "value2",
                "normal key after bracket keys still in Strings");

    TEST_ASSERT(ini.GetSectionSize("Strings") == 4, "Strings has 4 keys");

    // 不应该有 tag1 或 tag2 section
    TEST_ASSERT(ini.GetSection("tag1") == nullptr, "no tag1 section");
    TEST_ASSERT(ini.GetSection("tag2") == nullptr, "no tag2 section");

    CleanupFile(path);
}

// ============================================
// main
// ============================================
int main()
{
    printf("=== CIniParser Tests ===\n\n");

    Test_BasicKeyValue();
    Test_KeyTrailingSpace();
    Test_SpecialCharsInValue();
    Test_Comments();
    Test_EmptyLines();
    Test_MultipleSections();
    Test_LargeFile();
    Test_FileNotExist();
    Test_EmptyFile();
    Test_EqualsInValue();
    Test_EscapeCRLF_InKey();
    Test_EscapeCRLF_InValue();
    Test_EscapeBackslashAndQuote();
    Test_EscapeTab();
    Test_UnknownEscapePassthrough();
    Test_EscapeWithTrailingSpace();
    Test_BracketKey();

    printf("\n=== Results: %d/%d passed", g_passed, g_total);
    if (g_failed > 0)
        printf(", %d FAILED", g_failed);
    printf(" ===\n");

    return g_failed > 0 ? 1 : 0;
}
