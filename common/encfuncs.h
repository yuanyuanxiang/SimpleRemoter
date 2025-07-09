#pragma once


// 加密函数
inline void encrypt_v1(unsigned char* data, size_t length, unsigned char key) {
	for (size_t i = 0; i < length; i++) {
		if (i % 2 == 0) {
			data[i] = data[i] + key;  // 偶数索引加 key
		}
		else {
			data[i] = data[i] - key;  // 奇数索引减 key
		}
	}
}

// 解密函数
inline void decrypt_v1(unsigned char* data, size_t length, unsigned char key) {
	for (size_t i = 0; i < length; i++) {
		if (i % 2 == 0) {
			data[i] = data[i] - key;  // 偶数索引减 key 还原
		}
		else {
			data[i] = data[i] + key;  // 奇数索引加 key 还原
		}
	}
}

// 加密函数 - 使用异或和位旋转
inline void encrypt_v2(unsigned char* data, size_t length, unsigned char key) {
	for (size_t i = 0; i < length; i++) {
		// 偶数索引：与key异或后左循环移位1位
		// 奇数索引：与key异或后右循环移位1位
		data[i] ^= key;
		if (i % 2 == 0) {
			data[i] = (data[i] << 1) | (data[i] >> 7);  // 左循环移位
		}
		else {
			data[i] = (data[i] >> 1) | (data[i] << 7);  // 右循环移位
		}
	}
}

// 解密函数
inline void decrypt_v2(unsigned char* data, size_t length, unsigned char key) {
	for (size_t i = 0; i < length; i++) {
		// 加密的逆操作
		if (i % 2 == 0) {
			data[i] = (data[i] >> 1) | (data[i] << 7);  // 右循环移位还原
		}
		else {
			data[i] = (data[i] << 1) | (data[i] >> 7);  // 左循环移位还原
		}
		data[i] ^= key;  // 再次异或还原
	}
}

// 加密函数 V3 - 基于索引和key的动态计算
inline void encrypt_v3(unsigned char* data, size_t length, unsigned char key) {
	for (size_t i = 0; i < length; i++) {
		unsigned char dynamic_key = key + (i % 8);  // 动态变化的key（基于索引）
		if (i % 3 == 0) {
			data[i] = (data[i] + dynamic_key) ^ dynamic_key;  // 加法 + 异或
		}
		else if (i % 3 == 1) {
			data[i] = (data[i] ^ dynamic_key) - dynamic_key;  // 异或 + 减法
		}
		else {
			data[i] = ~(data[i] + dynamic_key);  // 取反 + 加法
		}
	}
}

// 解密函数 V3
inline void decrypt_v3(unsigned char* data, size_t length, unsigned char key) {
	for (size_t i = 0; i < length; i++) {
		unsigned char dynamic_key = key + (i % 8);
		if (i % 3 == 0) {
			data[i] = (data[i] ^ dynamic_key) - dynamic_key;  // 逆操作：先异或再减
		}
		else if (i % 3 == 1) {
			data[i] = (data[i] + dynamic_key) ^ dynamic_key;   // 逆操作：先加再异或
		}
		else {
			data[i] = ~data[i] - dynamic_key;                 // 逆操作：取反再减
		}
	}
}

// 加密函数 V4 - 基于伪随机序列（简单线性同余生成器）
inline void encrypt_v4(unsigned char* data, size_t length, unsigned char key) {
	unsigned char rand = key;
	for (size_t i = 0; i < length; i++) {
		rand = (rand * 13 + 17) % 256;  // 伪随机数生成（LCG）
		data[i] ^= rand;                 // 用伪随机数异或加密
	}
}

// 解密函数 V4（与加密完全相同，因为异或的自反性）
inline void decrypt_v4(unsigned char* data, size_t length, unsigned char key) {
	encrypt_v4(data, length, key);  // 异或加密的解密就是再执行一次
}

// 加密函数 V5 - V5 版本（动态密钥派生 + 多重位运算）
inline void encrypt_v5(unsigned char* data, size_t length, unsigned char key) {
	for (size_t i = 0; i < length; i++) {
		unsigned char dynamic_key = (key + i) ^ 0x55;  // 动态密钥派生
		data[i] = ((data[i] + dynamic_key) ^ (dynamic_key << 3)) + (i % 7);
	}
}

// 解密函数 V5
inline void decrypt_v5(unsigned char* data, size_t length, unsigned char key) {
	for (size_t i = 0; i < length; i++) {
		unsigned char dynamic_key = (key + i) ^ 0x55;
		data[i] = (((data[i] - (i % 7)) ^ (dynamic_key << 3)) - dynamic_key);
	}
}

// 加密/解密函数 V6（自反性） - V6 版本（伪随机流混淆 + 自反性解密）
inline void encrypt_v6(unsigned char* data, size_t length, unsigned char key) {
	unsigned char rand = key;
	for (size_t i = 0; i < length; i++) {
		rand = (rand * 31 + 17) % 256;  // 简单伪随机生成
		data[i] ^= rand + i;            // 异或动态值
	}
}

// 解密函数 V6（直接调用 encrypt_v6 即可）
inline void decrypt_v6(unsigned char* data, size_t length, unsigned char key) {
	encrypt_v6(data, length, key);  // 异或的自反性
}
