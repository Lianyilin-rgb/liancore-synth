# LianCore V3 - PDF 生成脚本
# 将 Markdown 用户手册转换为带水印的 PDF 文件
# 水印仅包含 "LianCore V3"，不包含任何人名

import os
import sys
import markdown
import pdfkit

# wkhtmltopdf 可执行文件路径
WKHTMLTOPDF_PATH = r"C:\Program Files\wkhtmltopdf\bin\wkhtmltopdf.exe"

# 项目根目录
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# 手册配置：语言 → (源文件路径, PDF输出路径)
MANUALS = {
    "zh-CN": (
        os.path.join(PROJECT_ROOT, "docs", "manuals", "zh-CN", "user-manual.md"),
        os.path.join(PROJECT_ROOT, "docs", "manuals", "zh-CN", "LianCoreV3_User_Manual_zh-CN.pdf"),
    ),
    "zh-TW": (
        os.path.join(PROJECT_ROOT, "docs", "manuals", "zh-TW", "user-manual.md"),
        os.path.join(PROJECT_ROOT, "docs", "manuals", "zh-TW", "LianCoreV3_User_Manual_zh-TW.pdf"),
    ),
    "en": (
        os.path.join(PROJECT_ROOT, "docs", "manuals", "en", "user-manual.md"),
        os.path.join(PROJECT_ROOT, "docs", "manuals", "en", "LianCoreV3_User_Manual_en.pdf"),
    ),
}

# 教程配置
TUTORIALS = {
    "zh-CN": (
        os.path.join(PROJECT_ROOT, "docs", "manuals", "zh-CN", "tutorial.md"),
        os.path.join(PROJECT_ROOT, "docs", "manuals", "zh-CN", "LianCoreV3_Tutorial_zh-CN.pdf"),
    ),
    "zh-TW": (
        os.path.join(PROJECT_ROOT, "docs", "manuals", "zh-TW", "tutorial.md"),
        os.path.join(PROJECT_ROOT, "docs", "manuals", "zh-TW", "LianCoreV3_Tutorial_zh-TW.pdf"),
    ),
    "en": (
        os.path.join(PROJECT_ROOT, "docs", "manuals", "en", "tutorial.md"),
        os.path.join(PROJECT_ROOT, "docs", "manuals", "en", "LianCoreV3_Tutorial_en.pdf"),
    ),
}

# HTML 模板 - 包含水印 CSS 和打印样式
HTML_TEMPLATE = """<!DOCTYPE html>
<html lang="{lang}">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>LianCore V3 用户手册</title>
<style>
  @page {{
    size: A4;
    margin: 20mm 18mm 20mm 18mm;
    @top-center {{
      content: "LianCore V3 - User Manual";
      font-size: 9px;
      color: #999;
      font-family: "Microsoft YaHei", "PingFang SC", "Noto Sans CJK SC", sans-serif;
    }}
    @bottom-center {{
      content: "Page " counter(page);
      font-size: 9px;
      color: #999;
    }}
  }}

  /* 水印样式 - 仅 "LianCore V3" */
  body {{
    font-family: "Microsoft YaHei", "PingFang SC", "Noto Sans CJK SC", "Segoe UI", sans-serif;
    font-size: 11pt;
    line-height: 1.7;
    color: #333;
    max-width: 100%;
    margin: 0;
    padding: 0;
    position: relative;
  }}

  /* 水印层 */
  body::before {{
    content: "LianCore V3";
    position: fixed;
    top: 50%;
    left: 50%;
    transform: translate(-50%, -50%) rotate(-45deg);
    font-size: 80px;
    color: rgba(180, 180, 180, 0.08);
    white-space: nowrap;
    pointer-events: none;
    z-index: 1000;
    font-family: "Microsoft YaHei", "PingFang SC", "Noto Sans CJK SC", sans-serif;
    font-weight: bold;
    letter-spacing: 10px;
  }}

  h1 {{
    font-size: 24pt;
    color: #1a1a2e;
    border-bottom: 3px solid #1a1a2e;
    padding-bottom: 10px;
    margin-top: 30px;
    page-break-after: avoid;
  }}

  h2 {{
    font-size: 18pt;
    color: #16213e;
    border-bottom: 2px solid #e0e0e0;
    padding-bottom: 6px;
    margin-top: 25px;
    page-break-after: avoid;
  }}

  h3 {{
    font-size: 14pt;
    color: #0f3460;
    margin-top: 20px;
    page-break-after: avoid;
  }}

  h4 {{
    font-size: 12pt;
    color: #444;
    margin-top: 15px;
  }}

  p {{
    margin: 8px 0;
  }}

  /* 链接样式 */
  a {{
    color: #0f3460;
    text-decoration: none;
  }}

  a:hover {{
    text-decoration: underline;
  }}

  /* 表格样式 */
  table {{
    width: 100%;
    border-collapse: collapse;
    margin: 12px 0;
    page-break-inside: avoid;
  }}

  th {{
    background-color: #1a1a2e;
    color: white;
    padding: 8px 12px;
    text-align: left;
    font-size: 10pt;
  }}

  td {{
    padding: 6px 12px;
    border: 1px solid #ddd;
    font-size: 10pt;
  }}

  tr:nth-child(even) {{
    background-color: #f5f5f5;
  }}

  /* 代码块 */
  code {{
    background-color: #f0f0f0;
    padding: 2px 6px;
    border-radius: 3px;
    font-family: "Consolas", "Courier New", monospace;
    font-size: 9pt;
    word-break: break-all;
  }}

  pre {{
    background-color: #f5f5f5;
    border: 1px solid #ddd;
    border-radius: 4px;
    padding: 12px;
    overflow-x: auto;
    page-break-inside: avoid;
  }}

  pre code {{
    background-color: transparent;
    padding: 0;
  }}

  /* 引用块 */
  blockquote {{
    border-left: 4px solid #0f3460;
    margin: 12px 0;
    padding: 8px 16px;
    background-color: #f8f9fa;
    color: #555;
  }}

  /* 水平线 */
  hr {{
    border: none;
    border-top: 1px solid #ddd;
    margin: 20px 0;
  }}

  /* 有序/无序列表 */
  ul, ol {{
    padding-left: 24px;
    margin: 8px 0;
  }}

  li {{
    margin: 4px 0;
  }}

  /* 强调 */
  strong {{
    color: #1a1a2e;
  }}

  /* 目录样式 */
  #toc {{
    page-break-after: always;
  }}

  /* 封面 */
  .cover {{
    text-align: center;
    padding-top: 150px;
    page-break-after: always;
  }}

  .cover h1 {{
    font-size: 36pt;
    border: none;
    margin-bottom: 10px;
  }}

  .cover .subtitle {{
    font-size: 16pt;
    color: #666;
    margin-bottom: 40px;
  }}

  .cover .version {{
    font-size: 12pt;
    color: #999;
  }}

  /* 打印优化 */
  @media print {{
    body {{
      font-size: 10pt;
    }}
    h1 {{ font-size: 20pt; }}
    h2 {{ font-size: 15pt; }}
    h3 {{ font-size: 12pt; }}
    table {{ page-break-inside: avoid; }}
    pre {{ page-break-inside: avoid; }}
  }}
</style>
</head>
<body>
<div class="cover">
  <h1>LianCore V3</h1>
  <div class="subtitle">User Manual / 用户手册</div>
  <div class="version">Version 3.0.0 | 2026-07</div>
</div>
{content}
</body>
</html>
"""


