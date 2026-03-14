/**
 * @file DateVerifyTest.cpp
 * @brief DateVerify 时间验证类测试
 *
 * 测试覆盖：
 * - isTimeTampered() 时间篡改检测
 * - getTimeOffset() 时间偏差获取
 * - isTrial() 试用期检测
 * - isTrail() 兼容性测试
 * - 边界条件和缓存机制
 */

#include <gtest/gtest.h>
#include <cstring>
#include <ctime>
#include <map>
#include <chrono>
#include <cmath>
#include <string>
#include <functional>

// ============================================
// 可测试版本的 DateVerify 类
// 允许注入模拟的网络时间
// ============================================

class TestableDateVerify
{
private:
    bool m_hasVerified = false;
    bool m_lastTimeTampered = true;
    time_t m_lastVerifyLocalTime = 0;
    time_t m_lastNetworkTime = 0;
    static const int VERIFY_INTERVAL = 6 * 3600;  // 6小时

    // 模拟的网络时间（0表示网络不可用）
    time_t m_mockNetworkTime = 0;
    bool m_useMockTime = false;

    // 模拟的本地时间
    time_t m_mockLocalTime = 0;
    bool m_useMockLocalTime = false;

    // 模拟的编译日期
    std::string m_mockCompileDate;

    time_t getNetworkTimeInChina()
    {
        if (m_useMockTime) {
            return m_mockNetworkTime;
        }
        // 实际测试中不调用网络
        return 0;
    }

    time_t getLocalTime()
    {
        if (m_useMockLocalTime) {
            return m_mockLocalTime;
        }
        return time(nullptr);
    }

    int monthAbbrevToNumber(const std::string& month)
    {
        static const std::map<std::string, int> months = {
            {"Jan", 1}, {"Feb", 2}, {"Mar", 3}, {"Apr", 4},
            {"May", 5}, {"Jun", 6}, {"Jul", 7}, {"Aug", 8},
            {"Sep", 9}, {"Oct", 10}, {"Nov", 11}, {"Dec", 12}
        };
        auto it = months.find(month);
        return (it != months.end()) ? it->second : 0;
    }

    tm parseCompileDate(const char* compileDate)
    {
        tm tmCompile = { 0 };
        std::string monthStr(compileDate, 3);
        std::string dayStr(compileDate + 4, 2);
        std::string yearStr(compileDate + 7, 4);

        tmCompile.tm_year = std::stoi(yearStr) - 1900;
        tmCompile.tm_mon = monthAbbrevToNumber(monthStr) - 1;
        tmCompile.tm_mday = std::stoi(dayStr);

        return tmCompile;
    }

    int daysBetweenDates(const tm& date1, const tm& date2)
    {
        auto timeToTimePoint = [](const tm& tmTime) {
            std::time_t tt = mktime(const_cast<tm*>(&tmTime));
            return std::chrono::system_clock::from_time_t(tt);
        };

        auto tp1 = timeToTimePoint(date1);
        auto tp2 = timeToTimePoint(date2);

        auto duration = tp1 > tp2 ? tp1 - tp2 : tp2 - tp1;
        return static_cast<int>(std::chrono::duration_cast<std::chrono::hours>(duration).count() / 24);
    }

    tm getCurrentDate()
    {
        std::time_t now = getLocalTime();
        tm tmNow = *std::localtime(&now);
        tmNow.tm_hour = 0;
        tmNow.tm_min = 0;
        tmNow.tm_sec = 0;
        return tmNow;
    }

public:
    // 测试辅助方法
    void setMockNetworkTime(time_t t) { m_mockNetworkTime = t; m_useMockTime = true; }
    void setNetworkUnavailable() { m_mockNetworkTime = 0; m_useMockTime = true; }
    void setMockLocalTime(time_t t) { m_mockLocalTime = t; m_useMockLocalTime = true; }
    void setMockCompileDate(const std::string& date) { m_mockCompileDate = date; }
    void resetCache() { m_hasVerified = false; m_lastTimeTampered = true; }
    bool hasVerified() const { return m_hasVerified; }

