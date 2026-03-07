// QualityAdaptiveTest.cpp - Phase 4: 质量自适应单元测试
// 测试 RTT 到质量等级的映射和防抖策略

#include <gtest/gtest.h>
#include <vector>
#include <cstdint>
#include <climits>

// ============================================
// 质量等级枚举 (来自 commands.h)
// ============================================
enum QualityLevel {
    QUALITY_DISABLED = -2,  // 关闭质量控制
    QUALITY_ADAPTIVE = -1,  // 自适应模式
    QUALITY_ULTRA    = 0,   // 极佳（局域网）
    QUALITY_HIGH     = 1,   // 优秀
    QUALITY_GOOD     = 2,   // 良好
    QUALITY_MEDIUM   = 3,   // 一般
    QUALITY_LOW      = 4,   // 较差
    QUALITY_MINIMAL  = 5,   // 最低
    QUALITY_COUNT    = 6,
};

// ============================================
// 算法枚举
// ============================================
#define ALGORITHM_GRAY    0
#define ALGORITHM_DIFF    1
#define ALGORITHM_H264    2
#define ALGORITHM_RGB565  3

// ============================================
// 质量配置结构体 (来自 commands.h)
// ============================================
struct QualityProfile {
    int maxFPS;       // 最大帧率
    int maxWidth;     // 最大宽度 (0=不限)
    int algorithm;    // 压缩算法
    int bitRate;      // kbps (仅H264使用)
};

// 默认质量配置表
static const QualityProfile g_QualityProfiles[QUALITY_COUNT] = {
    {25, 0,    ALGORITHM_DIFF,   0   },  // Ultra:   25FPS, 原始,  DIFF
    {20, 0,    ALGORITHM_RGB565, 0   },  // High:    20FPS, 原始,  RGB565
    {20, 1920, ALGORITHM_H264,   3000},  // Good:    20FPS, 1080P, H264
    {15, 1600, ALGORITHM_H264,   2000},  // Medium:  15FPS, 900P,  H264
    {12, 1280, ALGORITHM_H264,   1200},  // Low:     12FPS, 720P,  H264
    {8,  1024, ALGORITHM_H264,   800 },  // Minimal: 8FPS,  540P,  H264
};

// ============================================
// RTT 阈值表 (来自 commands.h)
// ============================================
// 行0: 直连模式, 行1: FRP代理模式
static const int g_RttThresholds[2][QUALITY_COUNT] = {
    /* DIRECT */ { 30,   80,  150,   250,  400, INT_MAX },
    /* PROXY  */ { 60,  160,  300,   500,  800, INT_MAX },
};

// ============================================
// RTT到质量等级映射函数 (来自 commands.h)
// ============================================
inline int GetTargetQualityLevel(int rtt, int usingFRP)
{
    int row = usingFRP ? 1 : 0;
    for (int level = 0; level < QUALITY_COUNT; level++) {
        if (rtt < g_RttThresholds[row][level]) {
            return level;
        }
    }
    return QUALITY_MINIMAL;
}

// ============================================
// 防抖策略模拟器
// ============================================
class QualityDebouncer {
public:
    // 防抖参数
    static const int DOWNGRADE_STABLE_COUNT = 2;   // 降级所需稳定次数
    static const int UPGRADE_STABLE_COUNT = 5;     // 升级所需稳定次数
    static const int DEFAULT_COOLDOWN_MS = 3000;   // 默认冷却时间
    static const int RES_CHANGE_DOWNGRADE_COOLDOWN_MS = 15000;  // 分辨率降级冷却
    static const int RES_CHANGE_UPGRADE_COOLDOWN_MS = 30000;    // 分辨率升级冷却
    static const int STARTUP_DELAY_MS = 60000;     // 启动延迟

    QualityDebouncer()
        : m_currentLevel(QUALITY_HIGH)
        , m_stableCount(0)
        , m_lastChangeTime(0)
        , m_startTime(0)
        , m_enabled(true)
    {}

    void Reset() {
        m_currentLevel = QUALITY_HIGH;
        m_stableCount = 0;
        m_lastChangeTime = 0;
        m_startTime = 0;
    }

    void SetStartTime(uint64_t time) {
        m_startTime = time;
    }

    void SetEnabled(bool enabled) {
        m_enabled = enabled;
    }

