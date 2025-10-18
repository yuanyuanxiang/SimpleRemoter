#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#pragma once

class ObfsBase {
public:
	// �Գƻ������������ڼ��ܺͽ���
	virtual void ObfuscateBuffer(uint8_t* buf, size_t len, uint32_t seed) {}

	// ������������˳���෴
	virtual void DeobfuscateBuffer(uint8_t* buf, size_t len, uint32_t seed) {}

	// �������������� C �����ʽд���ļ�
	virtual bool WriteBinaryAsCArray(const char* filename, uint8_t* data, size_t length, const char* arrayName) {
		FILE* file = fopen(filename, "w");
		if (!file) return false;

		fprintf(file, "unsigned char %s[] = {\n", arrayName);
		for (size_t i = 0; i < length; ++i) {
			if (i % 24 == 0) fprintf(file, "    ");
			fprintf(file, "0x%02X", data[i]);
			if (i != length - 1) fprintf(file, ",");
			if ((i + 1) % 24 == 0 || i == length - 1) fprintf(file, "\n");
			else fprintf(file, " ");
		}
		fprintf(file, "};\n");
		fprintf(file, "unsigned int %s_len = %zu;\n", arrayName, length);

		fclose(file);
		return true;
	}
};

class Obfs : public ObfsBase {
private:
	// ����8λ����
	static inline uint8_t rol8(uint8_t val, int shift) {
		return (val << shift) | (val >> (8 - shift));
	}

	// ����8λ����
	static inline uint8_t ror8(uint8_t val, int shift) {
		return (val >> shift) | (val << (8 - shift));
	}

public:
	// �Գƻ������������ڼ��ܺͽ���
	virtual void ObfuscateBuffer(uint8_t* buf, size_t len, uint32_t seed) {
		uint32_t state = seed;

		for (size_t i = 0; i < len; ++i) {
			uint8_t mask = (uint8_t)((state >> 16) & 0xFF);
			buf[i] = rol8(buf[i] ^ mask, 3);  // ���+��ת��������
			state = state * 2654435761u + buf[i]; // LCG + �����Ŷ�
		}
	}

	// ������������˳���෴
	virtual void DeobfuscateBuffer(uint8_t* buf, size_t len, uint32_t seed) {
		uint32_t state = seed;

		for (size_t i = 0; i < len; ++i) {
			uint8_t mask = (uint8_t)((state >> 16) & 0xFF);
			uint8_t orig = buf[i];
			buf[i] = ror8(buf[i], 3) ^ mask;
			state = state * 2654435761u + orig; // �����û���ǰ��ԭ�ֽڸ��� state
		}
	}
};