    // 检测本地时间是否被篡改
    bool isTimeTampered(int toleranceDays = 1)
    {
        time_t currentLocalTime = getLocalTime();

        // 检查是否可以使用缓存
        if (m_hasVerified) {
            time_t localElapsed = currentLocalTime - m_lastVerifyLocalTime;

            // 本地时间在合理范围内前进，使用缓存推算
            if (localElapsed >= 0 && localElapsed < VERIFY_INTERVAL) {
                time_t estimatedNetworkTime = m_lastNetworkTime + localElapsed;
                double diffDays = difftime(estimatedNetworkTime, currentLocalTime) / 86400.0;
                if (fabs(diffDays) <= toleranceDays) {
                    return false;  // 未篡改
                }
            }
        }

        // 执行网络验证
        time_t networkTime = getNetworkTimeInChina();
        if (networkTime == 0) {
            // 网络不可用：如果之前验证通过且本地时间没异常，暂时信任
            if (m_hasVerified && !m_lastTimeTampered) {
                time_t localElapsed = currentLocalTime - m_lastVerifyLocalTime;
                // 允许5分钟回拨和24小时内的前进
                if (localElapsed >= -300 && localElapsed < 24 * 3600) {
                    return false;
                }
            }
            return true;  // 无法验证，视为篡改
        }

        // 更新缓存
        m_hasVerified = true;
        m_lastVerifyLocalTime = currentLocalTime;
        m_lastNetworkTime = networkTime;

        double diffDays = difftime(networkTime, currentLocalTime) / 86400.0;
        m_lastTimeTampered = fabs(diffDays) > toleranceDays;

        return m_lastTimeTampered;
    }

    // 获取网络时间与本地时间的偏差（秒）
    int getTimeOffset()
    {
        time_t networkTime = getNetworkTimeInChina();
        if (networkTime == 0) return 0;
        return static_cast<int>(difftime(networkTime, getLocalTime()));
    }

    bool isTrial(int trialDays = 7)
    {
        if (isTimeTampered())
            return false;

        const char* compileDate = m_mockCompileDate.empty() ? __DATE__ : m_mockCompileDate.c_str();
        tm tmCompile = parseCompileDate(compileDate), tmCurrent = getCurrentDate();

        int daysDiff = daysBetweenDates(tmCompile, tmCurrent);

        return daysDiff <= trialDays;
    }

    // 兼容性函数
    bool isTrail(int trailDays = 7) { return isTrial(trailDays); }
};

// ============================================
// 辅助函数
// ============================================

// 创建指定日期的 time_t
time_t makeTime(int year, int month, int day, int hour = 12, int minute = 0, int second = 0)
{
    tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = minute;
    t.tm_sec = second;
    return mktime(&t);
}

// 格式化日期为 __DATE__ 格式 (例如 "Mar 13 2026")
std::string formatCompileDate(int year, int month, int day)
{
    static const char* months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    char buf[16];
    snprintf(buf, sizeof(buf), "%s %2d %d", months[month - 1], day, year);
    return std::string(buf);
}

// ============================================
// isTimeTampered 测试
// ============================================

class IsTimeTamperedTest : public ::testing::Test {
protected:
    TestableDateVerify verifier;
};

TEST_F(IsTimeTamperedTest, NetworkUnavailable_NoCache_ReturnsTampered)
{
    // 网络不可用，无缓存时应返回 true（视为篡改）
    verifier.setNetworkUnavailable();

    EXPECT_TRUE(verifier.isTimeTampered());
}

TEST_F(IsTimeTamperedTest, NetworkAvailable_TimeMatch_ReturnsNotTampered)
{
    // 网络时间与本地时间匹配
    time_t now = time(nullptr);
    verifier.setMockNetworkTime(now);
    verifier.setMockLocalTime(now);

    EXPECT_FALSE(verifier.isTimeTampered(1));
}

TEST_F(IsTimeTamperedTest, NetworkAvailable_TimeWithinTolerance_ReturnsNotTampered)
{
    // 网络时间与本地时间差异在容忍范围内（12小时差异，1天容忍）
    time_t now = time(nullptr);
    verifier.setMockNetworkTime(now);
    verifier.setMockLocalTime(now - 12 * 3600);  // 本地时间落后12小时

    EXPECT_FALSE(verifier.isTimeTampered(1));
}