    // 评估并返回新的质量等级
    // 返回 -1 表示不改变
    int Evaluate(int targetLevel, uint64_t currentTime, bool resolutionChange = false) {
        if (!m_enabled) return -1;

        // 启动延迟
        if (currentTime - m_startTime < STARTUP_DELAY_MS) {
            return -1;
        }

        // 冷却时间检查
        uint64_t cooldown = DEFAULT_COOLDOWN_MS;
        if (resolutionChange) {
            cooldown = (targetLevel > m_currentLevel)
                       ? RES_CHANGE_DOWNGRADE_COOLDOWN_MS
                       : RES_CHANGE_UPGRADE_COOLDOWN_MS;
        }

        if (currentTime - m_lastChangeTime < cooldown) {
            return -1;
        }

        // 降级: 快速响应
        if (targetLevel > m_currentLevel) {
            m_stableCount++;
            if (m_stableCount >= DOWNGRADE_STABLE_COUNT) {
                int newLevel = targetLevel;
                m_currentLevel = newLevel;
                m_stableCount = 0;
                m_lastChangeTime = currentTime;
                return newLevel;
            }
        }
        // 升级: 谨慎处理，每次只升一级
        else if (targetLevel < m_currentLevel) {
            m_stableCount++;
            if (m_stableCount >= UPGRADE_STABLE_COUNT) {
                int newLevel = m_currentLevel - 1;  // 只升一级
                m_currentLevel = newLevel;
                m_stableCount = 0;
                m_lastChangeTime = currentTime;
                return newLevel;
            }
        }
        // 目标等级等于当前等级
        else {
            m_stableCount = 0;
        }

        return -1;
    }

    int GetCurrentLevel() const { return m_currentLevel; }
    int GetStableCount() const { return m_stableCount; }

private:
    int m_currentLevel;
    int m_stableCount;
    uint64_t m_lastChangeTime;
    uint64_t m_startTime;
    bool m_enabled;
};

// ============================================
// 测试夹具
// ============================================
class QualityAdaptiveTest : public ::testing::Test {
protected:
    void SetUp() override {
        debouncer.Reset();
    }

    QualityDebouncer debouncer;
};

// ============================================
// RTT 映射测试 - 直连模式
// ============================================

TEST_F(QualityAdaptiveTest, RTT_Direct_Ultra) {
    // RTT < 30ms -> Ultra
    EXPECT_EQ(GetTargetQualityLevel(0, 0), QUALITY_ULTRA);
    EXPECT_EQ(GetTargetQualityLevel(10, 0), QUALITY_ULTRA);
    EXPECT_EQ(GetTargetQualityLevel(29, 0), QUALITY_ULTRA);
}

TEST_F(QualityAdaptiveTest, RTT_Direct_High) {
    // 30ms <= RTT < 80ms -> High
    EXPECT_EQ(GetTargetQualityLevel(30, 0), QUALITY_HIGH);
    EXPECT_EQ(GetTargetQualityLevel(50, 0), QUALITY_HIGH);
    EXPECT_EQ(GetTargetQualityLevel(79, 0), QUALITY_HIGH);
}

TEST_F(QualityAdaptiveTest, RTT_Direct_Good) {
    // 80ms <= RTT < 150ms -> Good
    EXPECT_EQ(GetTargetQualityLevel(80, 0), QUALITY_GOOD);
    EXPECT_EQ(GetTargetQualityLevel(100, 0), QUALITY_GOOD);
    EXPECT_EQ(GetTargetQualityLevel(149, 0), QUALITY_GOOD);
}

TEST_F(QualityAdaptiveTest, RTT_Direct_Medium) {
    // 150ms <= RTT < 250ms -> Medium
    EXPECT_EQ(GetTargetQualityLevel(150, 0), QUALITY_MEDIUM);
    EXPECT_EQ(GetTargetQualityLevel(200, 0), QUALITY_MEDIUM);
    EXPECT_EQ(GetTargetQualityLevel(249, 0), QUALITY_MEDIUM);
}

TEST_F(QualityAdaptiveTest, RTT_Direct_Low) {
    // 250ms <= RTT < 400ms -> Low
    EXPECT_EQ(GetTargetQualityLevel(250, 0), QUALITY_LOW);
    EXPECT_EQ(GetTargetQualityLevel(300, 0), QUALITY_LOW);
    EXPECT_EQ(GetTargetQualityLevel(399, 0), QUALITY_LOW);
}

