#pragma once


// ���ܺ���
inline void encrypt_v1(unsigned char* data, size_t length, unsigned char key) {
	for (size_t i = 0; i < length; i++) {
		if (i % 2 == 0) {
			data[i] = data[i] + key;  // ż�������� key
		}
		else {
			data[i] = data[i] - key;  // ���������� key
		}
	}
}

// ���ܺ���
inline void decrypt_v1(unsigned char* data, size_t length, unsigned char key) {
	for (size_t i = 0; i < length; i++) {
		if (i % 2 == 0) {
			data[i] = data[i] - key;  // ż�������� key ��ԭ
		}
		else {
			data[i] = data[i] + key;  // ���������� key ��ԭ
		}
	}
}

// ���ܺ��� - ʹ������λ��ת
inline void encrypt_v2(unsigned char* data, size_t length, unsigned char key) {
	for (size_t i = 0; i < length; i++) {
		// ż����������key������ѭ����λ1λ
		// ������������key������ѭ����λ1λ
		data[i] ^= key;
		if (i % 2 == 0) {
			data[i] = (data[i] << 1) | (data[i] >> 7);  // ��ѭ����λ
		}
		else {
			data[i] = (data[i] >> 1) | (data[i] << 7);  // ��ѭ����λ
		}
	}
}

// ���ܺ���
inline void decrypt_v2(unsigned char* data, size_t length, unsigned char key) {
	for (size_t i = 0; i < length; i++) {
		// ���ܵ������
		if (i % 2 == 0) {
			data[i] = (data[i] >> 1) | (data[i] << 7);  // ��ѭ����λ��ԭ
		}
		else {
			data[i] = (data[i] << 1) | (data[i] >> 7);  // ��ѭ����λ��ԭ
		}
		data[i] ^= key;  // �ٴ����ԭ
	}
}

// ���ܺ��� V3 - ����������key�Ķ�̬����
inline void encrypt_v3(unsigned char* data, size_t length, unsigned char key) {
	for (size_t i = 0; i < length; i++) {
		unsigned char dynamic_key = key + (i % 8);  // ��̬�仯��key������������
		if (i % 3 == 0) {
			data[i] = (data[i] + dynamic_key) ^ dynamic_key;  // �ӷ� + ���
		}
		else if (i % 3 == 1) {
			data[i] = (data[i] ^ dynamic_key) - dynamic_key;  // ��� + ����
		}
		else {
			data[i] = ~(data[i] + dynamic_key);  // ȡ�� + �ӷ�
		}
	}
}

// ���ܺ��� V3
inline void decrypt_v3(unsigned char* data, size_t length, unsigned char key) {
	for (size_t i = 0; i < length; i++) {
		unsigned char dynamic_key = key + (i % 8);
		if (i % 3 == 0) {
			data[i] = (data[i] ^ dynamic_key) - dynamic_key;  // �������������ټ�
		}
		else if (i % 3 == 1) {
			data[i] = (data[i] + dynamic_key) ^ dynamic_key;   // ��������ȼ������
		}
		else {
			data[i] = ~data[i] - dynamic_key;                 // �������ȡ���ټ�
		}
	}
}

// ���ܺ��� V4 - ����α������У�������ͬ����������
inline void encrypt_v4(unsigned char* data, size_t length, unsigned char key) {
	unsigned char rand = key;
	for (size_t i = 0; i < length; i++) {
		rand = (rand * 13 + 17) % 256;  // α��������ɣ�LCG��
		data[i] ^= rand;                 // ��α�����������
	}
}

// ���ܺ��� V4���������ȫ��ͬ����Ϊ�����Է��ԣ�
inline void decrypt_v4(unsigned char* data, size_t length, unsigned char key) {
	encrypt_v4(data, length, key);  // �����ܵĽ��ܾ�����ִ��һ��
}

// ���ܺ��� V5 - V5 �汾����̬��Կ���� + ����λ���㣩
inline void encrypt_v5(unsigned char* data, size_t length, unsigned char key) {
	for (size_t i = 0; i < length; i++) {
		unsigned char dynamic_key = (key + i) ^ 0x55;  // ��̬��Կ����
		data[i] = ((data[i] + dynamic_key) ^ (dynamic_key << 3)) + (i % 7);
	}
}

// ���ܺ��� V5
inline void decrypt_v5(unsigned char* data, size_t length, unsigned char key) {
	for (size_t i = 0; i < length; i++) {
		unsigned char dynamic_key = (key + i) ^ 0x55;
		data[i] = (((data[i] - (i % 7)) ^ (dynamic_key << 3)) - dynamic_key);
	}
}

// ����/���ܺ��� V6���Է��ԣ� - V6 �汾��α��������� + �Է��Խ��ܣ�
inline void encrypt_v6(unsigned char* data, size_t length, unsigned char key) {
	unsigned char rand = key;
	for (size_t i = 0; i < length; i++) {
		rand = (rand * 31 + 17) % 256;  // ��α�������
		data[i] ^= rand + i;            // ���ֵ̬
	}
}

// ���ܺ��� V6��ֱ�ӵ��� encrypt_v6 ���ɣ�
inline void decrypt_v6(unsigned char* data, size_t length, unsigned char key) {
	encrypt_v6(data, length, key);  // �����Է���
}
