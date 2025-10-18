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

    // �򵥷�����α�����������
    uint8_t prngNext()
    {
        // ���ӣ�xorshift32������һ�������
        state ^= (state << 13);
        state ^= (state >> 17);
        state ^= (state << 5);
        // �ٻ��һ���򵥵ķ����Ա任
        uint8_t out = (state & 0xFF) ^ ((state >> 8) & 0xFF);
        return out;
    }

public:
    StreamCipher(uint32_t key) : state(key) {}

    // ���ܽ��ܣ��Գƣ����Ȳ��䣩
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
    // �ԳƼӽ��ܣ�����������Ϊ�ɴ�ӡ�ַ�
    // ǰ�᣺������32~126��Χ���ַ�
    void process(char* data, size_t len)
    {
        for (size_t i = 0; i < len; ++i) {
            char c = data[i];
            // ��֤�����ַ��ǿɴ�ӡ��Χ
            if (c < 32 || c > 126) {
                // ������Ǵ�ӡ�ַ���������Ҳ�����Զ��������
                continue;
            }
            // ���0x55��'U'����ȷ���������32~126֮��
            char encrypted = c ^ 0x55;
            // ������ڷ�Χ�������ط�Χ�ڣ��򵥼Ӽ�ѭ����
            if (encrypted < 32) encrypted += 95;
            if (encrypted > 126) encrypted -= 95;
            data[i] = encrypted;
        }
    }
};