TEST_F(QualityAdaptiveTest, RTT_Direct_Minimal) {
    // RTT >= 400ms -> Minimal
    EXPECT_EQ(GetTargetQualityLevel(400, 0), QUALITY_MINIMAL);
    EXPECT_EQ(GetTargetQualityLevel(500, 0), QUALITY_MINIMAL);
    EXPECT_EQ(GetTargetQualityLevel(1000, 0), QUALITY_MINIMAL);
}

// ============================================
// RTT 映射测试 - FRP代理模式
// ============================================

TEST_F(QualityAdaptiveTest, RTT_FRP_Ultra) {
    // RTT < 60ms -> Ultra (FRP模式阈值更宽松)
    EXPECT_EQ(GetTargetQualityLevel(0, 1), QUALITY_ULTRA);
    EXPECT_EQ(GetTargetQualityLevel(30, 1), QUALITY_ULTRA);
    EXPECT_EQ(GetTargetQualityLevel(59, 1), QUALITY_ULTRA);
}

TEST_F(QualityAdaptiveTest, RTT_FRP_High) {
    // 60ms <= RTT < 160ms -> High
    EXPECT_EQ(GetTargetQualityLevel(60, 1), QUALITY_HIGH);
    EXPECT_EQ(GetTargetQualityLevel(100, 1), QUALITY_HIGH);
    EXPECT_EQ(GetTargetQualityLevel(159, 1), QUALITY_HIGH);
}

TEST_F(QualityAdaptiveTest, RTT_FRP_Good) {
    // 160ms <= RTT < 300ms -> Good
    EXPECT_EQ(GetTargetQualityLevel(160, 1), QUALITY_GOOD);
    EXPECT_EQ(GetTargetQualityLevel(200, 1), QUALITY_GOOD);
    EXPECT_EQ(GetTargetQualityLevel(299, 1), QUALITY_GOOD);
}

TEST_F(QualityAdaptiveTest, RTT_FRP_Medium) {
    // 300ms <= RTT < 500ms -> Medium
    EXPECT_EQ(GetTargetQualityLevel(300, 1), QUALITY_MEDIUM);
    EXPECT_EQ(GetTargetQualityLevel(400, 1), QUALITY_MEDIUM);
    EXPECT_EQ(GetTargetQualityLevel(499, 1), QUALITY_MEDIUM);
}

TEST_F(QualityAdaptiveTest, RTT_FRP_Low) {
    // 500ms <= RTT < 800ms -> Low
    EXPECT_EQ(GetTargetQualityLevel(500, 1), QUALITY_LOW);
    EXPECT_EQ(GetTargetQualityLevel(600, 1), QUALITY_LOW);
    EXPECT_EQ(GetTargetQualityLevel(799, 1), QUALITY_LOW);
}

TEST_F(QualityAdaptiveTest, RTT_FRP_Minimal) {
    // RTT >= 800ms -> Minimal
    EXPECT_EQ(GetTargetQualityLevel(800, 1), QUALITY_MINIMAL);
    EXPECT_EQ(GetTargetQualityLevel(1000, 1), QUALITY_MINIMAL);
    EXPECT_EQ(GetTargetQualityLevel(2000, 1), QUALITY_MINIMAL);
}

// ============================================
// 质量配置表测试
// ============================================

TEST_F(QualityAdaptiveTest, Profile_Ultra) {
    const auto& p = g_QualityProfiles[QUALITY_ULTRA];
    EXPECT_EQ(p.maxFPS, 25);
    EXPECT_EQ(p.maxWidth, 0);  // 无限制
    EXPECT_EQ(p.algorithm, ALGORITHM_DIFF);
    EXPECT_EQ(p.bitRate, 0);
}

TEST_F(QualityAdaptiveTest, Profile_High) {
    const auto& p = g_QualityProfiles[QUALITY_HIGH];
    EXPECT_EQ(p.maxFPS, 20);
    EXPECT_EQ(p.maxWidth, 0);  // 无限制
    EXPECT_EQ(p.algorithm, ALGORITHM_RGB565);
    EXPECT_EQ(p.bitRate, 0);
}