TEST_F(IsTimeTamperedTest, NetworkAvailable_TimeExceedsTolerance_ReturnsTampered)
{
    // 网络时间与本地时间差异超过容忍范围（2天差异，1天容忍）
    time_t now = time(nullptr);
    verifier.setMockNetworkTime(now);
    verifier.setMockLocalTime(now - 2 * 86400);  // 本地时间落后2天

    EXPECT_TRUE(verifier.isTimeTampered(1));
}

TEST_F(IsTimeTamperedTest, ToleranceDays_Zero_StrictMode)
{
    // 0天容忍度，任何偏差都应报告为篡改
    time_t now = time(nullptr);
    verifier.setMockNetworkTime(now);
    verifier.setMockLocalTime(now - 86400);  // 1天偏差

    EXPECT_TRUE(verifier.isTimeTampered(0));
}

TEST_F(IsTimeTamperedTest, ToleranceDays_Large_LenientMode)
{
    // 大容忍度（7天），较大偏差仍应通过
    time_t now = time(nullptr);
    verifier.setMockNetworkTime(now);
    verifier.setMockLocalTime(now - 5 * 86400);  // 5天偏差

    EXPECT_FALSE(verifier.isTimeTampered(7));
}

TEST_F(IsTimeTamperedTest, LocalTimeAhead_ExceedsTolerance_ReturnsTampered)
{
    // 本地时间超前网络时间
    time_t now = time(nullptr);
    verifier.setMockNetworkTime(now);
    verifier.setMockLocalTime(now + 3 * 86400);  // 本地时间超前3天

    EXPECT_TRUE(verifier.isTimeTampered(1));
}

TEST_F(IsTimeTamperedTest, CacheHit_WithinInterval_UsesCache)
{
    // 首次验证成功
    time_t now = time(nullptr);
    verifier.setMockNetworkTime(now);
    verifier.setMockLocalTime(now);

    EXPECT_FALSE(verifier.isTimeTampered(1));
    EXPECT_TRUE(verifier.hasVerified());

    // 模拟时间前进1小时（在6小时缓存间隔内）
    verifier.setMockLocalTime(now + 3600);
    verifier.setNetworkUnavailable();  // 网络不可用

    // 应该使用缓存，不报告篡改
    EXPECT_FALSE(verifier.isTimeTampered(1));
}

TEST_F(IsTimeTamperedTest, CacheMiss_ExceedsInterval_RequiresNetwork)
{
    // 首次验证成功
    time_t now = time(nullptr);
    verifier.setMockNetworkTime(now);
    verifier.setMockLocalTime(now);

    EXPECT_FALSE(verifier.isTimeTampered(1));

    // 模拟时间前进25小时（超过24小时宽容期）
    // 注：实现中网络不可用时有24小时宽容期，超过后才报告篡改
    verifier.setMockLocalTime(now + 25 * 3600);
    verifier.setNetworkUnavailable();

    // 超过24小时宽容期，网络不可用，应报告篡改
    EXPECT_TRUE(verifier.isTimeTampered(1));
}

TEST_F(IsTimeTamperedTest, NetworkUnavailable_WithGoodCache_AllowsSmallRewind)
{
    // 首次验证成功
    time_t now = time(nullptr);
    verifier.setMockNetworkTime(now);
    verifier.setMockLocalTime(now);

    EXPECT_FALSE(verifier.isTimeTampered(1));

    // 模拟时间回拨2分钟（在允许的5分钟范围内）
    verifier.setMockLocalTime(now - 120);
    verifier.setNetworkUnavailable();

    // 应该暂时信任
    EXPECT_FALSE(verifier.isTimeTampered(1));
}

TEST_F(IsTimeTamperedTest, NetworkUnavailable_WithGoodCache_RejectsLargeRewind)
{
    // 首次验证成功
    time_t now = time(nullptr);
    verifier.setMockNetworkTime(now);
    verifier.setMockLocalTime(now);

    EXPECT_FALSE(verifier.isTimeTampered(1));

    // 模拟时间回拨10分钟（超过允许的5分钟）
    verifier.setMockLocalTime(now - 600);
    verifier.setNetworkUnavailable();

    // 应该报告篡改
    EXPECT_TRUE(verifier.isTimeTampered(1));
}

// ============================================
// getTimeOffset 测试
// ============================================

class GetTimeOffsetTest : public ::testing::Test {
protected:
    TestableDateVerify verifier;
};

