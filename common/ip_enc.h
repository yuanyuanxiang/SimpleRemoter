#pragma once

#include <vector>
#include <cstdint>
#include <string>

// Encode for IP and Port.
// provided by ChatGPT.
class StreamCipher {
private:
	uint32_t state;

	// 简单非线性伪随机数生成器
	uint8_t prngNext() {
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
	void process(uint8_t* data, size_t len) {
		for (size_t i = 0; i < len; ++i) {
			data[i] ^= prngNext();
		}
	}
};
