#pragma once

#include <vector>
#include <cstdint>
#include <string>

// Encode for IP and Port.
// provided by ChatGPT.
class StreamCipher
{
private:
    uint32_t state;

    // 简单非线性伪随机数生成器
    uint8_t prngNext()
    {
        // 例子：xorshift32，改造一点非线性
        state ^= (state << 13);
        state ^= (state >> 17);
        state ^= (state << 5);
        // 再混合一个简单的非线性变换
        uint8_t out = (state & 0xFF) ^ ((state >> 8) & 0xFF);
        return out;
    }

public:
    StreamCipher(uint32_t key) : state(key) {}

    // 加密解密（对称，长度不变）
    void process(uint8_t* data, size_t len)
    {
        for (size_t i = 0; i < len; ++i) {
            data[i] ^= prngNext();
        }
    }
};

class PrintableXORCipher
{
public:
    // 对称加解密，输入和输出均为可打印字符
    // 前提：输入是32~126范围的字符
    void process(char* data, size_t len)
    {
        for (size_t i = 0; i < len; ++i) {
            char c = data[i];
            // 保证输入字符是可打印范围
            if (c < 32 || c > 126) {
                // 不处理非打印字符（或者你也可以自定义错误处理）
                continue;
            }
            // 异或0x55（'U'）且确保结果仍是32~126之间
            char encrypted = c ^ 0x55;
            // 如果不在范围，修正回范围内（简单加减循环）
            if (encrypted < 32) encrypted += 95;
            if (encrypted > 126) encrypted -= 95;
            data[i] = encrypted;
        }
    }
};