TEST_F(GetTimeOffsetTest, NetworkUnavailable_ReturnsZero)
{
    verifier.setNetworkUnavailable();
    EXPECT_EQ(verifier.getTimeOffset(), 0);
}

TEST_F(GetTimeOffsetTest, LocalTimeBehind_ReturnsPositive)
{
    time_t now = time(nullptr);
    verifier.setMockNetworkTime(now);
    verifier.setMockLocalTime(now - 3600);  // 本地落后1小时

    int offset = verifier.getTimeOffset();
    EXPECT_GT(offset, 0);
    EXPECT_NEAR(offset, 3600, 5);  // 允许5秒误差
}

TEST_F(GetTimeOffsetTest, LocalTimeAhead_ReturnsNegative)
{
    time_t now = time(nullptr);
    verifier.setMockNetworkTime(now);
    verifier.setMockLocalTime(now + 3600);  // 本地超前1小时

    int offset = verifier.getTimeOffset();
    EXPECT_LT(offset, 0);
    EXPECT_NEAR(offset, -3600, 5);
}

TEST_F(GetTimeOffsetTest, TimeMatch_ReturnsNearZero)
{
    time_t now = time(nullptr);
    verifier.setMockNetworkTime(now);
    verifier.setMockLocalTime(now);

    int offset = verifier.getTimeOffset();
    EXPECT_NEAR(offset, 0, 2);  // 允许2秒误差
}

// ============================================
// isTrial / isTrail 测试
// ============================================

class IsTrialTest : public ::testing::Test {
protected:
    TestableDateVerify verifier;

    void SetUp() override {
        // 默认设置：网络正常，时间同步
        time_t now = time(nullptr);
        verifier.setMockNetworkTime(now);
        verifier.setMockLocalTime(now);
    }
};

TEST_F(IsTrialTest, WithinTrialPeriod_ReturnsTrue)
{
    // 编译日期：3天前
    time_t now = time(nullptr);
    tm* tmNow = localtime(&now);

    // 创建3天前的日期
    time_t threeDaysAgo = now - 3 * 86400;
    tm* tmCompile = localtime(&threeDaysAgo);

    std::string compileDate = formatCompileDate(
        tmCompile->tm_year + 1900,
        tmCompile->tm_mon + 1,
        tmCompile->tm_mday
    );
    verifier.setMockCompileDate(compileDate);

    EXPECT_TRUE(verifier.isTrial(7));  // 7天试用期内
}

TEST_F(IsTrialTest, ExactTrialPeriod_ReturnsTrue)
{
    // 编译日期：正好7天前
    time_t now = time(nullptr);
    time_t sevenDaysAgo = now - 7 * 86400;
    tm* tmCompile = localtime(&sevenDaysAgo);

    std::string compileDate = formatCompileDate(
        tmCompile->tm_year + 1900,
        tmCompile->tm_mon + 1,
        tmCompile->tm_mday
    );
    verifier.setMockCompileDate(compileDate);

    EXPECT_TRUE(verifier.isTrial(7));  // 边界：正好7天
}

TEST_F(IsTrialTest, ExceedsTrialPeriod_ReturnsFalse)
{
    // 编译日期：8天前
    time_t now = time(nullptr);
    time_t eightDaysAgo = now - 8 * 86400;
    tm* tmCompile = localtime(&eightDaysAgo);

    std::string compileDate = formatCompileDate(
        tmCompile->tm_year + 1900,
        tmCompile->tm_mon + 1,
        tmCompile->tm_mday
    );
    verifier.setMockCompileDate(compileDate);

    EXPECT_FALSE(verifier.isTrial(7));  // 超过7天试用期
}

TEST_F(IsTrialTest, CompileToday_ReturnsTrue)
{
    // 编译日期：今天
    time_t now = time(nullptr);
    tm* tmNow = localtime(&now);

    std::string compileDate = formatCompileDate(
        tmNow->tm_year + 1900,
        tmNow->tm_mon + 1,
        tmNow->tm_mday
    );
    verifier.setMockCompileDate(compileDate);

    EXPECT_TRUE(verifier.isTrial(7));
    EXPECT_TRUE(verifier.isTrial(1));
    EXPECT_TRUE(verifier.isTrial(0));
}

