# PP-OCRv6 集成与图片选字交互 — 工作总结

> 日期：2026-11-09　分支：`main`　提交：`8d9c01a feat: 集成PP-OCRv6并实现图片选字交互`
> 仓库：https://github.com/Samonsix/ScreenCapture（已推送，不涉及上游 xland/ScreenCapture，无 PR）

## 一、任务目标

将截图工具的 OCR 功能从第三方插件（ImageReader.exe，约 25MB，需单独下载）替换为内置的
PP-OCRv6 推理引擎，并实现微信"实况文本"式的图片选字交互：

- 截图/贴图时**后台自动执行 OCR**，完全静默（无 loading、无失败提示）
- 点击工具条 OCR 图标后**暂停拖动**，鼠标可直接在图片上**划选文字并复制**
- 支持中英日三种语言

## 二、关键调研结论

### 微信交互方案
- **实况文本（主流）**：OCR 后台自动完成，无显式按钮；光标悬停文字变 I 型竖线；
  按住左键直接划选，跨行跨块；Ctrl+C / 右键菜单复制；已知痛点是拖图与划选冲突
- 竞品参考：PixPin（贴图自动 OCR + 划选 + Shift+C 复制全部）、QQ（识别后右侧面板）

### 技术选型
- **PP-OCRv6**（2026-06 随 PaddleOCR 3.7.0 发布）small 档：det 9.6MB + rec 20.4MB，
  单模型支持 50 语种（含中英日；tiny 不含日文）
- **ncnn 全静态**编译进主 exe（BSD-3，无 DLL 依赖，保持单文件分发理念）；
  Paddle Inference 依赖 GB 级被否决，ONNX Runtime 需带 20MB DLL 被否决
- CPU 推理：截图类稀疏 UI 文字单帧约 100~400ms，后台线程无感

## 三、交互规格（与用户逐项确认）

| 决策点 | 结论 |
|---|---|
| 宿主窗口 | 留在截图窗口内，不弹独立窗口 |
| 视觉呈现 | 划选式（平时无高亮，悬停 I 型光标，选中显示半透明蓝底） |
| 复制方式 | Ctrl+C + 右键菜单（复制/全选）；不用浮动复制按钮 |
| 退出方式 | ESC 或再次点击 OCR 图标 → 回到正常截图状态 |
| 后台 OCR | 框选完成即跑；选区调整停止 500ms 防抖重跑；贴图创建时跑一次 |
| 模型档位 | PP-OCRv6 small（中英日） |
| 引擎分发 | ncnn 全静态；模型可从 exe 旁 `ocr\` 目录读，或 `OCR_EMBED_MODELS` 宏内嵌资源 |
| 旧插件 | ImageReader.exe 逻辑完全移除 |
| 复制后行为 | 短暂提示"已复制"，留在文字模式继续选取 |
| 识别反馈 | 完全静默 |
| 覆盖范围 | 截图窗口（WinCap）+ 钉图窗口（WinPin）都支持 |

## 四、实现清单

### 新增
| 文件 | 职责 |
|---|---|
| `Src/Ocr/Ocr.h / Ocr.cpp` | OCR 引擎：DB 文本检测后处理（阈值化+连通域+unclip，自实现无 OpenCV）、CTC 识别解码（含字符级 x 坐标）、单工作线程（只保留最新任务） |
| `Src/Win/OcrSelector.h / OcrSelector.cpp` | 共享交互组件：划选高亮、I 型光标判定、双击选整行、右键菜单（TrackPopupMenu 同步）、"已复制"轻提示 |
| `Tools/build_ocr_models.py` | 构建期转换脚本：Paddle→ONNX→ncnn，产出 det/rec 模型与中英日字典 |

### 修改
- `Src/Win/WinCap.cpp / .h`：框选完成后台识别 + 防抖重跑；文字模式切换；`--enter=ocr` 直通
- `Src/Win/WinPin.cpp / .h`：贴图自动识别；OCR 按钮；进入后窗口拖动/画笔暂停、双击改选整行
- `Src/Tool/ToolMain.h / .cpp`：钉图工具条新增 OCR 按钮
- `Src/Util.cpp / .h`、`Src/Setting.cpp`：移除 `openWithImageReader`/`findImageReader` 插件逻辑
- `Src/ScreenCapture.vcxproj`：新增文件与 ncnn 路径（D:\sdk\ncnn，/MT 静态）
- `Src/Res/Resource.rc`：模型 RCDATA 条目（`#ifdef OCR_EMBED_MODELS` 时生效）
- `Lang/*.json` 六种语言：新增 `tool.ocr` 与菜单/提示词条
- `ReadMe.md` 与多语言文档：移除插件下载引导

## 五、使用前提（Windows 构建机）

1. 编译 ncnn 静态库（cmake，/MT、NCNN_VULKAN=OFF），放到 `D:\sdk\ncnn`
2. 运行 `python Tools/build_ocr_models.py` 生成 ncnn 格式模型与字典，
   产物放 exe 同目录 `ocr\`（或加 `OCR_EMBED_MODELS` 宏内嵌资源）
3. 打开 `ScreenCapture.slnx` 编译

## 六、已知限制

- det 采用轴对齐矩形近似，倾斜文本精度略降
- 双击选中是整行而非单词（OCR 只有行文本，字符坐标仅横向）
- PP-OCRv6 若算子转换受阻，换 PP-OCRv5 mobile 模型即可（`Ocr.cpp` 顶部常量可调）
