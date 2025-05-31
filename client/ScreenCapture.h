#pragma once
#include "stdafx.h"
#include <assert.h>
#include "CursorInfo.h"
#include "../common/commands.h"

#define DEFAULT_GOP 0x7FFFFFFF

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include "X264Encoder.h"


class ThreadPool {
public:
	// ���캯���������̶��������߳�
	ThreadPool(size_t numThreads) : stop(false) {
		for (size_t i = 0; i < numThreads; ++i) {
			workers.emplace_back([this] {
				while (true) {
					std::function<void()> task;
					{
						std::unique_lock<std::mutex> lock(this->queueMutex);
						this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
						if (this->stop && this->tasks.empty()) return;
						task = std::move(this->tasks.front());
						this->tasks.pop();
					}

					try {
						task();
					}
					catch (...) {
						// �����쳣
					}
				}
				});
		}
	}

	// ���������������̳߳�
	~ThreadPool() {
		{
			std::unique_lock<std::mutex> lock(queueMutex);
			stop = true;
		}
		condition.notify_all();
		for (std::thread& worker : workers)
			worker.join();
	}

	// �����ύ
	template<typename F>
	auto enqueue(F&& f) -> std::future<decltype(f())> {
		using ReturnType = decltype(f());
		auto task = std::make_shared<std::packaged_task<ReturnType()>>(std::forward<F>(f));
		std::future<ReturnType> res = task->get_future();
		{
			std::unique_lock<std::mutex> lock(queueMutex);
			tasks.emplace([task]() { (*task)(); });
		}
		condition.notify_one();
		return res;
	}

	void waitAll() {
		std::unique_lock<std::mutex> lock(queueMutex);
		condition.wait(lock, [this] { return tasks.empty(); });
	}

private:
	std::vector<std::thread> workers;
	std::queue<std::function<void()>> tasks;

	std::mutex queueMutex;
	std::condition_variable condition;
	std::atomic<bool> stop;
};

class ScreenCapture
{
public:
	ThreadPool*      m_ThreadPool;		 // �̳߳�
	BYTE*            m_FirstBuffer;		 // ��һ֡����
	BYTE*			 m_RectBuffer;       // ��ǰ������
	LPBYTE*		     m_BlockBuffers;	 // �ֿ黺��
	ULONG*			 m_BlockSizes;       // �ֿ����������
	int				 m_BlockNum;         // �ֿ����
	int				 m_SendQuality;		 // ��������

	LPBITMAPINFO     m_BitmapInfor_Full; // BMP��Ϣ
	BYTE             m_bAlgorithm;       // ��Ļ�����㷨

	ULONG			 m_iScreenX;		 // ��ʼx����
	ULONG			 m_iScreenY;		 // ��ʼy����
	ULONG            m_ulFullWidth;      // ��Ļ��
	ULONG            m_ulFullHeight;     // ��Ļ��
	bool             m_bZoomed;          // ��Ļ������
	double           m_wZoom;            // ��Ļ�������ű�
	double           m_hZoom;            // ��Ļ�������ű�

	int			     m_biBitCount;		 // ÿ���ر�����
	int              m_FrameID;          // ֡���
	int              m_GOP;              // �ؼ�֡���
	bool		     m_SendKeyFrame;	 // ���͹ؼ�֡
	CX264Encoder	 *m_encoder;		 // ������

	ScreenCapture(int n = 32, BYTE algo = ALGORITHM_DIFF) : 
		m_ThreadPool(nullptr), m_FirstBuffer(nullptr), m_RectBuffer(nullptr),
		m_BitmapInfor_Full(nullptr), m_bAlgorithm(ALGORITHM_DIFF), m_SendQuality(100),
		m_ulFullWidth(0), m_ulFullHeight(0), m_bZoomed(false), m_wZoom(1), m_hZoom(1),
		m_FrameID(0), m_GOP(DEFAULT_GOP), m_iScreenX(0), m_iScreenY(0), m_biBitCount(n),
		m_SendKeyFrame(false), m_encoder(nullptr) {

		m_BlockNum = 8;
		m_ThreadPool = new ThreadPool(m_BlockNum);

		//::GetSystemMetrics(SM_CXSCREEN/SM_CYSCREEN)��ȡ��Ļ��С��׼
		//���統��Ļ��ʾ����Ϊ125%ʱ����ȡ������Ļ��С��Ҫ����1.25�Ŷ�
		DEVMODE devmode;
		memset(&devmode, 0, sizeof(devmode));
		devmode.dmSize = sizeof(DEVMODE);
		devmode.dmDriverExtra = 0;
		BOOL Isgetdisplay = EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &devmode);
		m_ulFullWidth = devmode.dmPelsWidth;
		m_ulFullHeight = devmode.dmPelsHeight;
		int w = GetSystemMetrics(SM_CXSCREEN), h = GetSystemMetrics(SM_CYSCREEN);
		m_bZoomed = (w != m_ulFullWidth) || (h != m_ulFullHeight);
		m_wZoom = double(m_ulFullWidth) / w, m_hZoom = double(m_ulFullHeight) / h;
		Mprintf("=> �������ű���: %.2f, %.2f\t�ֱ��ʣ�%d x %d\n", m_wZoom, m_hZoom, m_ulFullWidth, m_ulFullHeight);
		m_wZoom = 1.0 / m_wZoom, m_hZoom = 1.0 / m_hZoom;

		if (ALGORITHM_H264 == m_bAlgorithm)
		{
			m_encoder = new CX264Encoder();
			if (!m_encoder->open(m_ulFullWidth, m_ulFullHeight, 20, m_ulFullWidth * m_ulFullHeight / 1266)) {
				Mprintf("Open x264encoder failed!!!\n");
			}
		}

		m_BlockBuffers = new LPBYTE[m_BlockNum];
		m_BlockSizes = new ULONG[m_BlockNum];
		for (int blockY = 0; blockY < m_BlockNum; ++blockY) {
			m_BlockBuffers[blockY] = new BYTE[m_ulFullWidth * m_ulFullHeight * 4 * 2 / m_BlockNum + 12];
		}
	}
	virtual ~ScreenCapture(){
		if (m_BitmapInfor_Full != NULL) {
			delete[](char*)m_BitmapInfor_Full;
			m_BitmapInfor_Full = NULL;
		}
		SAFE_DELETE_ARRAY(m_RectBuffer);

		for (int blockY = 0; blockY < m_BlockNum; ++blockY) {
			SAFE_DELETE_ARRAY(m_BlockBuffers[blockY]);
		}
		SAFE_DELETE_ARRAY(m_BlockBuffers);
		SAFE_DELETE_ARRAY(m_BlockSizes);

		SAFE_DELETE(m_ThreadPool);
		SAFE_DELETE(m_encoder);
	}

	virtual int SendQuality(int quality) {
		int old = m_SendQuality;
		m_SendQuality = quality;
		return old;
	}

	virtual RECT GetScreenRect() const {
		RECT rect;
		rect.left = m_iScreenX;
		rect.top = m_iScreenY;
		rect.right = m_ulFullWidth;
		rect.bottom = m_ulFullHeight;
		return rect;
	}