TEST_F(IsTrialTest, TimeTampered_ReturnsFalse)
{
    // 时间被篡改时，无论试用期如何都应返回 false
    time_t now = time(nullptr);
    verifier.setMockNetworkTime(now);
    verifier.setMockLocalTime(now - 30 * 86400);  // 本地时间落后30天

    // 即使编译日期是今天，时间篡改也会导致返回 false
    tm* tmNow = localtime(&now);
    std::string compileDate = formatCompileDate(
        tmNow->tm_year + 1900,
        tmNow->tm_mon + 1,
        tmNow->tm_mday
    );
    verifier.setMockCompileDate(compileDate);

    EXPECT_FALSE(verifier.isTrial(7));
}

TEST_F(IsTrialTest, CustomTrialDays_Zero)
{
    // 0天试用期：只有编译当天有效
    time_t now = time(nullptr);
    time_t yesterday = now - 86400;
    tm* tmYesterday = localtime(&yesterday);

    std::string compileDate = formatCompileDate(
        tmYesterday->tm_year + 1900,
        tmYesterday->tm_mon + 1,
        tmYesterday->tm_mday
    );
    verifier.setMockCompileDate(compileDate);

    EXPECT_FALSE(verifier.isTrial(0));  // 昨天编译，0天试用期已过
}

TEST_F(IsTrialTest, IsTrail_CompatibilityAlias)
{
    // isTrail 应该与 isTrial 行为一致
    time_t now = time(nullptr);
    tm* tmNow = localtime(&now);

    std::string compileDate = formatCompileDate(
        tmNow->tm_year + 1900,
        tmNow->tm_mon + 1,
        tmNow->tm_mday
    );
    verifier.setMockCompileDate(compileDate);

    EXPECT_EQ(verifier.isTrail(7), verifier.isTrial(7));
    EXPECT_EQ(verifier.isTrail(0), verifier.isTrial(0));
}

// ============================================
// 边界条件测试
// ============================================

class BoundaryTest : public ::testing::Test {
protected:
    TestableDateVerify verifier;
};

TEST_F(BoundaryTest, ToleranceDays_ExactBoundary)
{
    // 测试容忍度边界：偏差正好等于容忍天数
    time_t now = time(nullptr);
    verifier.setMockNetworkTime(now);
    verifier.setMockLocalTime(now - 86400);  // 正好1天偏差

    // 1天容忍度，1天偏差，应该通过（<= 比较）
    EXPECT_FALSE(verifier.isTimeTampered(1));
}

TEST_F(BoundaryTest, ToleranceDays_JustOverBoundary)
{
    // 测试容忍度边界：偏差刚刚超过容忍天数
    time_t now = time(nullptr);
    verifier.setMockNetworkTime(now);
    verifier.setMockLocalTime(now - 86400 - 3600);  // 1天+1小时偏差

    // 1天容忍度，偏差超过1天，应该失败
    EXPECT_TRUE(verifier.isTimeTampered(1));
}

TEST_F(BoundaryTest, CacheInterval_ExactBoundary)
{
    // 测试网络不可用时的24小时宽容期边界
    time_t now = time(nullptr);
    verifier.setMockNetworkTime(now);
    verifier.setMockLocalTime(now);

    EXPECT_FALSE(verifier.isTimeTampered(1));

    // 正好24小时后（刚好到达宽容期边界）
    verifier.setMockLocalTime(now + 24 * 3600);
    verifier.setNetworkUnavailable();

    // 刚好到达24小时边界，网络不可用，应报告篡改
    EXPECT_TRUE(verifier.isTimeTampered(1));
}

TEST_F(BoundaryTest, CacheInterval_JustUnderBoundary)
{
    // 测试缓存间隔边界：略少于6小时
    time_t now = time(nullptr);
    verifier.setMockNetworkTime(now);
    verifier.setMockLocalTime(now);

    EXPECT_FALSE(verifier.isTimeTampered(1));

    // 5小时59分钟后（仍在缓存有效期内）
    verifier.setMockLocalTime(now + 6 * 3600 - 60);
    verifier.setNetworkUnavailable();

    // 仍在缓存有效期内
    EXPECT_FALSE(verifier.isTimeTampered(1));
}

