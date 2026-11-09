#include "pch.h"
#include "Ocr.h"
#include <net.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

// 嵌入资源里的模型（Resource.rc 中 #ifdef OCR_EMBED_MODELS 时才编译进去）。
// 没嵌时从 exe 同目录的 ocr\ 文件夹读，两处都没有就静默返回空结果
namespace
{
	// det 预处理：最长边压到 960 并对齐 32（PP-OCR 系列的惯例输入约束）
	constexpr float kDetLimitSide{ 960.f };
	// DB 后处理：概率图二值化阈值、外扩系数、最小框边长（像素）
	constexpr float kDetThresh{ 0.3f };
	constexpr float kDetUnclipRatio{ 1.5f };
	constexpr int kDetMinSize{ 3 };
	// rec 输入高度。PP-OCR 系列的识别头是 48，换输入高不同的模型时改这里
	constexpr int kRecHeight{ 48 };
	// 单行最长宽高比 8（超过的文本条会被横向压扁，识别精度下降但不会崩）
	constexpr float kRecMaxRatio{ 8.f };
	constexpr int kRecMinWidth{ 16 };
	constexpr int kThreads{ 4 };

	// 模型资源 ID，与 Resource.rc 里的条目一一对应
	constexpr int kResDetParam{ 201 };
	constexpr int kResDetBin{ 202 };
	constexpr int kResRecParam{ 203 };
	constexpr int kResRecBin{ 204 };
	constexpr int kResDict{ 205 };

	// 引擎只被 OcrWorker 的后台线程访问，文件级静态即可
	ncnn::Net detNet, recNet;

	// 读一份 RCDATA 资源，返回空表示没有嵌
	std::vector<unsigned char> loadResource(int id)
	{
		std::vector<unsigned char> data;
		auto res = FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA);
		if (!res) return data;
		auto handle = LoadResource(nullptr, res);
		if (!handle) return data;
		auto ptr = static_cast<const unsigned char*>(LockResource(handle));
		auto size = SizeofResource(nullptr, res);
		if (!ptr || !size) return data;
		data.assign(ptr, ptr + size);
		return data;
	}

	std::filesystem::path exeDir()
	{
		wchar_t buffer[MAX_PATH]{};
		GetModuleFileNameW(nullptr, buffer, MAX_PATH);
		return std::filesystem::path{ buffer }.parent_path();
	}

	// UTF-8 字典一行一个字符；去掉行尾 \r，空行跳过
	std::vector<std::wstring> parseDict(const unsigned char* data, size_t size)
	{
		std::vector<std::wstring> dict;
		std::string text{ reinterpret_cast<const char*>(data), size };
		size_t pos{ 0 };
		while (pos < text.size()) {
			auto end = text.find('\n', pos);
			if (end == std::string::npos) end = text.size();
			auto line = text.substr(pos, end - pos);
			if (!line.empty() && line.back() == '\r') line.pop_back();
			if (!line.empty()) {
				auto need = MultiByteToWideChar(CP_UTF8, 0, line.data(), (int)line.size(), nullptr, 0);
				if (need > 0) {
					std::wstring wch(need, L'\0');
					MultiByteToWideChar(CP_UTF8, 0, line.data(), (int)line.size(), wch.data(), need);
					dict.push_back(std::move(wch));
				}
			}
			if (end == text.size()) break;
			pos = end + 1;
		}
		return dict;
	}

	// BGRA 双线性缩放。OCR 输入最大 960 见方，性能不是瓶颈
	void resizeBilinear(const BYTE* src, int sw, int sh, std::vector<BYTE>& dst, int dw, int dh)
	{
		dst.assign((size_t)dw * dh * 4, 0);
		if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
		// 目标中心反推源坐标（half pixel 对齐），边缘 clamp
		const float rx = (dw > 1) ? (float)(sw - 1) / (dw - 1) : 0.f;
		const float ry = (dh > 1) ? (float)(sh - 1) / (dh - 1) : 0.f;
		for (int y = 0; y < dh; y++) {
			auto fy = y * ry;
			auto y0 = (int)fy;
			auto y1 = std::min(y0 + 1, sh - 1);
			auto wy = fy - y0;
			for (int x = 0; x < dw; x++) {
				auto fx = x * rx;
				auto x0 = (int)fx;
				auto x1 = std::min(x0 + 1, sw - 1);
				auto wx = fx - x0;
				auto p00 = src + ((size_t)y0 * sw + x0) * 4;
				auto p01 = src + ((size_t)y0 * sw + x1) * 4;
				auto p10 = src + ((size_t)y1 * sw + x0) * 4;
				auto p11 = src + ((size_t)y1 * sw + x1) * 4;
				auto out = dst.data() + ((size_t)y * dw + x) * 4;
				for (int c = 0; c < 4; c++) {
					float v = p00[c] * (1 - wx) * (1 - wy) + p01[c] * wx * (1 - wy)
						+ p10[c] * (1 - wx) * wy + p11[c] * wx * wy;
					out[c] = (BYTE)std::clamp(v, 0.f, 255.f);
				}
			}
		}
	}
}