TEST_F(QualityAdaptiveTest, Profile_Good) {
    const auto& p = g_QualityProfiles[QUALITY_GOOD];
    EXPECT_EQ(p.maxFPS, 20);
    EXPECT_EQ(p.maxWidth, 1920);  // 1080p
    EXPECT_EQ(p.algorithm, ALGORITHM_H264);
    EXPECT_EQ(p.bitRate, 3000);
}

TEST_F(QualityAdaptiveTest, Profile_Medium) {
    const auto& p = g_QualityProfiles[QUALITY_MEDIUM];
    EXPECT_EQ(p.maxFPS, 15);
    EXPECT_EQ(p.maxWidth, 1600);  // 900p
    EXPECT_EQ(p.algorithm, ALGORITHM_H264);
    EXPECT_EQ(p.bitRate, 2000);
}

TEST_F(QualityAdaptiveTest, Profile_Low) {
    const auto& p = g_QualityProfiles[QUALITY_LOW];
    EXPECT_EQ(p.maxFPS, 12);
    EXPECT_EQ(p.maxWidth, 1280);  // 720p
    EXPECT_EQ(p.algorithm, ALGORITHM_H264);
    EXPECT_EQ(p.bitRate, 1200);
}

TEST_F(QualityAdaptiveTest, Profile_Minimal) {
    const auto& p = g_QualityProfiles[QUALITY_MINIMAL];
    EXPECT_EQ(p.maxFPS, 8);
    EXPECT_EQ(p.maxWidth, 1024);  // 540p
    EXPECT_EQ(p.algorithm, ALGORITHM_H264);
    EXPECT_EQ(p.bitRate, 800);
}

// ============================================
// 防抖策略测试 - 启动延迟
// ============================================

TEST_F(QualityAdaptiveTest, Debounce_StartupDelay) {
    debouncer.SetStartTime(0);

    // 启动后60秒内不应改变
    EXPECT_EQ(debouncer.Evaluate(QUALITY_MINIMAL, 1000), -1);
    EXPECT_EQ(debouncer.Evaluate(QUALITY_MINIMAL, 30000), -1);
    EXPECT_EQ(debouncer.Evaluate(QUALITY_MINIMAL, 59999), -1);
}

TEST_F(QualityAdaptiveTest, Debounce_AfterStartupDelay) {
    debouncer.SetStartTime(0);

    // 启动60秒后应该可以改变
    // 需要连续2次降级请求
    debouncer.Evaluate(QUALITY_MINIMAL, 60000);  // 第1次
    int result = debouncer.Evaluate(QUALITY_MINIMAL, 60001);  // 第2次
    EXPECT_EQ(result, QUALITY_MINIMAL);
}

// ============================================
// 防抖策略测试 - 降级
// ============================================

TEST_F(QualityAdaptiveTest, Debounce_Downgrade_RequiresTwice) {
    debouncer.SetStartTime(0);
    uint64_t time = 60000;

    // 第1次降级请求 - 不应立即执行
    int result = debouncer.Evaluate(QUALITY_MINIMAL, time);
    EXPECT_EQ(result, -1);
    EXPECT_EQ(debouncer.GetStableCount(), 1);

    // 第2次降级请求 - 应该执行
    result = debouncer.Evaluate(QUALITY_MINIMAL, time + 100);
    EXPECT_EQ(result, QUALITY_MINIMAL);
    EXPECT_EQ(debouncer.GetCurrentLevel(), QUALITY_MINIMAL);
}

TEST_F(QualityAdaptiveTest, Debounce_Downgrade_ResetOnStable) {
    debouncer.SetStartTime(0);
    uint64_t time = 60000;

    // 第1次降级请求
    debouncer.Evaluate(QUALITY_LOW, time);
    EXPECT_EQ(debouncer.GetStableCount(), 1);

    // 目标等级恢复 - 计数应重置
    debouncer.Evaluate(QUALITY_HIGH, time + 100);  // 当前等级
    EXPECT_EQ(debouncer.GetStableCount(), 0);
}

// ============================================
// 防抖策略测试 - 升级
// ============================================

