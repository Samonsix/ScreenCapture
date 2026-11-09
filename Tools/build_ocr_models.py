# -*- coding: utf-8 -*-
"""
PP-OCRv6 -> ncnn 模型转换脚本（构建期一次性工具，产物是平台无关的 ncnn 格式）

本项目的 OCR 引擎（Src/Ocr/Ocr.cpp）跑在 ncnn 上，吃三样东西：
    det.param / det.bin   PP-OCRv6 small 文本检测模型
    rec.param / rec.bin   PP-OCRv6 small 文本识别模型（中英日多语种）
    dict.txt              rec 模型的字典（UTF-8，一行一个字符）

转换链路：Paddle 推理模型 --paddle2onnx--> ONNX --onnxsim--> 简化 ONNX
         --onnx2ncnn--> ncnn 的 .param/.bin

前置准备：
1. 下载官方推理模型（解压后目录里是 inference.pdmodel / inference.pdiparams）：
   - PP-OCRv6 small det/rec：见 PaddleOCR 文档模型列表
     https://www.paddleocr.ai/main/version3.x/module_usage/text_detection.html
     https://www.paddleocr.ai/main/version3.x/module_usage/text_recognition.html
2. 下载 rec 模型对应的字典（PaddleOCR 仓库 ppocr/utils/dict/ 下与 v6 配套的那份），
   中英日走多语种字典；拿不准时用 paddleocr Python 包打印
   `from paddleocr import TextRecognition; m = TextRecognition("PP-OCRv6_small_rec")` 的模型目录，
   里面会有匹配的字典文件。
3. 装 Python 依赖：pip install "paddle2onnx==1.2.4" onnx onnxsim
4. 编一份带转换工具的 ncnn（MSVC、静态、/MT，与主工程一致）：
     cmake -B build -A x64 -DNCNN_VULKAN=OFF -DNCNN_BUILD_TOOLS=ON -DNCNN_BUILD_EXAMPLES=OFF
     cmake --build build --config Release --target ncnn onnx2ncnn
   然后把 include/ src/ lib/ 放到 D:\\sdk\\ncnn（工程已经按这个路径引了 ncnn.lib），
   tools/onnx/onnx2ncnn.exe 的位置通过 --ncnn-tools 传进来。
   编译时建议给 ncnn 的 CMake 加 -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded，否则
   ncnn.lib 会拖 /MD 运行时进来，和主工程的 /MT 打架。

用法示例：
    python Tools/build_ocr_models.py ^
        --det-dir D:\\models\\PP-OCRv6_small_det_infer ^
        --rec-dir D:\\models\\PP-OCRv6_small_rec_infer ^
        --dict   D:\\models\\dict.txt ^
        --ncnn-tools D:\\sdk\\ncnn-build\\tools\\onnx

产物默认写到 Src/Res/ocr/：
- 开发调试：把整个 ocr 文件夹拷到 exe 同目录，运行时直接读文件；
- 随包分发：在 ResourceCompile 的 PreprocessorDefinitions 里加 OCR_EMBED_MODELS，
  Resource.rc 会把这几个文件作为 RCDATA 编进 exe（Ocr.cpp 优先读内嵌资源）。
"""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path


def run(cmd: list) -> None:
    print("+ " + " ".join(str(c) for c in cmd))
    result = subprocess.run(cmd, shell=True)
    if result.returncode != 0:
        sys.exit(f"命令失败（exit {result.returncode}）：{' '.join(str(c) for c in cmd)}")


def check_tool(name: str) -> None:
    if shutil.which(name) is None:
        sys.exit(f"找不到 {name}，请先安装（见脚本头部的前置准备）")


def infer_model_files(model_dir: Path) -> tuple:
    """Paddle 推理模型的文件名有两代习惯，都容一下"""
    for stem in ("inference", "model"):
        pdmodel = model_dir / f"{stem}.pdmodel"
        pdiparams = model_dir / f"{stem}.pdiparams"
        if pdmodel.exists() and pdiparams.exists():
            return pdmodel, pdiparams
    sys.exit(f"{model_dir} 里没找到 inference.pdmodel / inference.pdiparams")


def main() -> None:
    parser = argparse.ArgumentParser(description="PP-OCRv6 -> ncnn 模型转换")
    parser.add_argument("--det-dir", required=True, help="det 推理模型目录")
    parser.add_argument("--rec-dir", required=True, help="rec 推理模型目录")
    parser.add_argument("--dict", required=True, help="rec 字典文件（UTF-8，一行一个字符）")
    parser.add_argument("--ncnn-tools", required=True,
                        help="ncnn 构建产物里 tools/onnx 目录（内含 onnx2ncnn.exe）")
    parser.add_argument("--output-dir", default=str(Path(__file__).resolve().parent.parent / "Src" / "Res" / "ocr"),
                        help="产物目录，默认 Src/Res/ocr")
    parser.add_argument("--opset", type=int, default=12, help="paddle2onnx 导出的 opset（默认 12）")
    args = parser.parse_args()

    check_tool("paddle2onnx")
    onnx2ncnn = Path(args.ncnn_tools) / "onnx2ncnn.exe"
    if not onnx2ncnn.exists():
        sys.exit(f"找不到 {onnx2ncnn}，先用 NCNN_BUILD_TOOLS=ON 编一份 ncnn")
    dict_file = Path(args.dict)
    if not dict_file.exists():
        sys.exit(f"找不到字典文件 {dict_file}")

    out_dir = Path(args.output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    tmp_dir = out_dir / "_tmp"
    tmp_dir.mkdir(exist_ok=True)

    for name, model_dir in (("det", Path(args.det_dir)), ("rec", Path(args.rec_dir))):
        pdmodel, pdiparams = infer_model_files(model_dir)
        onnx_file = tmp_dir / f"{name}.onnx"
        run(["paddle2onnx",
             "--model_dir", model_dir,
             "--model_filename", pdmodel.name,
             "--params_filename", pdiparams.name,
             "--save_file", onnx_file,
             "--opset_version", args.opset])
        # 简化 ONNX 能显著提高 onnx2ncnn 的成功率（折起常量、去掉冗余节点）
        sim_file = tmp_dir / f"{name}.sim.onnx"
        run([sys.executable, "-m", "onnxsim", onnx_file, sim_file])
        run([onnx2ncnn, sim_file, out_dir / f"{name}.param", out_dir / f"{name}.bin"])

    shutil.copyfile(dict_file, out_dir / "dict.txt")
    shutil.rmtree(tmp_dir, ignore_errors=True)

    sizes = {f.name: f.stat().st_size for f in sorted(out_dir.iterdir())}
    print("\n转换完成，产物：")
    for name, size in sizes.items():
        print(f"  {name}  {size / 1024:.0f} KB")
    print("\n下一步：")
    print("  开发调试：把 ocr 文件夹整个拷到 ScreenCapture.exe 同目录")
    print("  随包分发：vcxproj 的 ResourceCompile 加 PreprocessorDefinitions OCR_EMBED_MODELS")


if __name__ == "__main__":
    main()