public:
	//*************************************** ͼ������㷨�����У� *************************************
	virtual ULONG CompareBitmap(LPBYTE CompareSourData, LPBYTE CompareDestData, LPBYTE szBuffer,
		DWORD ulCompareLength, BYTE algo, int startPostion = 0) {

		// Windows�涨һ��ɨ������ռ���ֽ���������4�ı���, ������DWORD�Ƚ�
		LPDWORD	p1 = (LPDWORD)CompareDestData, p2 = (LPDWORD)CompareSourData;
		LPBYTE p = szBuffer;
		ULONG channel = algo == ALGORITHM_GRAY ? 1 : 4;
		ULONG ratio = algo == ALGORITHM_GRAY ? 4 : 1;
		for (ULONG i = 0; i < ulCompareLength; i += 4, ++p1, ++p2) {
			if (*p1 == *p2)
				continue;
			ULONG index = i;
			LPDWORD pos1 = p1++, pos2 = p2++;
			// �����м�������ֵ��ͬ
			for (i += 4; i < ulCompareLength && *p1 != *p2; i += 4, ++p1, ++p2);
			ULONG ulCount = i - index;
			memcpy(pos1, pos2, ulCount); // ����Ŀ������

			*(LPDWORD)(p) = index + startPostion;
			*(LPDWORD)(p + sizeof(ULONG)) = ulCount / ratio;
			p += 2 * sizeof(ULONG);
			if (channel != 1) {
				memcpy(p, pos2, ulCount);
				p += ulCount;
			}
			else {
				for (LPBYTE end = p + ulCount / ratio; p < end; p += channel, ++pos2) {
					LPBYTE src = (LPBYTE)pos2;
					*p = (306 * src[2] + 601 * src[0] + 117 * src[1]) >> 10;
				}
			}
		}

		return p - szBuffer;
	}

	//*************************************** ͼ������㷨�����У� *************************************
	ULONG MultiCompareBitmap(LPBYTE srcData, LPBYTE dstData, LPBYTE szBuffer,
		DWORD ulCompareLength, BYTE algo) {

		int N = m_BlockNum;
		ULONG blockLength = ulCompareLength / N;  // ÿ������Ļ����ֽ���
		ULONG remainingLength = ulCompareLength % N;  // ʣ����ֽ���

		std::vector<std::future<ULONG>> futures;
		for (int blockY = 0; blockY < N; ++blockY) {
			// ���㵱ǰ������ֽ���
			ULONG currentBlockLength = blockLength + (blockY == N - 1 ? remainingLength : 0);
			// ���㵱ǰ�������ʼλ��
			ULONG startPosition = blockY * blockLength;

			futures.emplace_back(m_ThreadPool->enqueue([=]() -> ULONG {
				LPBYTE srcBlock = srcData + startPosition;
				LPBYTE dstBlock = dstData + startPosition;
				LPBYTE blockBuffer = m_BlockBuffers[blockY];

				// ����ǰ���񲢷��رȶ����ݴ�С
				return m_BlockSizes[blockY] = CompareBitmap(srcBlock, dstBlock, blockBuffer, currentBlockLength, algo, startPosition);
				}));
		}

		// �ȴ�����������ɲ���ȡ����ֵ
		for (auto& future : futures) {
			future.get();
		}

		// �ϲ����п�Ĳ�����Ϣ�� szBuffer
		ULONG offset = 0;
		for (int blockY = 0; blockY < N; ++blockY) {
			memcpy(szBuffer + offset, m_BlockBuffers[blockY], m_BlockSizes[blockY]);
			offset += m_BlockSizes[blockY];
		}

		return offset;  // ���ػ������Ĵ�С
	}

	virtual int GetFrameID() const {
		return m_FrameID;
	}

	virtual LPBYTE GetFirstBuffer() const {
		return m_FirstBuffer;
	}

	virtual int GetBMPSize() const {
		assert(m_BitmapInfor_Full);
		return m_BitmapInfor_Full->bmiHeader.biSizeImage;
	}

	void ToGray(LPBYTE dst, LPBYTE src, int biSizeImage) {
		for (ULONG i = 0; i < biSizeImage; i += 4, dst += 4, src += 4) {
			dst[0] = dst[1] = dst[2] = (306 * src[2] + 601 * src[0] + 117 * src[1]) >> 10;
		}
	}

	virtual LPBITMAPINFO ConstructBitmapInfo(int biBitCount, int biWidth, int biHeight)
	{
		assert(biBitCount == 32);
		BITMAPINFO* bmpInfo = (BITMAPINFO*) new BYTE[sizeof(BITMAPINFO)]();
		bmpInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmpInfo->bmiHeader.biWidth = biWidth;
		bmpInfo->bmiHeader.biHeight = biHeight;
		bmpInfo->bmiHeader.biPlanes = 1;
		bmpInfo->bmiHeader.biBitCount = 32;
		bmpInfo->bmiHeader.biCompression = BI_RGB;
		bmpInfo->bmiHeader.biSizeImage = biWidth * biHeight * 4;
		return bmpInfo;
	}

	// �㷨+���λ��+�������
	virtual LPBYTE GetNextScreenData(ULONG* ulNextSendLength) {
		BYTE algo = m_bAlgorithm;
		int frameID = m_FrameID + 1;
		bool keyFrame = (frameID % m_GOP == 0);
		m_RectBuffer[0] = keyFrame ? TOKEN_KEYFRAME : TOKEN_NEXTSCREEN;
		LPBYTE data = m_RectBuffer + 1;
		// д��ʹ���������㷨
		memcpy(data, (LPBYTE)&algo, sizeof(BYTE));

		// д����λ��
		POINT	CursorPos;
		GetCursorPos(&CursorPos);
		CursorPos.x /= m_wZoom;
		CursorPos.y /= m_hZoom;
		memcpy(data + sizeof(BYTE), (LPBYTE)&CursorPos, sizeof(POINT));

		// д�뵱ǰ�������
		static CCursorInfo m_CursorInfor;
		BYTE	bCursorIndex = m_CursorInfor.getCurrentCursorIndex();
		memcpy(data + sizeof(BYTE) + sizeof(POINT), &bCursorIndex, sizeof(BYTE));
		ULONG offset = sizeof(BYTE) + sizeof(POINT) + sizeof(BYTE);

		// �ֶ�ɨ��ȫ��Ļ  ���µ�λͼ���뵽m_hDiffMemDC��
		LPBYTE nextData = ScanNextScreen();
		if (nullptr == nextData) {
			// ɨ����һ֡ʧ��Ҳ��Ҫ���͹����Ϣ�����ƶ�
			*ulNextSendLength = 1 + offset;
			return m_RectBuffer;
		}

#if SCREENYSPY_IMPROVE
		memcpy(data + offset, &++m_FrameID, sizeof(int));
		offset += sizeof(int);
#if SCREENSPY_WRITE
		WriteBitmap(m_BitmapInfor_Full, nextData, "GHOST", m_FrameID);
#endif
#else
		m_FrameID++;
#endif

		if (keyFrame)
		{
			switch (algo)
			{
			case ALGORITHM_DIFF: {
				*ulNextSendLength = 1 + offset + m_BitmapInfor_Full->bmiHeader.biSizeImage;
				memcpy(data + offset, nextData, m_BitmapInfor_Full->bmiHeader.biSizeImage);
				break;
			}
			case ALGORITHM_GRAY: {
				*ulNextSendLength = 1 + offset + m_BitmapInfor_Full->bmiHeader.biSizeImage;
				ToGray(data + offset, nextData, m_BitmapInfor_Full->bmiHeader.biSizeImage);
				break;
			}
			case ALGORITHM_H264: {
				uint8_t* encoded_data = nullptr;
				uint32_t  encoded_size = 0;
				int err = m_encoder->encode(nextData, 32, 4*m_BitmapInfor_Full->bmiHeader.biWidth, 
					m_ulFullWidth, m_ulFullHeight, &encoded_data, &encoded_size);
				if (err) {
					return nullptr;
				}
				*ulNextSendLength = 1 + offset + encoded_size;
				memcpy(data + offset, encoded_data, encoded_size);
				break;
			}
			default:
				break;
			}
			memcpy(GetFirstBuffer(), nextData, m_BitmapInfor_Full->bmiHeader.biSizeImage);
		}
		else {
			switch (algo)
			{
			case ALGORITHM_DIFF: case ALGORITHM_GRAY: {
				*ulNextSendLength = 1 + offset + MultiCompareBitmap(nextData, GetFirstBuffer(), data + offset, GetBMPSize(), algo);
				break;
			}
			case ALGORITHM_H264: {
				uint8_t* encoded_data = nullptr;
				uint32_t  encoded_size = 0;
				int err = m_encoder->encode(nextData, 32, 4 * m_BitmapInfor_Full->bmiHeader.biWidth,
					m_ulFullWidth, m_ulFullHeight, &encoded_data, &encoded_size);
				if (err) {
					return nullptr;
				}
				*ulNextSendLength = 1 + offset + encoded_size;
				memcpy(data + offset, encoded_data, encoded_size);
				break;
			}
			default:
				break;
			}
		}

		return m_RectBuffer;
	}

	// ������Ļ�����㷨
	virtual BYTE SetAlgorithm(int algo) {
		BYTE oldAlgo = m_bAlgorithm;
		m_bAlgorithm = algo;
		return oldAlgo;
	}

	// ���λ��ת��
	virtual void PointConversion(POINT& pt) const {
		if (m_bZoomed) {
			pt.x *= m_wZoom;
			pt.y *= m_hZoom;
		}
	}

	// ��ȡλͼ�ṹ��Ϣ
	virtual const LPBITMAPINFO& GetBIData() const {
		return m_BitmapInfor_Full;
	}

public: // ����ӿ�

	// ��ȡ��һ֡��Ļ
	virtual LPBYTE GetFirstScreenData(ULONG* ulFirstScreenLength) = 0;

	// ��ȡ��һ֡��Ļ
	virtual LPBYTE ScanNextScreen() = 0;
};
