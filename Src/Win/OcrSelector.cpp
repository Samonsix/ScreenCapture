#include "pch.h"
#include "OcrSelector.h"
#include "../Lang.h"
#include <include/Ling.h>
#include <algorithm>

namespace
{
	// 右键菜单项 id
	constexpr UINT kMenuCopy{ 1 };
	constexpr UINT kMenuSelectAll{ 2 };
	// "已复制"提示停留时长
	constexpr ULONGLONG kCopiedTipMs{ 900 };

	// 词条缺失（老语言包没翻到）时给兜底文案，不至于出空白菜单
	std::wstring langOr(const wchar_t* key, const wchar_t* fallback)
	{
		auto value = Lang::get(key);
		return value.empty() ? std::wstring{ fallback } : value;
	}
}

void OcrSelector::setLines(std::vector<OcrLine> value)
{
	lines = std::move(value);
	clearSelection();
}

void OcrSelector::clearSelection()
{
	selected.clear();
	selecting = false;
}

POINT OcrSelector::selectionTopLeft() const
{
	POINT pt{ 0, 0 };
	if (selected.empty()) return pt;
	bool first{ true };
	for (auto idx : selected) {
		auto& l = lines[idx];
		if (first || l.y0 < pt.y || (l.y0 == pt.y && l.x0 < pt.x)) {
			pt = POINT{ (LONG)std::lround(l.x0), (LONG)std::lround(l.y0) };
			first = false;
		}
	}
	return pt;
}

void OcrSelector::ensureBrushes(ID2D1DeviceContext* ctx)
{
	if (brushHi) return;
	auto d2d = Ling::D2D::get();
	d2d->deviceContext->CreateSolidColorBrush(D2D1::ColorF(0x2E6BFF, 0.35f), brushHi.GetAddressOf());
	d2d->deviceContext->CreateSolidColorBrush(D2D1::ColorF(0x000000, 0.46f), brushTipBg.GetAddressOf());
	d2d->deviceContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), brushTipText.GetAddressOf());
}

void OcrSelector::onDown(const POINT& pos)
{
	selecting = true;
	startPt = pos;
	endPt = pos;
	updateSelection();
}

bool OcrSelector::onMove(const POINT& pos)
{
	if (!selecting) return false;
	endPt = pos;
	auto before = selected;
	updateSelection();
	return before != selected;
}

void OcrSelector::onUp(const POINT& pos)
{
	if (!selecting) return;
	selecting = false;
	endPt = pos;
	updateSelection();
}

void OcrSelector::updateSelection()
{
	selected.clear();
	// 划选矩形由按下/当前两点归一化而来；与文字行矩形有非空相交就算选中
	auto left = (std::min)(startPt.x, endPt.x);
	auto right = (std::max)(startPt.x, endPt.x);
	auto top = (std::min)(startPt.y, endPt.y);
	auto bottom = (std::max)(startPt.y, endPt.y);
	for (size_t i = 0; i < lines.size(); i++) {
		auto& l = lines[i];
		if (l.x0 < right && l.x1 > left && l.y0 < bottom && l.y1 > top) {
			selected.push_back(i);
		}
	}
}

void OcrSelector::dblClick(const POINT& pos)
{
	selected.clear();
	for (size_t i = 0; i < lines.size(); i++) {
		auto& l = lines[i];
		if (pos.x >= l.x0 && pos.x <= l.x1 && pos.y >= l.y0 && pos.y <= l.y1) {
			selected.push_back(i);
			break;
		}
	}
}

void OcrSelector::selectAll()
{
	selected.resize(lines.size());
	for (size_t i = 0; i < lines.size(); i++) selected[i] = i;
}

bool OcrSelector::copy()
{
	if (selected.empty()) return false;
	std::wstring text;
	// lines 本身就是阅读序，按下标顺序拼即可；跨行用换行，贴近文本编辑器的复制结果
	std::sort(selected.begin(), selected.end());
	for (auto idx : selected) {
		if (!text.empty()) text += L'\n';
		text += lines[idx].text;
	}
	Ling::Util::setTextToClipboard(text);
	return true;
}

bool OcrSelector::hoverText(const POINT& pos) const
{
	for (auto& l : lines) {
		if (pos.x >= l.x0 && pos.x <= l.x1 && pos.y >= l.y0 && pos.y <= l.y1) return true;
	}
	return false;
}

void OcrSelector::paint(ID2D1DeviceContext* ctx, float offX, float offY)
{
	if (selected.empty()) return;
	ensureBrushes(ctx);
	for (auto idx : selected) {
		auto& l = lines[idx];
		ctx->FillRectangle(D2D1::RectF(l.x0 + offX, l.y0 + offY, l.x1 + offX, l.y1 + offY), brushHi.Get());
	}
}

void OcrSelector::showCopiedTip()
{
	copiedTick = GetTickCount64() + kCopiedTipMs;
}

bool OcrSelector::copiedTipVisible() const
{
	return GetTickCount64() < copiedTick;
}

bool OcrSelector::paintCopiedTip(ID2D1DeviceContext* ctx, float anchorX, float anchorY, float dpi)
{
	if (!copiedTipVisible()) return false;
	auto text = langOr(L"ocr.copied", L"已复制");
	auto layout = Ling::D2D::get()->makeTextLayout(text, 11.f * dpi);
	DWRITE_TEXT_METRICS tm{};
	if (FAILED(layout->GetMetrics(&tm))) return false;
	ensureBrushes(ctx);
	auto pad = 3.f * dpi;
	D2D1_RECT_F bg{ anchorX, anchorY, anchorX + tm.width + pad * 2, anchorY + tm.height + pad * 2 };
	ctx->FillRectangle(bg, brushTipBg.Get());
	ctx->DrawTextLayout({ bg.left + pad, bg.top + pad }, layout.Get(), brushTipText.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
	return true;
}

bool OcrSelector::showContextMenu(HWND owner, const POINT& screenPos)
{
	HMENU menu = CreatePopupMenu();
	if (!menu) return false;
	AppendMenuW(menu, MF_STRING | (hasSelection() ? MF_ENABLED : MF_GRAYED),
		kMenuCopy, langOr(L"ocr.copy", L"复制").c_str());
	AppendMenuW(menu, MF_STRING, kMenuSelectAll, langOr(L"ocr.selectAll", L"全选").c_str());
	// TrackPopupMenu 的经典坑：菜单所属窗口不在前台时，菜单点完不会自己消失。
	// 先把宿主提上前台，选完补一个 WM_NULL 把消息队列刷干净
	SetForegroundWindow(owner);
	auto cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
		screenPos.x, screenPos.y, 0, owner, nullptr);
	DestroyMenu(menu);
	PostMessageW(owner, WM_NULL, 0, 0);
	bool changed = false;
	if (cmd == kMenuCopy) {
		if (copy()) showCopiedTip();
	}
	else if (cmd == kMenuSelectAll) {
		selectAll();
		changed = true;
	}
	return changed;
}