def convert_md_to_html(md_path: str, lang: str) -> str:
    """将 Markdown 文件转换为 HTML 内容"""
    with open(md_path, "r", encoding="utf-8") as f:
        md_content = f.read()

    # 使用 markdown 库转换为 HTML
    html_body = markdown.markdown(
        md_content,
        extensions=[
            "markdown.extensions.toc",           # 自动生成目录
            "markdown.extensions.tables",        # 表格支持
            "markdown.extensions.fenced_code",   # 代码块
            "markdown.extensions.codehilite",    # 代码高亮
            "markdown.extensions.nl2br",         # 换行支持
            "markdown.extensions.sane_lists",    # 列表解析
        ],
    )

    # 嵌入到完整 HTML 模板中
    full_html = HTML_TEMPLATE.format(content=html_body, lang=lang)
    return full_html


def generate_pdf(md_path: str, pdf_path: str, lang: str) -> bool:
    """生成带水印的 PDF 文件"""
    print(f"[{lang}] 正在转换: {os.path.basename(md_path)} → {os.path.basename(pdf_path)}")

    if not os.path.exists(md_path):
        print(f"  [错误] 源文件不存在: {md_path}")
        return False

    # 确保输出目录存在
    os.makedirs(os.path.dirname(pdf_path), exist_ok=True)

    # 转换为 HTML
    html_content = convert_md_to_html(md_path, lang)

    # PDF 选项
    options = {
        "page-size": "A4",
        "margin-top": "20mm",
        "margin-right": "18mm",
        "margin-bottom": "20mm",
        "margin-left": "18mm",
        "encoding": "UTF-8",
        "no-outline": None,
        "enable-local-file-access": None,
        "javascript-delay": "500",
        "no-stop-slow-scripts": None,
        "print-media-type": None,
        "no-background": None,
    }

    # 配置 pdfkit
    config = pdfkit.configuration(wkhtmltopdf=WKHTMLTOPDF_PATH)

    try:
        pdfkit.from_string(
            html_content,
            pdf_path,
            options=options,
            configuration=config,
        )

        # 验证文件大小
        size_mb = os.path.getsize(pdf_path) / (1024 * 1024)
        print(f"  [完成] {os.path.basename(pdf_path)} ({size_mb:.2f} MB)")
        return True

    except Exception as e:
        print(f"  [错误] PDF 生成失败: {e}")
        return False


def main():
    print("=" * 60)
    print("LianCore V3 - PDF 生成工具")
    print("=" * 60)

    if not os.path.exists(WKHTMLTOPDF_PATH):
        print(f"[错误] wkhtmltopdf 未找到: {WKHTMLTOPDF_PATH}")
        sys.exit(1)

    success_count = 0
    fail_count = 0

    print("\n--- 用户手册 ---")
    for lang, (md_path, pdf_path) in MANUALS.items():
        if generate_pdf(md_path, pdf_path, lang):
            success_count += 1
        else:
            fail_count += 1

    print("\n--- 教学手册 ---")
    for lang, (md_path, pdf_path) in TUTORIALS.items():
        if generate_pdf(md_path, pdf_path, lang):
            success_count += 1
        else:
            fail_count += 1

    print()
    print("=" * 60)
    print(f"生成完成: {success_count} 成功, {fail_count} 失败")
    print("=" * 60)

    return 0 if fail_count == 0 else 1


if __name__ == "__main__":
    sys.exit(main())