TEST_F(BoundaryTest, NetworkTimeRollback_AllowedMargin)
{
    // 测试允许的时间回拨范围：正好5分钟
    time_t now = time(nullptr);
    verifier.setMockNetworkTime(now);
    verifier.setMockLocalTime(now);

    EXPECT_FALSE(verifier.isTimeTampered(1));

    // 回拨正好5分钟
    verifier.setMockLocalTime(now - 300);
    verifier.setNetworkUnavailable();

    // 边界情况：-300 >= -300，应该通过
    EXPECT_FALSE(verifier.isTimeTampered(1));
}

TEST_F(BoundaryTest, NetworkTimeRollback_ExceedsMargin)
{
    // 测试超过允许的时间回拨范围
    time_t now = time(nullptr);
    verifier.setMockNetworkTime(now);
    verifier.setMockLocalTime(now);

    EXPECT_FALSE(verifier.isTimeTampered(1));

    // 回拨超过5分钟
    verifier.setMockLocalTime(now - 301);
    verifier.setNetworkUnavailable();

    // 超过允许范围
    EXPECT_TRUE(verifier.isTimeTampered(1));
}

TEST_F(BoundaryTest, TrialDays_LargeValue)
{
    // 测试大试用期值
    time_t now = time(nullptr);
    verifier.setMockNetworkTime(now);
    verifier.setMockLocalTime(now);

    // 100天前编译
    time_t hundredDaysAgo = now - 100 * 86400;
    tm* tmCompile = localtime(&hundredDaysAgo);

    std::string compileDate = formatCompileDate(
        tmCompile->tm_year + 1900,
        tmCompile->tm_mon + 1,
        tmCompile->tm_mday
    );
    verifier.setMockCompileDate(compileDate);

    EXPECT_FALSE(verifier.isTrial(99));   // 99天试用期已过
    EXPECT_TRUE(verifier.isTrial(100));   // 正好100天
    EXPECT_TRUE(verifier.isTrial(365));   // 365天试用期内
}

// ============================================
// 日期解析测试
// ============================================

class DateParsingTest : public ::testing::Test {
protected:
    TestableDateVerify verifier;

    void SetUp() override {
        time_t now = time(nullptr);
        verifier.setMockNetworkTime(now);
        verifier.setMockLocalTime(now);
    }
};

TEST_F(DateParsingTest, AllMonths)
{
    // 测试所有月份的解析
    const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    time_t now = time(nullptr);
    tm* tmNow = localtime(&now);

    for (int i = 0; i < 12; ++i) {
        char dateStr[16];
        snprintf(dateStr, sizeof(dateStr), "%s %2d %d", months[i], 15, tmNow->tm_year + 1900);
        verifier.setMockCompileDate(dateStr);

        // 只要不是时间篡改，应该能正常解析
        // 结果取决于当前日期与编译日期的差异
        bool result = verifier.isTrial(365);  // 使用较长试用期确保通过
        EXPECT_TRUE(result) << "Failed for month: " << months[i];
    }
}

TEST_F(DateParsingTest, SingleDigitDay)
{
    // 测试单数字日期（如 "Mar  1 2026"）
    time_t now = time(nullptr);
    tm* tmNow = localtime(&now);

    char dateStr[16];
    snprintf(dateStr, sizeof(dateStr), "Mar  1 %d", tmNow->tm_year + 1900);
    verifier.setMockCompileDate(dateStr);

    EXPECT_TRUE(verifier.isTrial(365));
}

TEST_F(DateParsingTest, DoubleDigitDay)
{
    // 测试双数字日期（如 "Mar 15 2026"）
    time_t now = time(nullptr);
    tm* tmNow = localtime(&now);

    char dateStr[16];
    snprintf(dateStr, sizeof(dateStr), "Mar 15 %d", tmNow->tm_year + 1900);
    verifier.setMockCompileDate(dateStr);

    EXPECT_TRUE(verifier.isTrial(365));
}

// ============================================
// 授权场景模拟测试
// ============================================

class AuthorizationScenarioTest : public ::testing::Test {
protected:
    TestableDateVerify verifier;
};

TEST_F(AuthorizationScenarioTest, ValidLicense_TimeNotTampered)
{
    // 场景：有效授权，时间未被篡改
    time_t now = time(nullptr);
    verifier.setMockNetworkTime(now);
    verifier.setMockLocalTime(now);

    // 授权检查应该通过
    EXPECT_FALSE(verifier.isTimeTampered(1));
}