TEST_F(QualityAdaptiveTest, Debounce_Upgrade_RequiresFiveTimes) {
    debouncer.SetStartTime(0);
    uint64_t time = 60000;

    // 先降级到 MINIMAL
    debouncer.Evaluate(QUALITY_MINIMAL, time);
    debouncer.Evaluate(QUALITY_MINIMAL, time + 100);
    EXPECT_EQ(debouncer.GetCurrentLevel(), QUALITY_MINIMAL);

    // 尝试升级到 ULTRA (需要5次)
    time += 5000;  // 冷却后
    for (int i = 0; i < 4; i++) {
        int result = debouncer.Evaluate(QUALITY_ULTRA, time + i * 100);
        EXPECT_EQ(result, -1);  // 前4次不应执行
        EXPECT_EQ(debouncer.GetStableCount(), i + 1);
    }

    // 第5次应该执行，但只升一级
    int result = debouncer.Evaluate(QUALITY_ULTRA, time + 500);
    EXPECT_EQ(result, QUALITY_LOW);  // MINIMAL -> LOW (只升一级)
}

TEST_F(QualityAdaptiveTest, Debounce_Upgrade_OneStepAtATime) {
    debouncer.SetStartTime(0);
    uint64_t time = 60000;

    // 降级到 MINIMAL
    debouncer.Evaluate(QUALITY_MINIMAL, time);
    debouncer.Evaluate(QUALITY_MINIMAL, time + 100);
    EXPECT_EQ(debouncer.GetCurrentLevel(), QUALITY_MINIMAL);

    // 多次升级请求，验证每次只升一级
    // 从 MINIMAL(5) 升到 HIGH(1) 需要4次升级，每次需要5个稳定请求
    time += 5000;
    int upgradeCount = 0;
    while (debouncer.GetCurrentLevel() > QUALITY_HIGH && upgradeCount < 10) {
        for (int i = 0; i < 5; i++) {
            debouncer.Evaluate(QUALITY_ULTRA, time);  // 请求最高等级
            time += 100;
        }
        time += 5000;  // 冷却
        upgradeCount++;
    }

    // 最终应该回到 HIGH (或更高)
    EXPECT_LE(debouncer.GetCurrentLevel(), QUALITY_HIGH);
}

// ============================================
// 防抖策略测试 - 冷却时间
// ============================================

TEST_F(QualityAdaptiveTest, Debounce_DefaultCooldown) {
    debouncer.SetStartTime(0);
    uint64_t time = 60000;

    // 执行一次降级
    debouncer.Evaluate(QUALITY_LOW, time);
    debouncer.Evaluate(QUALITY_LOW, time + 100);
    EXPECT_EQ(debouncer.GetCurrentLevel(), QUALITY_LOW);

    // 冷却期内不应再次改变
    int result = debouncer.Evaluate(QUALITY_MINIMAL, time + 200);
    EXPECT_EQ(result, -1);

    result = debouncer.Evaluate(QUALITY_MINIMAL, time + 2999);
    EXPECT_EQ(result, -1);
}

TEST_F(QualityAdaptiveTest, Debounce_AfterCooldown) {
    debouncer.SetStartTime(0);
    uint64_t time = 60000;

    // 执行一次降级
    debouncer.Evaluate(QUALITY_LOW, time);
    debouncer.Evaluate(QUALITY_LOW, time + 100);

    // 冷却后应该可以再次改变
    time += 3100;
    debouncer.Evaluate(QUALITY_MINIMAL, time);
    int result = debouncer.Evaluate(QUALITY_MINIMAL, time + 100);
    EXPECT_EQ(result, QUALITY_MINIMAL);
}

// ============================================
// 防抖策略测试 - 分辨率变化冷却
// ============================================

TEST_F(QualityAdaptiveTest, Debounce_ResolutionChange_DowngradeCooldown) {
    debouncer.SetStartTime(0);
    uint64_t time = 60000;

    // 执行一次带分辨率变化的降级
    debouncer.Evaluate(QUALITY_LOW, time, true);  // resolutionChange=true
    debouncer.Evaluate(QUALITY_LOW, time + 100, true);

    // 分辨率降级冷却15秒
    int result = debouncer.Evaluate(QUALITY_MINIMAL, time + 14000, true);
    EXPECT_EQ(result, -1);

    // 15秒后可以
    time += 16000;
    debouncer.Evaluate(QUALITY_MINIMAL, time, true);
    result = debouncer.Evaluate(QUALITY_MINIMAL, time + 100, true);
    EXPECT_EQ(result, QUALITY_MINIMAL);
}

