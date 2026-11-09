#pragma once
#include <array>
#include <optional>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <condition_variable>
#include <vector>

// 一行识别结果里的单个字符外接框，坐标是图像像素（与喂给 OCR 的图同一坐标系）
struct OcrChar
{
	float x0{ 0.f }, y0{ 0.f }, x1{ 0.f }, y1{ 0.f };
};

// 一行文字的识别结果：det 检出的一个文本框 + rec 识别出的内容。
// box 覆盖整行；chars 与 text 等长（按 wchar 计），给双击选词用
struct OcrLine
{
	float x0{ 0.f }, y0{ 0.f }, x1{ 0.f }, y1{ 0.f };
	std::wstring text;
	std::vector<OcrChar> chars;
};

// PP-OCRv6 推理引擎（ncnn 全静态，det -> rec，中英日）。
// 非线程安全：仅供 OcrWorker 的后台线程调用。
class OcrEngine
{
public:
	// 懒加载：首次调用时把模型读进来，失败返回空结果（完全静默，不打扰用户）。
	// 输入 BGRA、top-down、行紧凑（与 Util 其他图像函数一致）。
	// 返回结果按阅读序排好（先上后下、同行先左后右）。
	std::vector<OcrLine> recognize(const BYTE* bgra, int w, int h);
private:
	bool ensureModel();
	// det：全图文本检测，输出候选框（图像像素坐标，已按 unclip 外扩）
	std::vector<std::array<float, 4>> detect(const BYTE* bgra, int w, int h);
	// rec：对裁出的文本条识别。除文本外还带出每个字符的横向区间（裁剪框内坐标），供字符级选取
	std::wstring recognizeLine(const BYTE* bgra, int w, int h,
		const std::array<float, 4>& box, std::vector<OcrChar>& chars);
private:
	// 模型只加载一次，recognize 只会在工作线程被调，无需加锁
	bool loaded{ false };
	std::vector<std::wstring> dict;
};

// OCR 后台工作线程。submit 只保留最近一个任务（防抖重跑时旧任务直接被顶掉），
// 结果通过 onResult 回调交给调用方（回调里再 TryEnqueue 回 UI 线程）。
class OcrWorker
{
public:
	using ResultCallback = std::function<void(UINT64 taskId, std::vector<OcrLine> lines)>;
	~OcrWorker();
	void start(ResultCallback onResult);
	// 停线程并等当前任务跑完（单帧几百毫秒，不会卡多久）
	void stop();
	// pixels 整份移交。taskId 由调用方递增，用来丢弃过期结果
	void submit(UINT64 taskId, std::vector<BYTE>&& pixels, int w, int h);
private:
	void loop();
private:
	std::thread thread;
	std::mutex mtx;
	std::condition_variable cv;
	struct Task
	{
		UINT64 id{ 0 };
		std::vector<BYTE> pixels;
		int w{ 0 }, h{ 0 };
	};
	std::optional<Task> pending;
	bool running{ false };
	ResultCallback onResult;
};