TEST_F(AuthorizationScenarioTest, ExpiredLicense_UserRollsBackTime)
{
    // 场景：授权过期，用户将时间回拨企图绕过
    time_t now = time(nullptr);
    time_t thirtyDaysAgo = now - 30 * 86400;

    verifier.setMockNetworkTime(now);            // 网络时间是真实时间
    verifier.setMockLocalTime(thirtyDaysAgo);    // 用户将本地时间回拨30天

    // 应该检测到时间篡改
    EXPECT_TRUE(verifier.isTimeTampered(1));
}

TEST_F(AuthorizationScenarioTest, ExpiredLicense_UserAdvancesTime)
{
    // 场景：用户将时间提前（不太常见，但也应检测）
    time_t now = time(nullptr);
    time_t thirtyDaysAhead = now + 30 * 86400;

    verifier.setMockNetworkTime(now);
    verifier.setMockLocalTime(thirtyDaysAhead);

    // 应该检测到时间篡改
    EXPECT_TRUE(verifier.isTimeTampered(1));
}

TEST_F(AuthorizationScenarioTest, OfflineUser_RecentValidation)
{
    // 场景：用户刚刚验证通过后断网
    time_t now = time(nullptr);
    verifier.setMockNetworkTime(now);
    verifier.setMockLocalTime(now);

    EXPECT_FALSE(verifier.isTimeTampered(1));  // 首次验证通过

    // 用户断网，但时间正常前进
    verifier.setMockLocalTime(now + 3600);  // 1小时后
    verifier.setNetworkUnavailable();

    // 应该允许（使用缓存）
    EXPECT_FALSE(verifier.isTimeTampered(1));
}

TEST_F(AuthorizationScenarioTest, OfflineUser_LongOffline)
{
    // 场景：用户长时间离线后恢复
    time_t now = time(nullptr);
    verifier.setMockNetworkTime(now);
    verifier.setMockLocalTime(now);

    EXPECT_FALSE(verifier.isTimeTampered(1));

    // 用户离线超过24小时
    verifier.setMockLocalTime(now + 25 * 3600);
    verifier.setNetworkUnavailable();

    // 缓存已过期，网络不可用，应该拒绝
    EXPECT_TRUE(verifier.isTimeTampered(1));
}

TEST_F(AuthorizationScenarioTest, TrialUser_WithinPeriod)
{
    // 场景：试用用户在试用期内
    time_t now = time(nullptr);
    verifier.setMockNetworkTime(now);
    verifier.setMockLocalTime(now);

    time_t threeDaysAgo = now - 3 * 86400;
    tm* tmCompile = localtime(&threeDaysAgo);
    verifier.setMockCompileDate(formatCompileDate(
        tmCompile->tm_year + 1900,
        tmCompile->tm_mon + 1,
        tmCompile->tm_mday
    ));

    EXPECT_TRUE(verifier.isTrial(7));
}

TEST_F(AuthorizationScenarioTest, TrialUser_Expired)
{
    // 场景：试用用户试用期已过
    time_t now = time(nullptr);
    verifier.setMockNetworkTime(now);
    verifier.setMockLocalTime(now);

    time_t tenDaysAgo = now - 10 * 86400;
    tm* tmCompile = localtime(&tenDaysAgo);
    verifier.setMockCompileDate(formatCompileDate(
        tmCompile->tm_year + 1900,
        tmCompile->tm_mon + 1,
        tmCompile->tm_mday
    ));

    EXPECT_FALSE(verifier.isTrial(7));
}

TEST_F(AuthorizationScenarioTest, TrialUser_RollsBackTime)
{
    // 场景：试用用户回拨时间企图延长试用期
    time_t now = time(nullptr);
    time_t tenDaysAgo = now - 10 * 86400;

    verifier.setMockNetworkTime(now);        // 真实网络时间
    verifier.setMockLocalTime(tenDaysAgo);   // 用户回拨到10天前

    // 编译日期设为15天前
    time_t fifteenDaysAgo = now - 15 * 86400;
    tm* tmCompile = localtime(&fifteenDaysAgo);
    verifier.setMockCompileDate(formatCompileDate(
        tmCompile->tm_year + 1900,
        tmCompile->tm_mon + 1,
        tmCompile->tm_mday
    ));

    // 时间篡改应被检测，isTrial 应返回 false
    EXPECT_FALSE(verifier.isTrial(7));
}