bool OcrEngine::ensureModel()
{
	if (loaded) return true;
	// det / rec 的 param+bin 与字典，来源按优先级：内嵌资源 -> exe 同目录 ocr\ 文件夹。
	// ncnn 的 load_param/load_model 支持从内存读，嵌资源时不用落盘
	auto loadPair = [](const wchar_t* stem, int resParam, int resBin, ncnn::Net& net) -> bool {
		auto param = loadResource(resParam);
		auto bin = loadResource(resBin);
		if (!param.empty() && !bin.empty()) {
			if (net.load_param(param.data()) != 0) return false;
			if (net.load_model(bin.data()) != 0) return false;
			return true;
		}
		auto paramPath = exeDir().append(L"ocr").append(std::wstring{ stem } + L".param");
		auto binPath = exeDir().append(L"ocr").append(std::wstring{ stem } + L".bin");
		std::error_code ec;
		if (!std::filesystem::exists(paramPath, ec) || !std::filesystem::exists(binPath, ec)) return false;
		if (net.load_param(paramPath.string().c_str()) != 0) return false;
		if (net.load_model(binPath.string().c_str()) != 0) return false;
		return true;
	};
	if (!loadPair(L"det", kResDetParam, kResDetBin, detNet)) return false;
	if (!loadPair(L"rec", kResRecParam, kResRecBin, recNet)) return false;
	detNet.opt.num_threads = kThreads;
	recNet.opt.num_threads = kThreads;

	auto dictData = loadResource(kResDict);
	if (dictData.empty()) {
		auto dictPath = exeDir().append(L"ocr").append(L"dict.txt");
		std::error_code ec;
		if (std::filesystem::exists(dictPath, ec)) {
			std::ifstream file{ dictPath, std::ios::binary };
			if (file) {
				std::string text{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
				dictData.assign(text.begin(), text.end());
			}
		}
	}
	dict = parseDict(dictData.data(), dictData.size());
	if (dict.empty()) return false; //没字典没法把 id 映射回字符
	loaded = true;
	return true;
}

// det：BGRA 全图 -> 文本框列表（源图像素坐标）。
// DBNet 流水线：缩放归一化 -> 概率图 -> 阈值二值化 -> 连通域 -> 外接矩形 -> unclip 外扩
std::vector<std::array<float, 4>> OcrEngine::detect(const BYTE* bgra, int w, int h)
{
	std::vector<std::array<float, 4>> boxes;
	// 输入尺寸：最长边压到 kDetLimitSide，再各自对齐 32（卷积下采样 5 次要能整除）
	auto scale = (std::min)(1.f, kDetLimitSide / (float)(std::max)(w, h));
	int nw = std::max(32, ((int)std::lround(w * scale) + 15) / 32 * 32);
	int nh = std::max(32, ((int)std::lround(h * scale) + 15) / 32 * 32);
	std::vector<BYTE> resized;
	resizeBilinear(bgra, w, h, resized, nw, nh);

	// 归一化 + CHW 排布（ImageNet mean/std，PP-OCR det 预处理的标准参数）
	ncnn::Mat in(nw, nh, 3);
	const float mean[3] = { 0.406f, 0.456f, 0.485f }; //按 BGR 排
	const float invStd[3] = { 1.f / 0.225f, 1.f / 0.224f, 1.f / 0.229f };
	for (int c = 0; c < 3; c++) {
		auto plane = in.channel(c);
		for (int y = 0; y < nh; y++) {
			auto src = resized.data() + ((size_t)y * nw) * 4 + c;
			for (int x = 0; x < nw; x++) {
				plane[x] = (src[x * 4] / 255.f - mean[c]) * invStd[c];
			}
		}
	}

	ncnn::Extractor ex = detNet.create_extractor();
	// 用 blob 序号而不是名字：onnx2ncnn 转出来的 blob 名不一定稳定，序号 0 是输入、最后一个必是输出
	if (ex.input(0, in) != 0) return boxes;
	ncnn::Mat out;
	if (ex.extract((int)detNet.blobs.size() - 1, out) != 0) return boxes;
	if (out.empty()) return boxes;

	// 概率图二值化
	std::vector<BYTE> binary((size_t)out.w * out.h, 0);
	for (int y = 0; y < out.h; y++) {
		auto row = out.row(y);
		for (int x = 0; x < out.w; x++) {
			if (row[x] >= kDetThresh) binary[(size_t)y * out.w + x] = 1;
		}
	}

	// 连通域（8 邻接，迭代栈式泛洪，避免递归爆栈）。顺便做一遍外接矩形
	std::vector<int> labels(binary.size(), -1);
	struct Comp { int minX, minY, maxX, maxY; };
	std::vector<Comp> comps;
	std::vector<int> stack;
	const int bw = out.w, bh = out.h;
	for (int start = 0; start < bw * bh; start++) {
		if (!binary[start] || labels[start] >= 0) continue;
		int id = (int)comps.size();
		Comp comp{ start % bw, start / bw, start % bw, start / bw };
		labels[start] = id;
		stack.push_back(start);
		while (!stack.empty()) {
			int cur = stack.back();
			stack.pop_back();
			int cx = cur % bw, cy = cur / bw;
			comp.minX = (std::min)(comp.minX, cx);
			comp.maxX = (std::max)(comp.maxX, cx);
			comp.minY = (std::min)(comp.minY, cy);
			comp.maxY = (std::max)(comp.maxY, cy);
			for (int dy = -1; dy <= 1; dy++) {
				for (int dx = -1; dx <= 1; dx++) {
					if (!dx && !dy) continue;
					int nx = cx + dx, ny = cy + dy;
					if (nx < 0 || ny < 0 || nx >= bw || ny >= bh) continue;
					auto idx = (size_t)ny * bw + nx;
					if (binary[idx] && labels[idx] < 0) {
						labels[idx] = id;
						stack.push_back((int)idx);
					}
				}
			}
		}
		comps.push_back(comp);
	}

	// 外接矩形映射回源图，再按 DB 的 unclip 公式外扩：
	// offset = area * ratio / perimeter，四条边各向外移 offset。
	// 原版用 Clipper 对多边形做偏移，截图场景文本几乎都是水平矩形，轴对齐近似就够了
	const float fx = (float)w / nw, fy = (float)h / nh;
	for (auto& comp : comps) {
		float x0 = comp.minX * fx, y0 = comp.minY * fy;
		float x1 = (comp.maxX + 1) * fx, y1 = (comp.maxY + 1) * fy;
		float rw = x1 - x0, rh = y1 - y0;
		if (rw < kDetMinSize || rh < kDetMinSize) continue;
		auto offset = rw * rh * kDetUnclipRatio / (2 * (rw + rh));
		x0 = (std::max)(0.f, x0 - offset);
		y0 = (std::max)(0.f, y0 - offset);
		x1 = (std::min)((float)w, x1 + offset);
		y1 = (std::min)((float)h, y1 + offset);
		if (x1 - x0 < 1 || y1 - y0 < 1) continue;
		boxes.push_back({ x0, y0, x1, y1 });
	}
	return boxes;
}

// rec：单个文本条 -> 文本 + 字符框（坐标在源图上）
std::wstring OcrEngine::recognizeLine(const BYTE* bgra, int w, int h,
	const std::array<float, 4>& box, std::vector<OcrChar>& chars)
{
	std::wstring text;
	// 检出框裁出来（轴对齐，clamp 到图内）
	int cx0 = std::clamp((int)box[0], 0, w - 1);
	int cy0 = std::clamp((int)box[1], 0, h - 1);
	int cx1 = std::clamp((int)std::ceil(box[2]), cx0 + 1, w);
	int cy1 = std::clamp((int)std::ceil(box[3]), cy0 + 1, h);
	int cw = cx1 - cx0, ch = cy1 - cy0;
	if (cw < 2 || ch < 2) return text;

	// 裁剪 -> 等比缩到 rec 输入高；宽高比超限就横向压扁
	auto ratio = cw / (float)ch;
	int rw = std::clamp((int)std::lround(kRecHeight * ratio), kRecMinWidth,
		(int)std::lround(kRecHeight * kRecMaxRatio));
	// resizeBilinear 要求源是行紧凑整图，先按行把裁剪块拷出来
	std::vector<BYTE> cropRect((size_t)cw * ch * 4);
	for (int y = 0; y < ch; y++) {
		memcpy(cropRect.data() + (size_t)y * cw * 4,
			bgra + ((size_t)(cy0 + y) * w + cx0) * 4, (size_t)cw * 4);
	}
	std::vector<BYTE> crop;
	resizeBilinear(cropRect.data(), cw, ch, crop, rw, kRecHeight);

	// 归一化到 [-1,1]（PP-OCR rec 预处理），CHW 排布
	ncnn::Mat in(rw, kRecHeight, 3);
	for (int c = 0; c < 3; c++) {
		auto plane = in.channel(c);
		for (int y = 0; y < kRecHeight; y++) {
			auto src = crop.data() + ((size_t)y * rw) * 4 + c;
			for (int x = 0; x < rw; x++) {
				plane[x] = src[x * 4] / 127.5f - 1.f;
			}
		}
	}

	ncnn::Extractor ex = recNet.create_extractor();
	if (ex.input(0, in) != 0) return text;
	ncnn::Mat out;
	if (ex.extract((int)recNet.blobs.size() - 1, out) != 0) return text;
	if (out.empty()) return text;

	// CTC 贪心解码：每列取 argmax，连续相同且非 blank（id 0）的列合并成一个字符。
	// 字典 id 从 1 起，dict.size()+1 是空格（use_space_char）。字符横向位置按列号映射回裁剪框
	const int T = out.h, C = out.w;
	int prevId{ 0 }, runStart{ 0 };
	auto flushChar = [&](int id, int t0, int t1) {
		std::wstring token;
		if (id == (int)dict.size() + 1) {
			token = L" ";
		}
		else if (id >= 1 && id <= (int)dict.size()) {
			token = dict[id - 1];
		}
		else {
			return;
		}
		text += token;
		// 列号 -> 裁剪框像素：字符框占整行高度，横向用列区间
		auto x0 = cx0 + (float)t0 / T * cw;
		auto x1 = cx0 + (float)(t1 + 1) / T * cw;
		chars.push_back({ x0, (float)cy0, x1, (float)cy1 });
	};
	for (int t = 0; t < T; t++) {
		auto row = out.row(t);
		int bestId{ 0 };
		float bestVal{ row[0] };
		for (int c = 1; c < C; c++) {
			if (row[c] > bestVal) {
				bestVal = row[c];
				bestId = c;
			}
		}
		if (bestId == prevId) continue;
		if (prevId != 0) flushChar(prevId, runStart, t - 1);
		prevId = bestId;
		runStart = t;
	}
	if (prevId != 0) flushChar(prevId, runStart, T - 1);
	return text;
}

std::vector<OcrLine> OcrEngine::recognize(const BYTE* bgra, int w, int h)
{
	std::vector<OcrLine> lines;
	if (w <= 0 || h <= 0 || !bgra) return lines;
	if (!ensureModel()) return lines;
	for (auto& box : detect(bgra, w, h)) {
		OcrLine line;
		std::vector<OcrChar> chars;
		line.text = recognizeLine(bgra, w, h, box, chars);
		if (line.text.empty()) continue;
		line.x0 = box[0]; line.y0 = box[1]; line.x1 = box[2]; line.y1 = box[3];
		line.chars = std::move(chars);
		lines.push_back(std::move(line));
	}
	// det 输出无序，按阅读序排：行中心先上下后左右
	std::sort(lines.begin(), lines.end(), [](const OcrLine& a, const OcrLine& b) {
		auto cy = [](const OcrLine& l) { return (l.y0 + l.y1) * 0.5f; };
		if (std::abs(cy(a) - cy(b)) > 0.5f) return cy(a) < cy(b);
		return a.x0 < b.x0;
	});
	return lines;
}

OcrWorker::~OcrWorker()
{
	stop();
}

void OcrWorker::start(ResultCallback callback)
{
	onResult = std::move(callback);
	{
		std::lock_guard lock(mtx);
		running = true;
	}
	thread = std::thread(&OcrWorker::loop, this);
}

void OcrWorker::stop()
{
	{
		std::lock_guard lock(mtx);
		running = false;
	}
	cv.notify_all();
	if (thread.joinable()) thread.join();
}

void OcrWorker::submit(UINT64 taskId, std::vector<BYTE>&& pixels, int w, int h)
{
	{
		std::lock_guard lock(mtx);
		// 只留最近一个任务：防抖重跑时旧的直接被顶掉，排队没有意义
		pending = Task{ taskId, std::move(pixels), w, h };
	}
	cv.notify_one();
}

void OcrWorker::loop()
{
	static OcrEngine engine; //模型加载一次，跨任务复用
	for (;;) {
		Task task;
		{
			std::unique_lock lock(mtx);
			cv.wait(lock, [this] { return pending || !running; });
			if (!pending) return; //running 已 false 且没有存货
			task = std::move(*pending);
			pending.reset();
		}
		auto lines = engine.recognize(task.pixels.data(), task.w, task.h);
		if (onResult) onResult(task.id, std::move(lines));
	}
}
