[简体中文](https://github.com/xland/ScreenCapture/) | English | [Русский](./ReadMe.ru-RU.md) | [Bahasa Indonesia](./ReadMe.id-ID.md)

![banner](./banner.png)

**ScreenCapture** A powerful and lightweight Windows screenshot tool.

## Features

- Screenshot, drawing annotations, scrolling screenshot (long screenshot), screen recording (GIF/MP4), text recognition (OCR), QR code recognition.
- Color picker, supports shortcut keys to copy RGB color (`Ctrl+R`), HEX color (`Ctrl+H`) and CMYK color (`Ctrl+K`).
- Draw ellipses, perfect circles (hold `Shift`), rectangles, squares (hold `Shift`), arrows, numbered labels, etc.
- Draw curves, straight lines (hold `Shift`), mosaic, eraser, text.
- Modify or delete drawn elements at any time (hover the mouse over an element).
- Undo (`Ctrl+Z`), redo (`Ctrl+Y`), save to file (`Ctrl+S`), save to clipboard (`Ctrl+C` or double-click).
- Fast performance with low memory usage.
- Small size, a single executable file, no installation required, does not depend on any dynamic link libraries.
- Supports a variety of command-line arguments for launching a specified function directly.
- Supports one-time execution mode (the process will not remain resident in the system).
- Multi-language support.

## Download

[Release](https://github.com/xland/ScreenCapture/releases/) (1MB)

## Supported Operating Systems

- Windows 10 1607 or Later

## Compilation

- The main branch depends on the [Ling](https://github.com/xland/Ling) GUI framework.
- The project can be compiled with Visual Studio 2026 (installed with the C++ Desktop Development Kit).
- OCR requires the [ncnn](https://github.com/Tencent/ncnn) static library (`-DNCNN_VULKAN=OFF -DNCNN_BUILD_TOOLS=ON`, built with the /MT runtime), placed at `D:\sdk\ncnn`; see `Tools/build_ocr_models.py` for model conversion.
- [2.4.25 (based on D2D)](https://github.com/xland/ScreenCapture/tree/2.4.25) and [2.3.3 (based on Qt)](https://github.com/xland/ScreenCapture/tree/2.3.3_qt) are the previous stable branches.

## Command Line

```
// Terminate the process immediately after the capture is finished.
> ScreenCapture.exe --auto-quit=true

// Skip the toolbar once the region is selected and go straight into the specified feature:
// long = scrolling capture (long screenshot)
> ScreenCapture.exe --enter=long
// video = screen recording
> ScreenCapture.exe --enter=video
// ocr = text recognition (enters the text-selection mode right after the region is selected)
> ScreenCapture.exe --enter=ocr
// qr = QR code recognition
> ScreenCapture.exe --enter=qr

// The two arguments can be combined, for example: no tray icon, and the process quits right after the long screenshot is taken.
> ScreenCapture.exe --enter=long --auto-quit=true
```

## Text Recognition (OCR)

Built-in offline OCR ([PP-OCRv6](https://github.com/PaddlePaddle/PaddleOCR) small models with static ncnn inference, supports Chinese, English and Japanese). No plugin download is needed anymore.

- OCR runs automatically in the background once the region is selected. Click the OCR icon on the toolbar to enter the text-selection mode: the cursor turns into an I-beam over recognized text, hold the left button and drag to select, then copy via `Ctrl+C` or the context menu. Press `ESC` or click the icon again to exit.
- The pin window supports it too: OCR starts automatically when a pin is created; click the OCR button on its toolbar to enter the text-selection mode.
- Model files are read from the `ocr` sub-directory next to ScreenCapture.exe (det.param/det.bin/rec.param/rec.bin/dict.txt). See `Tools/build_ocr_models.py` for the conversion script; models can also be embedded into the exe with the `OCR_EMBED_MODELS` preprocessor definition at build time.

## Sponsor

<table>
  <tr>
    <td align="center">
      <img alt="Alipay Sponsor" src="./alipay.jpg" width="160" height="160">
      <p>Alipay Sponsor</p>
    </td>
    <td align="center">
      <img alt="WeChat Sponsor" src="./wechat.png" width="160" height="160">
      <p>WeChat Sponsor</p>
    </td>
    <td align="center">
      <img alt="Author WeChat" src="./author.jpg" width="160" height="160">
      <p>Author WeChat</p>
    </td>
    <td align="center">
      <img alt="Official Account QR Code" src="./gongzhonghao.jpg" width="160" height="160">
      <p>WeChat Blog: Desktop Software</p>
    </td>
  </tr>
</table>
