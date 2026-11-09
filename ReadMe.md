简体中文 | [English](./Doc/ReadMe.en-US.md) | [Русский](./Doc/ReadMe.ru-RU.md) | [Bahasa Indonesia](./Doc/ReadMe.id-ID.md)

![banner](./Doc/banner.png)

**ScreenCapture** 一个小巧但功能强大的Windows截图工具。

## 特性

- 截图、绘图标注、滚动截图（截长图）、录屏（GIF/MP4）、文字识别（OCR）、二维码识别。
- 取景框（拾色器），支持快捷键复制 RGB 颜色（`Ctrl+R`）、 HEX 颜色（`Ctrl+H`）与 CMYK 颜色（`Ctrl+K`）。
- 绘制椭圆、正圆（按住`Shift`）、矩形、正方形（按住`Shift`）、箭头、标号等。
- 绘制曲线、直线（按住`Shift`）、马赛克、橡皮擦、文本。
- 可以随时修改、删除已绘制的元素（鼠标移到元素上）。
- 撤销（`Ctrl+Z`）、重做（`Ctrl+Y`）、保存为文件（`Ctrl+S`）、保存到剪贴板（`Ctrl+C`或双击）。
- 运行速度快、内存占用低。
- 体积小、仅一个可执行文件，无需安装，不依赖任何动态链接库。
- 支持多种命令行参数直接启动指定的功能。
- 支持用完即走（进程不驻留在系统中）。
- 多语言支持。

## 下载

[Release](https://github.com/xland/ScreenCapture/releases/) （1MB）

## 常用功能与问题

- 按住 `Ctrl键` 框选截图区域后，直接进入图像标记窗口（钉图窗口）
- 按住 `Ctrl键` 滚动鼠标滚轮可以放大、缩小图像标记窗口（钉图窗口）
- 长截图拼接不符合预期时，尝试调整截图区域往往能解决问题
- 如手动下载新版本，则必须退出老版本再启动新版本

## 支持的操作系统

- Windows 10 1803 or Later

## 编译

- main分支依赖 [Ling](https://github.com/xland/Ling) GUI 框架.
- 使用 Visual Studio 2026（With C++ Desktop Dev Kit）即可编译项目。
- OCR 需要 [ncnn](https://github.com/Tencent/ncnn) 静态库（`-DNCNN_VULKAN=OFF -DNCNN_BUILD_TOOLS=ON`，/MT 运行时），放到 `D:\sdk\ncnn`；模型转换见 `Tools/build_ocr_models.py`。
- [2.4.25（基于D2D）](https://github.com/xland/ScreenCapture/tree/2.4.25)或 [2.3.3（基于Qt）](https://github.com/xland/ScreenCapture/tree/2.3.3_qt)是以前的稳定分支。

## 命令行

```
// 截图完成后即退出进程。
> ScreenCapture.exe --auto-quit=true

// 框选完成后不显示工具条，直接进入指定功能：
// pin 钉图/图像标记
> ScreenCapture.exe --enter=pin
// long 长截图
> ScreenCapture.exe --enter=long
// video 屏幕录制
> ScreenCapture.exe --enter=video
// ocr 文字识别（框选完成后点工具条上的 OCR 图标，或直接进入文字选取模式）
> ScreenCapture.exe --enter=ocr
// qr 二维码识别
> ScreenCapture.exe --enter=qr
// tray 仅注册托盘图标，不执行任何操作
> ScreenCapture.exe --enter=tray

// 两个参数可以联合使用，比如：不注册托盘图标，截完长图后进程直接退出
> ScreenCapture.exe --enter=long --auto-quit=true
```

## 文字识别

内置离线 OCR（[PP-OCRv6](https://github.com/PaddlePaddle/PaddleOCR) small 模型 + ncnn 静态推理，支持中、英、日），无需下载任何插件。

- 框选完成后 OCR 在后台自动识别；点工具条上的 OCR 图标进入文字选取模式，鼠标在文字上变 I 型光标，按住左键划选即可复制（`Ctrl+C` 或右键菜单），`ESC` 或再次点击图标退出。
- 贴图窗口同样支持：贴图创建即自动识别，点工具条上的 OCR 按钮进入文字选取模式。
- 模型文件默认从 `ScreenCapture.exe` 同目录的 `ocr` 子目录读取（det.param/det.bin/rec.param/rec.bin/dict.txt）；模型转换脚本见 `Tools/build_ocr_models.py`，也可以在构建时以 `OCR_EMBED_MODELS` 宏把模型内嵌进 exe。

## 便携能力

默认情况下 ScreenCapture 会从 `%appdata%\ScreenCapture\Lang` 目录下读取配置信息及语言文件。

但用户可以在 `ScreenCapture.exe` 同目录下创建一个 `config.json` 的空文件，重启应用，`ScreenCapture` 即会在此文件中设置配置信息。

`ScreenCapture.exe` 同目录下创建一个 `Lang` 子目录，然后把语言文件放置到此目录下，`ScreenCapture` 即会读取此目录下的语言文件。

OCR 模型也可以放置在 `ScreenCapture.exe` 同目录下的 `ocr` 子目录中

## 赞助

<table>
  <tr>
    <td align="center">
      <img alt="支付宝赞助" src="./Doc/alipay.jpg" width="160" height="160">
      <p>支付宝赞助</p>
    </td>
    <td align="center">
      <img alt="微信赞助" src="./Doc/wechat.png" width="160" height="160">
      <p>微信赞助</p>
    </td>
    <td align="center">
      <img alt="作者微信" src="./Doc/author.jpg" width="160" height="160">
      <p>作者微信</p>
    </td>
    <td align="center">
      <img alt="公众号二维码" src="./Doc/gongzhonghao.jpg" width="160" height="160">
      <p>公众号：桌面软件</p>
    </td>
  </tr>
</table>