TEST_F(QualityAdaptiveTest, Debounce_ResolutionChange_UpgradeCooldown) {
    debouncer.SetStartTime(0);
    uint64_t time = 60000;

    // 先降级到 MINIMAL (不带分辨率变化，使用默认冷却)
    debouncer.Evaluate(QUALITY_MINIMAL, time);
    debouncer.Evaluate(QUALITY_MINIMAL, time + 100);
    EXPECT_EQ(debouncer.GetCurrentLevel(), QUALITY_MINIMAL);

    // 等待足够长时间（>30秒）后，进行带分辨率变化的升级
    time += 35000;  // 超过30秒冷却
    for (int i = 0; i < 5; i++) {
        debouncer.Evaluate(QUALITY_ULTRA, time + i * 100, true);  // resolutionChange=true
    }
    // 应该成功升级一级 (MINIMAL -> LOW)
    EXPECT_EQ(debouncer.GetCurrentLevel(), QUALITY_LOW);

    // 30秒内不应再次升级（带分辨率变化的升级需要30秒冷却）
    time += 1000;
    for (int i = 0; i < 5; i++) {
        debouncer.Evaluate(QUALITY_ULTRA, time + i * 100, true);
    }
    EXPECT_EQ(debouncer.GetCurrentLevel(), QUALITY_LOW);  // 未改变（还在30秒冷却期内）
}

// ============================================
// 禁用自适应测试
// ============================================

TEST_F(QualityAdaptiveTest, Debounce_Disabled) {
    debouncer.SetStartTime(0);
    debouncer.SetEnabled(false);

    uint64_t time = 60000;
    int result = debouncer.Evaluate(QUALITY_MINIMAL, time);
    EXPECT_EQ(result, -1);

    result = debouncer.Evaluate(QUALITY_MINIMAL, time + 100);
    EXPECT_EQ(result, -1);

    // 质量等级应保持不变
    EXPECT_EQ(debouncer.GetCurrentLevel(), QUALITY_HIGH);
}

// ============================================
// 边界值测试
// ============================================

TEST_F(QualityAdaptiveTest, RTT_Boundary_Zero) {
    EXPECT_EQ(GetTargetQualityLevel(0, 0), QUALITY_ULTRA);
    EXPECT_EQ(GetTargetQualityLevel(0, 1), QUALITY_ULTRA);
}

TEST_F(QualityAdaptiveTest, RTT_Boundary_Negative) {
    // 负值RTT应该仍返回最高质量
    EXPECT_EQ(GetTargetQualityLevel(-1, 0), QUALITY_ULTRA);
    EXPECT_EQ(GetTargetQualityLevel(-100, 0), QUALITY_ULTRA);
}

TEST_F(QualityAdaptiveTest, RTT_Boundary_VeryHigh) {
    // 非常高的RTT
    EXPECT_EQ(GetTargetQualityLevel(10000, 0), QUALITY_MINIMAL);
    EXPECT_EQ(GetTargetQualityLevel(100000, 0), QUALITY_MINIMAL);
    EXPECT_EQ(GetTargetQualityLevel(INT_MAX - 1, 0), QUALITY_MINIMAL);
}

// ============================================
// 常量验证测试
// ============================================

TEST(QualityConstantsTest, QualityLevelEnum) {
    EXPECT_EQ(QUALITY_DISABLED, -2);
    EXPECT_EQ(QUALITY_ADAPTIVE, -1);
    EXPECT_EQ(QUALITY_ULTRA, 0);
    EXPECT_EQ(QUALITY_HIGH, 1);
    EXPECT_EQ(QUALITY_GOOD, 2);
    EXPECT_EQ(QUALITY_MEDIUM, 3);
    EXPECT_EQ(QUALITY_LOW, 4);
    EXPECT_EQ(QUALITY_MINIMAL, 5);
    EXPECT_EQ(QUALITY_COUNT, 6);
}

TEST(QualityConstantsTest, ProfileCount) {
    // 应该有 QUALITY_COUNT 个配置
    EXPECT_EQ(sizeof(g_QualityProfiles) / sizeof(g_QualityProfiles[0]),
              static_cast<size_t>(QUALITY_COUNT));
}

TEST(QualityConstantsTest, ThresholdCount) {
    // 每行应该有 QUALITY_COUNT 个阈值
    EXPECT_EQ(sizeof(g_RttThresholds[0]) / sizeof(g_RttThresholds[0][0]),
              static_cast<size_t>(QUALITY_COUNT));
}

