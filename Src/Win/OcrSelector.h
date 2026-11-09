#pragma once
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <vector>
#include "../Ocr/Ocr.h"

// 截图窗口（WinCap）与钉图窗口（WinPin）共用的"文字选取模式"交互逻辑。
// 微信实况文本的路子：平时不高亮，光标悬停到文字上变 I 型，按住左键划选，
// 与识别文字框相交的行整行高亮，Ctrl+C / 右键菜单复制。
// 本类不持有窗口：所有事件由宿主窗口转进来，坐标一律是图像坐标
//（WinCap 是选区内坐标，WinPin 是底图像素坐标），绘制时的平移/缩放由宿主管。
class OcrSelector
{
public:
	// 收到新识别结果时整组替换，选区一并清掉（结果换了一批，旧选中没有意义）
	void setLines(std::vector<OcrLine> value);
	void clearSelection();
	bool hasSelection() const { return !selected.empty(); }
	// 还没有任何识别结果（宿主据此决定要不要在进入模式时补跑一轮）
	bool empty() const { return lines.empty(); }
	// 选中区域的左上角（图像坐标），"已复制"提示锚在这个位置附近
	POINT selectionTopLeft() const;
	// 下面的事件里 pos 一律是图像坐标
	void onDown(const POINT& pos);
	// 返回选中集有没有变化（变了宿主才需要重绘）
	bool onMove(const POINT& pos);
	void onUp(const POINT& pos);
	// 双击：选中光标所在的整行（OCR 只有行文本与字符横向区间，词边界做不准，整行最稳）
	void dblClick(const POINT& pos);
	void selectAll();
	// 选中文字按阅读序用换行拼起来进剪切板，有选中才返回 true
	bool copy();
	// 光标悬停处有没有文字（宿主据此把光标换成 I 型）
	bool hoverText(const POINT& pos) const;
	// 画选中行的高亮底色。offX/offY 是图像坐标到绘制坐标的平移量，
	// 缩放变换由宿主自己管（WinPin 带缩放、WinCap 原样平移）
	void paint(ID2D1DeviceContext* ctx, float offX, float offY);
	// "已复制"轻提示，显示约 900ms。要在窗口坐标下画（宿主先把缩放变换收回来），
	// anchor 是提示框左上角。返回 false 表示提示已过期（宿主可以顺手关掉定时器）
	bool paintCopiedTip(ID2D1DeviceContext* ctx, float anchorX, float anchorY, float dpi);
	// 复制成功后调用：让下一次 paint 画出提示，宿主据此开个定时器到点重绘
	void showCopiedTip();
	// "已复制"提示当前是否在显示期
	bool copiedTipVisible() const;
	// 右键菜单（复制 / 全选）。做了 TrackPopupMenu 的经典前台激活处理，
	// 否则菜单点完不会自动消失。返回交互有没有改变选中集（宿主据此重绘）
	bool showContextMenu(HWND owner, const POINT& screenPos);
private:
	void updateSelection();
	void ensureBrushes(ID2D1DeviceContext* ctx);
private:
	// 识别结果，按阅读序（先上后下、同行先左后右，引擎里已排好）
	std::vector<OcrLine> lines;
	// 选中行在 lines 里的下标
	std::vector<size_t> selected;
	// 划选中：按下点与当前点，落选区矩形由两点归一化而来
	bool selecting{ false };
	POINT startPt{ 0, 0 }, endPt{ 0, 0 };
	// "已复制"提示显示到的时刻（GetTickCount64 时刻制）
	ULONGLONG copiedTick{ 0 };
	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brushHi;
	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brushTipBg;
	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brushTipText;
};