TEST(QualityConstantsTest, ThresholdIncreasing) {
    // 阈值应该递增
    for (int row = 0; row < 2; row++) {
        for (int i = 0; i < QUALITY_COUNT - 1; i++) {
            EXPECT_LT(g_RttThresholds[row][i], g_RttThresholds[row][i + 1])
                << "Row " << row << ", index " << i;
        }
    }
}

TEST(QualityConstantsTest, FRPThresholdsHigher) {
    // FRP模式阈值应该比直连模式高
    for (int i = 0; i < QUALITY_COUNT - 1; i++) {
        EXPECT_GT(g_RttThresholds[1][i], g_RttThresholds[0][i])
            << "Index " << i;
    }
}

// ============================================
// 参数化测试：RTT值遍历
// ============================================

class RTTMappingTest : public ::testing::TestWithParam<std::tuple<int, int, int>> {};

TEST_P(RTTMappingTest, RTT_Mapping) {
    auto [rtt, usingFRP, expectedLevel] = GetParam();
    EXPECT_EQ(GetTargetQualityLevel(rtt, usingFRP), expectedLevel);
}

INSTANTIATE_TEST_SUITE_P(
    DirectMode,
    RTTMappingTest,
    ::testing::Values(
        std::make_tuple(0, 0, QUALITY_ULTRA),
        std::make_tuple(29, 0, QUALITY_ULTRA),
        std::make_tuple(30, 0, QUALITY_HIGH),
        std::make_tuple(79, 0, QUALITY_HIGH),
        std::make_tuple(80, 0, QUALITY_GOOD),
        std::make_tuple(149, 0, QUALITY_GOOD),
        std::make_tuple(150, 0, QUALITY_MEDIUM),
        std::make_tuple(249, 0, QUALITY_MEDIUM),
        std::make_tuple(250, 0, QUALITY_LOW),
        std::make_tuple(399, 0, QUALITY_LOW),
        std::make_tuple(400, 0, QUALITY_MINIMAL),
        std::make_tuple(1000, 0, QUALITY_MINIMAL)
    )
);

INSTANTIATE_TEST_SUITE_P(
    FRPMode,
    RTTMappingTest,
    ::testing::Values(
        std::make_tuple(0, 1, QUALITY_ULTRA),
        std::make_tuple(59, 1, QUALITY_ULTRA),
        std::make_tuple(60, 1, QUALITY_HIGH),
        std::make_tuple(159, 1, QUALITY_HIGH),
        std::make_tuple(160, 1, QUALITY_GOOD),
        std::make_tuple(299, 1, QUALITY_GOOD),
        std::make_tuple(300, 1, QUALITY_MEDIUM),
        std::make_tuple(499, 1, QUALITY_MEDIUM),
        std::make_tuple(500, 1, QUALITY_LOW),
        std::make_tuple(799, 1, QUALITY_LOW),
        std::make_tuple(800, 1, QUALITY_MINIMAL),
        std::make_tuple(2000, 1, QUALITY_MINIMAL)
    )
);

// ============================================
// 质量配置合理性测试
// ============================================

TEST(QualityProfileTest, FPSDecreasing) {
    // FPS 应该随质量降低而减少
    for (int i = 0; i < QUALITY_COUNT - 1; i++) {
        EXPECT_GE(g_QualityProfiles[i].maxFPS, g_QualityProfiles[i + 1].maxFPS)
            << "Level " << i;
    }
}

TEST(QualityProfileTest, MaxWidthDecreasing) {
    // maxWidth 应该随质量降低而减少（除了0表示不限）
    int prevWidth = INT_MAX;
    for (int i = 0; i < QUALITY_COUNT; i++) {
        int width = g_QualityProfiles[i].maxWidth;
        if (width > 0) {
            EXPECT_LE(width, prevWidth) << "Level " << i;
            prevWidth = width;
        }
    }
}

TEST(QualityProfileTest, BitRateDecreasing) {
    // H264 bitRate 应该随质量降低而减少
    int prevBitRate = INT_MAX;
    for (int i = 0; i < QUALITY_COUNT; i++) {
        if (g_QualityProfiles[i].algorithm == ALGORITHM_H264) {
            int bitRate = g_QualityProfiles[i].bitRate;
            EXPECT_LE(bitRate, prevBitRate) << "Level " << i;
            prevBitRate = bitRate;
        }
    }
}
