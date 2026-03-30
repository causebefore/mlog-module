#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
mlog_scan.py - MLog 日志调用静态扫描工具

扫描项目源码中的所有 mlog 日志调用，提取结构化信息并生成报告。

功能:
  - 提取所有日志宏调用 (mlog_a/e/w/i/d/v, log_a/e/w/i/d/v, mlog_raw, MLOG_ASSERT, mlog_hexdump)
  - 解析每条日志的：文件路径、行号、级别、tag、格式字符串
  - 检测 LOG_TAG 未定义的文件
  - 检测被 MLOG_OUTPUT_LVL / LOG_LVL 编译期裁掉的日志
  - 简单的 printf 格式字符串参数数量校验
  - 按级别/tag 分组统计
  - 支持 table / csv / json 输出格式

用法:
  python mlog_scan.py <项目路径> [选项]
  python mlog_scan.py .                     # 扫描当前目录
  python mlog_scan.py . --level ERROR       # 只显示 >= ERROR
  python mlog_scan.py . --tag sensor        # 只显示指定 tag
  python mlog_scan.py . --format csv        # 输出 CSV
  python mlog_scan.py . --check             # 启用格式字符串校验 + LOG_TAG 检查
  python mlog_scan.py . --show-trimmed      # 显示被编译期裁掉的日志
  python mlog_scan.py . -o report.csv       # 输出到文件
"""

import argparse
import csv
import io
import json
import os
import re
import sys
from collections import defaultdict
from dataclasses import dataclass, asdict, field
from pathlib import Path
from typing import List, Optional, Dict

# ---------------------------------------------------------------------------
# 常量
# ---------------------------------------------------------------------------

LEVEL_NAMES = ["ASSERT", "ERROR", "WARN", "INFO", "DEBUG", "VERBOSE"]
LEVEL_MAP = {name: idx for idx, name in enumerate(LEVEL_NAMES)}

# 带显式 tag 的宏 -> 级别名
EXPLICIT_TAG_MACROS = {
    "mlog_assert": "ASSERT", "mlog_a": "ASSERT",
    "mlog_error":  "ERROR",  "mlog_e": "ERROR",
    "mlog_warn":   "WARN",   "mlog_w": "WARN",
    "mlog_info":   "INFO",   "mlog_i": "INFO",
    "mlog_debug":  "DEBUG",  "mlog_d": "DEBUG",
    "mlog_verbose": "VERBOSE", "mlog_v": "VERBOSE",
}

# 使用 LOG_TAG 的宏 -> 级别名
IMPLICIT_TAG_MACROS = {
    "log_a": "ASSERT", "loga": "ASSERT",
    "log_e": "ERROR",  "loge": "ERROR",
    "log_w": "WARN",   "logw": "WARN",
    "log_i": "INFO",   "logi": "INFO",
    "log_d": "DEBUG",  "logd": "DEBUG",
    "log_v": "VERBOSE", "logv": "VERBOSE",
}

# 特殊宏
SPECIAL_MACROS = {"mlog_raw", "MLOG_ASSERT", "mlog_hexdump"}

# 扫描的文件扩展名
SOURCE_EXTENSIONS = {".c", ".h", ".cpp", ".cc", ".cxx", ".hpp"}

# printf 格式说明符 (简化匹配)
# 匹配 %[-+ #0]*[宽度][.精度][长度修饰符][转换字符]
# 排除 %% (转义的百分号)
FMT_SPEC_RE = re.compile(
    r'%(?!%)[-+ #0]*(?:\*|\d+)?(?:\.(?:\*|\d+))?(?:hh|h|ll|l|j|z|t|L)?[diouxXeEfFgGaAcspn%]'
)


# ---------------------------------------------------------------------------
# 数据结构
# ---------------------------------------------------------------------------

@dataclass
class LogEntry:
    """单条日志调用"""
    file: str           # 相对路径
    line: int           # 行号
    level: str          # 级别名 ("ASSERT".."VERBOSE", "RAW", "HEXDUMP", "ASSERT_CHK")
    tag: str            # tag 值
    fmt_string: str     # 格式字符串 (提取到的原始文本)
    macro: str          # 使用的宏名
    is_trimmed: bool = False       # 是否被编译期裁掉
    trimmed_by: str = ""           # 被什么配置裁掉 ("MLOG_OUTPUT_LVL" / "LOG_LVL")
    tag_from_log_tag: bool = False # tag 来自 LOG_TAG 宏
    raw_call: str = ""             # 原始调用文本 (截断到合理长度)


@dataclass
class FileInfo:
    """文件级信息"""
    path: str
    log_tag: Optional[str] = None       # #define LOG_TAG 的值
    log_lvl: Optional[str] = None       # #define LOG_LVL 的值
    log_tag_defined: bool = False
    log_lvl_defined: bool = False
    entries: List[LogEntry] = field(default_factory=list)


@dataclass
class CheckWarning:
    """校验警告"""
    file: str
    line: int
    macro: str
    category: str   # "NO_LOG_TAG" / "FMT_MISMATCH" / "TRIMMED"
    message: str


# ---------------------------------------------------------------------------
# 核心解析
# ---------------------------------------------------------------------------

def extract_balanced_parens(text: str, start: int) -> Optional[str]:
    """从 start 位置的 '(' 开始提取到匹配的 ')' 之间的内容(含括号)。
    处理嵌套括号和字符串字面量。"""
    if start >= len(text) or text[start] != '(':
        return None

    depth = 0
    in_string = False
    string_char = None
    i = start
    while i < len(text):
        ch = text[i]
        if in_string:
            if ch == '\\':
                i += 1  # 跳过转义字符
            elif ch == string_char:
                in_string = False
        else:
            if ch == '"' or ch == "'":
                in_string = True
                string_char = ch
            elif ch == '(':
                depth += 1
            elif ch == ')':
                depth -= 1
                if depth == 0:
                    return text[start:i + 1]
        i += 1
    return None


def split_args(args_str: str) -> List[str]:
    """按顶层逗号拆分参数列表，处理嵌套括号和字符串。"""
    args = []
    depth = 0
    in_string = False
    string_char = None
    current = []

    for i, ch in enumerate(args_str):
        if in_string:
            current.append(ch)
            if ch == '\\':
                if i + 1 < len(args_str):
                    current.append(args_str[i + 1])
                continue
            if ch == string_char:
                in_string = False
        else:
            if ch == '"' or ch == "'":
                in_string = True
                string_char = ch
                current.append(ch)
            elif ch == '(' or ch == '[' or ch == '{':
                depth += 1
                current.append(ch)
            elif ch == ')' or ch == ']' or ch == '}':
                depth -= 1
                current.append(ch)
            elif ch == ',' and depth == 0:
                args.append(''.join(current).strip())
                current = []
            else:
                current.append(ch)

    if current:
        args.append(''.join(current).strip())
    return args


def extract_string_literal(token: str) -> Optional[str]:
    """从 token 中提取第一个字符串字面量的内容。"""
    m = re.search(r'"((?:[^"\\]|\\.)*)"', token)
    if m:
        return m.group(1)
    return None


def count_fmt_specifiers(fmt_str: str) -> int:
    """统计格式字符串中的参数占位符数量（排除 %% 和 %n）。"""
    specs = FMT_SPEC_RE.findall(fmt_str)
    count = 0
    for s in specs:
        if s == '%%':
            continue
        if s.endswith('n'):
            continue
        # %* 或 %.* 每个 * 额外消耗一个参数
        count += 1
        count += s.count('*')
    return count


def join_continuation_lines(lines: List[str]) -> str:
    """将行尾续行符 \\ 的行合并为逻辑行。"""
    result = []
    continued = []
    for line in lines:
        stripped = line.rstrip()
        if stripped.endswith('\\'):
            continued.append(stripped[:-1])
        else:
            if continued:
                continued.append(line)
                result.append(' '.join(continued))
                continued = []
            else:
                result.append(line)
    if continued:
        result.append(' '.join(continued))
    return '\n'.join(result)


# ---------------------------------------------------------------------------
# 文件扫描
# ---------------------------------------------------------------------------

def parse_global_config(project_path: str) -> int:
    """从 mlog_cfg.h 解析 MLOG_OUTPUT_LVL，返回级别值。"""
    cfg_candidates = [
        os.path.join(project_path, "inc", "mlog_cfg.h"),
        os.path.join(project_path, "include", "mlog_cfg.h"),
        os.path.join(project_path, "mlog_cfg.h"),
    ]
    for cfg_path in cfg_candidates:
        if os.path.isfile(cfg_path):
            try:
                with open(cfg_path, 'r', encoding='utf-8', errors='replace') as f:
                    content = f.read()
            except OSError:
                continue
            # 匹配 #define MLOG_OUTPUT_LVL MLOG_LVL_xxx 或数字
            m = re.search(
                r'#\s*define\s+MLOG_OUTPUT_LVL\s+(MLOG_LVL_(\w+)|\d+)',
                content
            )
            if m:
                if m.group(2):
                    name = m.group(2).upper()
                    return LEVEL_MAP.get(name, 5)
                else:
                    try:
                        return int(m.group(1))
                    except ValueError:
                        pass
    return 5  # 默认 VERBOSE


def scan_file(filepath: str, rel_path: str, global_output_lvl: int) -> FileInfo:
    """扫描单个源文件，提取所有日志调用。"""
    try:
        with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
            raw_lines = f.readlines()
    except OSError:
        return FileInfo(path=rel_path)

    info = FileInfo(path=rel_path)

    # 提取 LOG_TAG
    for raw_line in raw_lines:
        m = re.match(r'\s*#\s*define\s+LOG_TAG\s+"([^"]*)"', raw_line)
        if m:
            info.log_tag = m.group(1)
            info.log_tag_defined = True
            break

    # 提取 LOG_LVL
    for raw_line in raw_lines:
        m = re.match(
            r'\s*#\s*define\s+LOG_LVL\s+(MLOG_LVL_(\w+)|\d+)',
            raw_line
        )
        if m:
            info.log_lvl_defined = True
            if m.group(2):
                info.log_lvl = m.group(2).upper()
            else:
                info.log_lvl = m.group(1)
            break

    # 文件级日志级别阈值
    file_lvl = 5  # 默认 VERBOSE
    if info.log_lvl_defined and info.log_lvl:
        if info.log_lvl in LEVEL_MAP:
            file_lvl = LEVEL_MAP[info.log_lvl]
        else:
            try:
                file_lvl = int(info.log_lvl)
            except ValueError:
                pass

    # 合并续行后的全文（用于括号匹配）
    full_text = join_continuation_lines(raw_lines)
    full_lines = full_text.split('\n')

    # 构建所有宏名的匹配模式
    all_macros = set(EXPLICIT_TAG_MACROS.keys()) | set(IMPLICIT_TAG_MACROS.keys()) | SPECIAL_MACROS
    macro_pattern = re.compile(
        r'\b(' + '|'.join(re.escape(m) for m in sorted(all_macros, key=len, reverse=True)) + r')\s*\('
    )

    # 逐逻辑行扫描
    for line_idx, line_text in enumerate(full_lines):
        line_no = line_idx + 1

        # 跳过注释行 (简单处理)
        stripped = line_text.strip()
        if stripped.startswith('//') or stripped.startswith('/*') or stripped.startswith('*'):
            continue
        # 跳过 #define 行中的宏定义本身（避免匹配宏展开定义）
        if re.match(r'\s*#\s*define\s+', stripped):
            continue
        # 跳过函数声明/原型行（含类型前缀的声明，如 "void mlog_hexdump(...)"）
        if re.match(r'\s*(?:void|size_t|uint\d+_t|int\d*_t|const\s|MlogErrCode|bool)\s+\w+\s*\(', stripped):
            continue
        # 跳过 extern 声明
        if re.match(r'\s*extern\s+', stripped):
            continue

        for m in macro_pattern.finditer(line_text):
            macro_name = m.group(1)
            paren_start = m.end() - 1  # 指向 '('

            # 提取完整参数括号
            paren_content = extract_balanced_parens(line_text, paren_start)
            if paren_content is None:
                continue

            # 去掉外层括号
            inner = paren_content[1:-1].strip()
            args = split_args(inner)
            raw_call = f"{macro_name}{paren_content}"
            if len(raw_call) > 200:
                raw_call = raw_call[:197] + "..."

            entry = None

            if macro_name in EXPLICIT_TAG_MACROS:
                # mlog_a(tag, fmt, ...) / mlog_assert(tag, fmt, ...)
                level_name = EXPLICIT_TAG_MACROS[macro_name]
                tag = extract_string_literal(args[0]) if args else "?"
                fmt_str = extract_string_literal(args[1]) if len(args) > 1 else ""
                entry = LogEntry(
                    file=rel_path, line=line_no, level=level_name,
                    tag=tag or "?", fmt_string=fmt_str or "",
                    macro=macro_name, raw_call=raw_call,
                )

            elif macro_name in IMPLICIT_TAG_MACROS:
                # log_a(fmt, ...) - tag 来自 LOG_TAG
                level_name = IMPLICIT_TAG_MACROS[macro_name]
                tag = info.log_tag if info.log_tag_defined else "NO_TAG"
                fmt_str = extract_string_literal(args[0]) if args else ""
                entry = LogEntry(
                    file=rel_path, line=line_no, level=level_name,
                    tag=tag, fmt_string=fmt_str or "",
                    macro=macro_name, tag_from_log_tag=True,
                    raw_call=raw_call,
                )

            elif macro_name == "mlog_raw":
                fmt_str = extract_string_literal(args[0]) if args else ""
                entry = LogEntry(
                    file=rel_path, line=line_no, level="RAW",
                    tag="", fmt_string=fmt_str or "",
                    macro=macro_name, raw_call=raw_call,
                )

            elif macro_name == "MLOG_ASSERT":
                expr = inner.strip()
                entry = LogEntry(
                    file=rel_path, line=line_no, level="ASSERT_CHK",
                    tag="mlog", fmt_string=expr,
                    macro=macro_name, raw_call=raw_call,
                )

            elif macro_name == "mlog_hexdump":
                name_arg = extract_string_literal(args[0]) if args else "?"
                entry = LogEntry(
                    file=rel_path, line=line_no, level="HEXDUMP",
                    tag=name_arg or "?",
                    fmt_string=f"width={args[1]}, size={args[3]}" if len(args) >= 4 else inner,
                    macro=macro_name, raw_call=raw_call,
                )

            if entry is None:
                continue

            # 判断编译期裁剪
            if entry.level in LEVEL_MAP:
                lvl_val = LEVEL_MAP[entry.level]
                if lvl_val > global_output_lvl:
                    entry.is_trimmed = True
                    entry.trimmed_by = "MLOG_OUTPUT_LVL"
                elif entry.tag_from_log_tag and lvl_val > file_lvl:
                    entry.is_trimmed = True
                    entry.trimmed_by = "LOG_LVL"

            info.entries.append(entry)

    return info


def scan_project(project_path: str) -> tuple:
    """扫描整个项目目录，返回 (global_output_lvl, [FileInfo])。"""
    global_output_lvl = parse_global_config(project_path)
    results = []

    for root, dirs, files in os.walk(project_path):
        # 跳过常见非源码目录
        dirs[:] = [d for d in dirs if d not in {
            '.git', '.svn', '__pycache__', 'node_modules',
            'build', 'cmake-build-debug', 'cmake-build-release',
            '.vscode', '.idea', 'tools',
        }]
        for fname in files:
            ext = os.path.splitext(fname)[1].lower()
            if ext not in SOURCE_EXTENSIONS:
                continue
            full_path = os.path.join(root, fname)
            rel_path = os.path.relpath(full_path, project_path)
            info = scan_file(full_path, rel_path, global_output_lvl)
            if info.entries:
                results.append(info)

    return global_output_lvl, results


# ---------------------------------------------------------------------------
# 校验
# ---------------------------------------------------------------------------

def run_checks(file_infos: List[FileInfo]) -> List[CheckWarning]:
    """执行增强校验，返回警告列表。"""
    warnings = []

    for fi in file_infos:
        # 检查 LOG_TAG 未定义
        has_implicit = any(e.tag_from_log_tag for e in fi.entries)
        if has_implicit and not fi.log_tag_defined:
            warnings.append(CheckWarning(
                file=fi.path, line=0,
                macro="LOG_TAG",
                category="NO_LOG_TAG",
                message=f"文件使用了 log_x() 宏但未定义 LOG_TAG，将使用默认值 \"NO_TAG\"",
            ))

        # 逐条检查格式字符串
        for entry in fi.entries:
            if entry.level in ("RAW", "ASSERT_CHK", "HEXDUMP"):
                if entry.level == "RAW" and entry.fmt_string:
                    pass  # 可以检查 raw 的格式字符串
                else:
                    continue

            if not entry.fmt_string:
                continue

            # 格式字符串参数数量校验
            expected_args = count_fmt_specifiers(entry.fmt_string)
            if expected_args > 0:
                # 计算实际传参数量
                raw = entry.raw_call
                paren_start = raw.find('(')
                if paren_start >= 0:
                    paren_content = extract_balanced_parens(raw, paren_start)
                    if paren_content:
                        inner = paren_content[1:-1].strip()
                        all_args = split_args(inner)

                        # 确定格式字符串在参数列表中的位置
                        if entry.macro in EXPLICIT_TAG_MACROS:
                            # mlog_x(tag, fmt, ...)
                            extra_args = len(all_args) - 2  # 减去 tag 和 fmt
                        elif entry.macro in IMPLICIT_TAG_MACROS or entry.macro == "mlog_raw":
                            # log_x(fmt, ...) / mlog_raw(fmt, ...)
                            extra_args = len(all_args) - 1  # 减去 fmt
                        else:
                            continue

                        if extra_args < 0:
                            extra_args = 0

                        if extra_args != expected_args:
                            warnings.append(CheckWarning(
                                file=fi.path, line=entry.line,
                                macro=entry.macro,
                                category="FMT_MISMATCH",
                                message=(
                                    f"格式字符串需要 {expected_args} 个参数，"
                                    f"实际传入 {extra_args} 个"
                                    f" → \"{entry.fmt_string}\""
                                ),
                            ))

    return warnings


# ---------------------------------------------------------------------------
# 输出格式化
# ---------------------------------------------------------------------------

LEVEL_COLORS = {
    "ASSERT":     "\033[35m",  # 紫色
    "ASSERT_CHK": "\033[35m",
    "ERROR":      "\033[31m",  # 红色
    "WARN":       "\033[33m",  # 黄色
    "INFO":       "\033[36m",  # 青色
    "DEBUG":      "\033[32m",  # 绿色
    "VERBOSE":    "\033[34m",  # 蓝色
    "RAW":        "\033[37m",  # 白色
    "HEXDUMP":    "\033[90m",  # 灰色
}
COLOR_RESET = "\033[0m"
COLOR_DIM = "\033[2m"
COLOR_BOLD = "\033[1m"
COLOR_WARN_BG = "\033[43;30m"  # 黄底黑字
COLOR_ERR_BG = "\033[41;37m"   # 红底白字


def colorize(text: str, color: str, use_color: bool) -> str:
    if use_color:
        return f"{color}{text}{COLOR_RESET}"
    return text


def format_table(
    file_infos: List[FileInfo],
    warnings: List[CheckWarning],
    global_lvl: int,
    show_trimmed: bool,
    use_color: bool,
) -> str:
    """生成终端表格报告。"""
    out = io.StringIO()

    def w(s=""):
        out.write(s + "\n")

    header = "MLog 日志调用扫描报告"
    w(colorize(f"{'=' * 60}", COLOR_BOLD, use_color))
    w(colorize(f"  {header}", COLOR_BOLD, use_color))
    w(colorize(f"{'=' * 60}", COLOR_BOLD, use_color))
    w(f"  全局输出级别: MLOG_OUTPUT_LVL = {LEVEL_NAMES[global_lvl]} ({global_lvl})")
    w()

    total_entries = 0
    total_trimmed = 0
    level_counts: Dict[str, int] = defaultdict(int)
    tag_counts: Dict[str, int] = defaultdict(int)

    for fi in file_infos:
        # 文件头
        tag_info = ""
        if fi.log_tag_defined:
            tag_info = f'LOG_TAG="{fi.log_tag}"'
        lvl_info = ""
        if fi.log_lvl_defined:
            lvl_info = f"LOG_LVL={fi.log_lvl}"
        meta_parts = [p for p in [tag_info, lvl_info] if p]
        meta = f" ({', '.join(meta_parts)})" if meta_parts else ""

        w(colorize(f"文件: {fi.path}{meta}", COLOR_BOLD, use_color))

        for entry in fi.entries:
            if entry.is_trimmed and not show_trimmed:
                total_trimmed += 1
                level_counts[entry.level] += 1
                tag_counts[entry.tag] += 1
                total_entries += 1
                continue

            total_entries += 1
            level_counts[entry.level] += 1
            if entry.tag:
                tag_counts[entry.tag] += 1

            lvl_color = LEVEL_COLORS.get(entry.level, "")
            lvl_str = colorize(f"[{entry.level:^10s}]", lvl_color, use_color)

            trimmed_mark = ""
            if entry.is_trimmed:
                total_trimmed += 1
                trimmed_mark = colorize(
                    f" ✂ 被 {entry.trimmed_by} 裁掉", COLOR_DIM, use_color
                )

            tag_str = ""
            if entry.tag and entry.level not in ("RAW", "ASSERT_CHK"):
                tag_str = f' tag="{entry.tag}"'

            # 格式字符串显示（截断过长的）
            fmt_display = entry.fmt_string
            if len(fmt_display) > 80:
                fmt_display = fmt_display[:77] + "..."

            if entry.level == "ASSERT_CHK":
                w(f"  L{entry.line:<5d} {lvl_str} MLOG_ASSERT({fmt_display}){trimmed_mark}")
            elif entry.level == "HEXDUMP":
                w(f"  L{entry.line:<5d} {lvl_str}{tag_str} ({fmt_display}){trimmed_mark}")
            else:
                w(f'  L{entry.line:<5d} {lvl_str}{tag_str} "{fmt_display}"{trimmed_mark}')

        w()

    # 警告
    if warnings:
        w(colorize(f"{'=' * 60}", COLOR_BOLD, use_color))
        w(colorize("  校验警告", COLOR_BOLD, use_color))
        w(colorize(f"{'=' * 60}", COLOR_BOLD, use_color))
        for warn in warnings:
            if warn.category == "NO_LOG_TAG":
                icon = colorize("⚠", COLOR_WARN_BG, use_color)
                loc = warn.file
            elif warn.category == "FMT_MISMATCH":
                icon = colorize("✗", COLOR_ERR_BG, use_color)
                loc = f"{warn.file}:L{warn.line}"
            else:
                icon = "?"
                loc = warn.file
            w(f"  {icon} [{warn.category}] {loc}")
            w(f"    {warn.message}")
        w()

    # 统计
    w(colorize(f"{'=' * 60}", COLOR_BOLD, use_color))
    w(colorize("  统计摘要", COLOR_BOLD, use_color))
    w(colorize(f"{'=' * 60}", COLOR_BOLD, use_color))
    w(f"  总计: {total_entries} 条日志调用, {len(file_infos)} 个文件")
    if total_trimmed > 0:
        w(f"  编译期裁剪: {total_trimmed} 条被裁掉")
    w()

    # 级别分布
    w("  级别分布:")
    for lvl in LEVEL_NAMES + ["RAW", "HEXDUMP", "ASSERT_CHK"]:
        cnt = level_counts.get(lvl, 0)
        if cnt > 0:
            bar = "█" * min(cnt, 40)
            lvl_color = LEVEL_COLORS.get(lvl, "")
            w(f"    {colorize(f'{lvl:>10s}', lvl_color, use_color)} : "
              f"{cnt:>4d}  {colorize(bar, lvl_color, use_color)}")
    w()

    # Tag 分布
    if tag_counts:
        w("  Tag 分布:")
        sorted_tags = sorted(tag_counts.items(), key=lambda x: -x[1])
        for tag, cnt in sorted_tags:
            if not tag:
                tag = "(无tag)"
            indicator = ""
            if tag == "NO_TAG":
                indicator = colorize(" ← 建议定义 LOG_TAG", COLOR_WARN_BG, use_color)
            w(f"    {tag:>15s} : {cnt:>4d}{indicator}")
        w()

    return out.getvalue()


def format_csv(file_infos: List[FileInfo], show_trimmed: bool) -> str:
    """生成 CSV 格式输出。"""
    out = io.StringIO()
    writer = csv.writer(out)
    writer.writerow([
        "文件", "行号", "级别", "Tag", "宏", "格式字符串",
        "Tag来源", "被裁剪", "裁剪原因"
    ])
    for fi in file_infos:
        for entry in fi.entries:
            if entry.is_trimmed and not show_trimmed:
                continue
            writer.writerow([
                entry.file, entry.line, entry.level, entry.tag,
                entry.macro, entry.fmt_string,
                "LOG_TAG" if entry.tag_from_log_tag else "显式",
                "是" if entry.is_trimmed else "",
                entry.trimmed_by if entry.is_trimmed else "",
            ])
    return out.getvalue()


def format_json(file_infos: List[FileInfo], show_trimmed: bool) -> str:
    """生成 JSON 格式输出。"""
    data = {
        "files": [],
        "summary": {"total": 0, "trimmed": 0, "by_level": {}, "by_tag": {}},
    }
    level_counts: Dict[str, int] = defaultdict(int)
    tag_counts: Dict[str, int] = defaultdict(int)
    total = 0
    trimmed = 0

    for fi in file_infos:
        file_data = {
            "path": fi.path,
            "log_tag": fi.log_tag,
            "log_lvl": fi.log_lvl,
            "entries": [],
        }
        for entry in fi.entries:
            total += 1
            level_counts[entry.level] += 1
            if entry.tag:
                tag_counts[entry.tag] += 1
            if entry.is_trimmed:
                trimmed += 1
            if entry.is_trimmed and not show_trimmed:
                continue
            file_data["entries"].append({
                "line": entry.line,
                "level": entry.level,
                "tag": entry.tag,
                "macro": entry.macro,
                "fmt_string": entry.fmt_string,
                "tag_from_log_tag": entry.tag_from_log_tag,
                "is_trimmed": entry.is_trimmed,
                "trimmed_by": entry.trimmed_by,
            })
        data["files"].append(file_data)

    data["summary"]["total"] = total
    data["summary"]["trimmed"] = trimmed
    data["summary"]["by_level"] = dict(level_counts)
    data["summary"]["by_tag"] = dict(tag_counts)

    return json.dumps(data, ensure_ascii=False, indent=2)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="MLog 日志调用静态扫描工具 - 提取项目中所有 mlog 日志调用",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "示例:\n"
            "  python mlog_scan.py .                     # 扫描当前目录\n"
            "  python mlog_scan.py . --level ERROR       # 只显示 >= ERROR\n"
            "  python mlog_scan.py . --tag sensor        # 只显示指定 tag\n"
            "  python mlog_scan.py . --format csv -o r.csv  # 导出 CSV\n"
            "  python mlog_scan.py . --check             # 启用校验\n"
            "  python mlog_scan.py . --show-trimmed      # 显示被裁掉的日志\n"
        ),
    )
    parser.add_argument("project", help="项目根目录路径")
    parser.add_argument("--level", choices=LEVEL_NAMES, default=None,
                        help="只显示 >= 指定级别的日志")
    parser.add_argument("--tag", default=None,
                        help="只显示指定 tag 的日志")
    parser.add_argument("--format", choices=["table", "csv", "json"],
                        default="table", help="输出格式 (默认: table)")
    parser.add_argument("--check", action="store_true",
                        help="启用格式字符串校验和 LOG_TAG 检查")
    parser.add_argument("--show-trimmed", action="store_true",
                        help="显示被编译期裁掉的日志")
    parser.add_argument("--no-color", action="store_true",
                        help="禁用终端颜色")
    parser.add_argument("-o", "--output", default=None,
                        help="输出到文件 (默认: 标准输出)")

    args = parser.parse_args()

    project_path = os.path.abspath(args.project)
    if not os.path.isdir(project_path):
        print(f"错误: 目录不存在 - {project_path}", file=sys.stderr)
        sys.exit(1)

    # 扫描
    global_lvl, file_infos = scan_project(project_path)

    # 过滤
    if args.level:
        min_lvl = LEVEL_MAP[args.level]
        for fi in file_infos:
            fi.entries = [
                e for e in fi.entries
                if e.level not in LEVEL_MAP or LEVEL_MAP[e.level] <= min_lvl
            ]
        file_infos = [fi for fi in file_infos if fi.entries]

    if args.tag:
        for fi in file_infos:
            fi.entries = [e for e in fi.entries if e.tag == args.tag]
        file_infos = [fi for fi in file_infos if fi.entries]

    # 校验
    warnings = []
    if args.check:
        warnings = run_checks(file_infos)

    # 输出
    use_color = (not args.no_color) and (args.output is None) and sys.stdout.isatty()

    if args.format == "table":
        output = format_table(file_infos, warnings, global_lvl,
                              args.show_trimmed, use_color)
    elif args.format == "csv":
        output = format_csv(file_infos, args.show_trimmed)
    elif args.format == "json":
        output = format_json(file_infos, args.show_trimmed)
    else:
        output = ""

    if args.output:
        encoding = 'utf-8-sig' if args.format == 'csv' else 'utf-8'
        with open(args.output, 'w', encoding=encoding, newline='') as f:
            f.write(output)
        print(f"报告已导出到: {args.output}")
    else:
        sys.stdout.write(output)


# ---------------------------------------------------------------------------
# GUI (tkinter)
# ---------------------------------------------------------------------------

def launch_gui():
    """启动 tkinter 图形界面。"""
    import tkinter as tk
    from tkinter import ttk, filedialog, messagebox
    import threading

    # ---- 级别对应的颜色 (tkinter tag 用) ----
    GUI_LEVEL_COLORS = {
        "ASSERT":     "#c050c0",
        "ASSERT_CHK": "#c050c0",
        "ERROR":      "#e04040",
        "WARN":       "#d0a020",
        "INFO":       "#20a0b0",
        "DEBUG":      "#30a030",
        "VERBOSE":    "#4070d0",
        "RAW":        "#707070",
        "HEXDUMP":    "#909090",
    }
    TRIMMED_COLOR = "#aaaaaa"
    WARNING_BG = "#fff3cd"
    ERROR_BG = "#f8d7da"

    class MlogScanGUI:
        def __init__(self, root: tk.Tk):
            self.root = root
            self.root.title("MLog 日志扫描工具")
            self.root.geometry("1100x720")
            self.root.minsize(800, 500)

            # 扫描结果缓存
            self.file_infos: List[FileInfo] = []
            self.all_entries: List[LogEntry] = []
            self.warnings: List[CheckWarning] = []
            self.global_lvl = 5
            self.project_path = ""

            self._build_ui()

        def _build_ui(self):
            # ============ 顶部工具栏 ============
            toolbar = ttk.Frame(self.root, padding=6)
            toolbar.pack(fill=tk.X)

            ttk.Label(toolbar, text="项目路径:").pack(side=tk.LEFT)
            self.path_var = tk.StringVar()
            path_entry = ttk.Entry(toolbar, textvariable=self.path_var, width=50)
            path_entry.pack(side=tk.LEFT, padx=(4, 2), fill=tk.X, expand=True)

            ttk.Button(toolbar, text="浏览...", command=self._browse_dir, width=7).pack(side=tk.LEFT, padx=2)
            self.scan_btn = ttk.Button(toolbar, text="扫描", command=self._start_scan, width=7)
            self.scan_btn.pack(side=tk.LEFT, padx=2)

            # ============ 过滤栏 ============
            filter_frame = ttk.LabelFrame(self.root, text=" 过滤选项 ", padding=6)
            filter_frame.pack(fill=tk.X, padx=6, pady=(0, 4))

            ttk.Label(filter_frame, text="最低级别:").pack(side=tk.LEFT)
            self.level_var = tk.StringVar(value="全部")
            level_combo = ttk.Combobox(
                filter_frame, textvariable=self.level_var,
                values=["全部"] + LEVEL_NAMES, state="readonly", width=10,
            )
            level_combo.pack(side=tk.LEFT, padx=(2, 12))
            level_combo.bind("<<ComboboxSelected>>", lambda e: self._apply_filter())

            ttk.Label(filter_frame, text="Tag:").pack(side=tk.LEFT)
            self.tag_var = tk.StringVar()
            tag_entry = ttk.Entry(filter_frame, textvariable=self.tag_var, width=15)
            tag_entry.pack(side=tk.LEFT, padx=(2, 12))
            tag_entry.bind("<Return>", lambda e: self._apply_filter())

            self.show_trimmed_var = tk.BooleanVar(value=True)
            ttk.Checkbutton(
                filter_frame, text="显示被裁减的日志",
                variable=self.show_trimmed_var, command=self._apply_filter,
            ).pack(side=tk.LEFT, padx=(0, 12))

            self.check_var = tk.BooleanVar(value=True)
            ttk.Checkbutton(
                filter_frame, text="启用校验",
                variable=self.check_var, command=self._apply_filter,
            ).pack(side=tk.LEFT, padx=(0, 12))

            ttk.Button(filter_frame, text="筛选", command=self._apply_filter, width=6).pack(side=tk.LEFT)
            ttk.Button(filter_frame, text="导出CSV", command=self._export_csv, width=8).pack(side=tk.RIGHT, padx=2)
            ttk.Button(filter_frame, text="导出JSON", command=self._export_json, width=8).pack(side=tk.RIGHT, padx=2)

            # ============ 主内容区 (PanedWindow) ============
            paned = ttk.PanedWindow(self.root, orient=tk.VERTICAL)
            paned.pack(fill=tk.BOTH, expand=True, padx=6, pady=(0, 4))

            # ---- 上半部分: Treeview 表格 ----
            tree_frame = ttk.Frame(paned)
            paned.add(tree_frame, weight=3)

            columns = ("file", "line", "level", "tag", "macro", "fmt_string", "trimmed")
            self.tree = ttk.Treeview(
                tree_frame, columns=columns, show="headings",
                selectmode="browse",
            )
            self.tree.heading("file", text="文件", anchor=tk.W)
            self.tree.heading("line", text="行号", anchor=tk.CENTER)
            self.tree.heading("level", text="级别", anchor=tk.CENTER)
            self.tree.heading("tag", text="Tag", anchor=tk.W)
            self.tree.heading("macro", text="宏", anchor=tk.W)
            self.tree.heading("fmt_string", text="格式字符串 / 表达式", anchor=tk.W)
            self.tree.heading("trimmed", text="裁剪", anchor=tk.CENTER)

            self.tree.column("file", width=200, minwidth=100)
            self.tree.column("line", width=55, minwidth=45, anchor=tk.CENTER)
            self.tree.column("level", width=85, minwidth=60, anchor=tk.CENTER)
            self.tree.column("tag", width=100, minwidth=60)
            self.tree.column("macro", width=110, minwidth=70)
            self.tree.column("fmt_string", width=380, minwidth=150)
            self.tree.column("trimmed", width=70, minwidth=50, anchor=tk.CENTER)

            # 滚动条
            tree_scroll_y = ttk.Scrollbar(tree_frame, orient=tk.VERTICAL, command=self.tree.yview)
            tree_scroll_x = ttk.Scrollbar(tree_frame, orient=tk.HORIZONTAL, command=self.tree.xview)
            self.tree.configure(yscrollcommand=tree_scroll_y.set, xscrollcommand=tree_scroll_x.set)

            self.tree.grid(row=0, column=0, sticky="nsew")
            tree_scroll_y.grid(row=0, column=1, sticky="ns")
            tree_scroll_x.grid(row=1, column=0, sticky="ew")
            tree_frame.rowconfigure(0, weight=1)
            tree_frame.columnconfigure(0, weight=1)

            # 配置级别颜色 tag
            for lvl, color in GUI_LEVEL_COLORS.items():
                self.tree.tag_configure(f"lvl_{lvl}", foreground=color)
            self.tree.tag_configure("trimmed", foreground=TRIMMED_COLOR)

            # ---- 下半部分: 统计 + 警告 Notebook ----
            bottom_nb = ttk.Notebook(paned)
            paned.add(bottom_nb, weight=1)

            # 统计 tab
            stats_frame = ttk.Frame(bottom_nb, padding=6)
            bottom_nb.add(stats_frame, text=" 统计 ")
            self.stats_text = tk.Text(
                stats_frame, height=8, wrap=tk.WORD,
                font=("Consolas", 10), state=tk.DISABLED,
                bg="#fafafa", relief=tk.FLAT,
            )
            self.stats_text.pack(fill=tk.BOTH, expand=True)

            # 警告 tab
            warn_frame = ttk.Frame(bottom_nb, padding=6)
            bottom_nb.add(warn_frame, text=" 校验警告 ")
            self.warn_tree = ttk.Treeview(
                warn_frame,
                columns=("icon", "category", "location", "message"),
                show="headings", selectmode="browse",
            )
            self.warn_tree.heading("icon", text="", anchor=tk.CENTER)
            self.warn_tree.heading("category", text="类型", anchor=tk.W)
            self.warn_tree.heading("location", text="位置", anchor=tk.W)
            self.warn_tree.heading("message", text="详情", anchor=tk.W)
            self.warn_tree.column("icon", width=30, minwidth=25, anchor=tk.CENTER)
            self.warn_tree.column("category", width=110, minwidth=80)
            self.warn_tree.column("location", width=200, minwidth=100)
            self.warn_tree.column("message", width=500, minwidth=200)
            self.warn_tree.tag_configure("warn_tag", background=WARNING_BG)
            self.warn_tree.tag_configure("error_tag", background=ERROR_BG)

            warn_scroll = ttk.Scrollbar(warn_frame, orient=tk.VERTICAL, command=self.warn_tree.yview)
            self.warn_tree.configure(yscrollcommand=warn_scroll.set)
            self.warn_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
            warn_scroll.pack(side=tk.RIGHT, fill=tk.Y)

            # ============ 底部状态栏 ============
            self.status_var = tk.StringVar(value="就绪 - 请选择项目路径后点击扫描")
            status_bar = ttk.Label(
                self.root, textvariable=self.status_var,
                relief=tk.SUNKEN, anchor=tk.W, padding=(6, 2),
            )
            status_bar.pack(fill=tk.X, side=tk.BOTTOM)

        # ---- 交互方法 ----

        def _browse_dir(self):
            d = filedialog.askdirectory(title="选择项目根目录")
            if d:
                self.path_var.set(d)

        def _start_scan(self):
            path = self.path_var.get().strip()
            if not path or not os.path.isdir(path):
                messagebox.showwarning("路径无效", "请选择有效的项目目录。")
                return

            self.project_path = os.path.abspath(path)
            self.scan_btn.configure(state=tk.DISABLED)
            self.status_var.set("正在扫描...")

            # 在子线程执行扫描，避免 GUI 冻结
            threading.Thread(target=self._do_scan, daemon=True).start()

        def _do_scan(self):
            try:
                self.global_lvl, self.file_infos = scan_project(self.project_path)
                # 展平所有 entry
                self.all_entries = []
                for fi in self.file_infos:
                    for e in fi.entries:
                        self.all_entries.append(e)
                # 校验
                self.warnings = run_checks(self.file_infos)
            except Exception as exc:
                self.root.after(0, lambda: messagebox.showerror("扫描错误", str(exc)))
                self.root.after(0, lambda: self.status_var.set("扫描出错"))
                self.root.after(0, lambda: self.scan_btn.configure(state=tk.NORMAL))
                return

            self.root.after(0, self._on_scan_done)

        def _on_scan_done(self):
            self.scan_btn.configure(state=tk.NORMAL)
            self._apply_filter()
            self.status_var.set(
                f"扫描完成 — {len(self.all_entries)} 条日志, "
                f"{len(self.file_infos)} 个文件, "
                f"MLOG_OUTPUT_LVL = {LEVEL_NAMES[self.global_lvl]}"
            )

        def _apply_filter(self):
            """根据当前过滤条件刷新表格。"""
            level_filter = self.level_var.get()
            tag_filter = self.tag_var.get().strip()
            show_trimmed = self.show_trimmed_var.get()
            do_check = self.check_var.get()

            # 过滤
            filtered = []
            for entry in self.all_entries:
                if not show_trimmed and entry.is_trimmed:
                    continue
                if level_filter != "全部" and entry.level in LEVEL_MAP:
                    if LEVEL_MAP[entry.level] > LEVEL_MAP.get(level_filter, 5):
                        continue
                if tag_filter and entry.tag != tag_filter:
                    continue
                filtered.append(entry)

            # 刷新 treeview
            self.tree.delete(*self.tree.get_children())
            for entry in filtered:
                trimmed_str = ""
                if entry.is_trimmed:
                    trimmed_str = f"✂ {entry.trimmed_by}"

                fmt_display = entry.fmt_string
                if len(fmt_display) > 120:
                    fmt_display = fmt_display[:117] + "..."

                tags = []
                if entry.is_trimmed:
                    tags.append("trimmed")
                else:
                    tags.append(f"lvl_{entry.level}")

                self.tree.insert("", tk.END, values=(
                    entry.file, entry.line, entry.level,
                    entry.tag, entry.macro, fmt_display, trimmed_str,
                ), tags=tags)

            # 刷新统计
            self._refresh_stats(filtered)

            # 刷新警告
            self._refresh_warnings(do_check)

        def _refresh_stats(self, entries: List[LogEntry]):
            level_counts: Dict[str, int] = defaultdict(int)
            tag_counts: Dict[str, int] = defaultdict(int)
            trimmed_count = 0

            for e in self.all_entries:
                level_counts[e.level] += 1
                if e.tag:
                    tag_counts[e.tag] += 1
                if e.is_trimmed:
                    trimmed_count += 1

            self.stats_text.configure(state=tk.NORMAL)
            self.stats_text.delete("1.0", tk.END)

            lines = []
            lines.append(f"总计: {len(self.all_entries)} 条日志调用, "
                         f"{len(self.file_infos)} 个文件")
            lines.append(f"全局输出级别: MLOG_OUTPUT_LVL = "
                         f"{LEVEL_NAMES[self.global_lvl]} ({self.global_lvl})")
            if trimmed_count:
                lines.append(f"编译期裁剪: {trimmed_count} 条被裁掉")
            lines.append(f"当前显示: {len(entries)} 条 (过滤后)")
            lines.append("")

            lines.append("级别分布:")
            for lvl in LEVEL_NAMES + ["RAW", "HEXDUMP", "ASSERT_CHK"]:
                cnt = level_counts.get(lvl, 0)
                if cnt > 0:
                    bar = "█" * min(cnt, 50)
                    lines.append(f"  {lvl:>10s} : {cnt:>4d}  {bar}")

            lines.append("")
            lines.append("Tag 分布:")
            sorted_tags = sorted(tag_counts.items(), key=lambda x: -x[1])
            for tag, cnt in sorted_tags:
                if not tag:
                    tag = "(无tag)"
                note = "  ← 建议定义 LOG_TAG" if tag == "NO_TAG" else ""
                lines.append(f"  {tag:>15s} : {cnt:>4d}{note}")

            self.stats_text.insert("1.0", "\n".join(lines))
            self.stats_text.configure(state=tk.DISABLED)

        def _refresh_warnings(self, do_check: bool):
            self.warn_tree.delete(*self.warn_tree.get_children())
            if not do_check:
                return

            warnings = run_checks(self.file_infos)
            for w in warnings:
                if w.category == "NO_LOG_TAG":
                    icon = "⚠"
                    loc = w.file
                    tag = ("warn_tag",)
                elif w.category == "FMT_MISMATCH":
                    icon = "✗"
                    loc = f"{w.file}:L{w.line}"
                    tag = ("error_tag",)
                else:
                    icon = "?"
                    loc = w.file
                    tag = ()
                self.warn_tree.insert("", tk.END, values=(
                    icon, w.category, loc, w.message,
                ), tags=tag)

        # ---- 导出 ----

        def _get_filtered_infos(self) -> List[FileInfo]:
            """获得过滤后的 FileInfo 列表（用于导出）。"""
            level_filter = self.level_var.get()
            tag_filter = self.tag_var.get().strip()
            show_trimmed = self.show_trimmed_var.get()

            result = []
            for fi in self.file_infos:
                filtered_entries = []
                for e in fi.entries:
                    if not show_trimmed and e.is_trimmed:
                        continue
                    if level_filter != "全部" and e.level in LEVEL_MAP:
                        if LEVEL_MAP[e.level] > LEVEL_MAP.get(level_filter, 5):
                            continue
                    if tag_filter and e.tag != tag_filter:
                        continue
                    filtered_entries.append(e)
                if filtered_entries:
                    new_fi = FileInfo(
                        path=fi.path, log_tag=fi.log_tag, log_lvl=fi.log_lvl,
                        log_tag_defined=fi.log_tag_defined,
                        log_lvl_defined=fi.log_lvl_defined,
                        entries=filtered_entries,
                    )
                    result.append(new_fi)
            return result

        def _export_csv(self):
            if not self.all_entries:
                messagebox.showinfo("提示", "请先执行扫描。")
                return
            path = filedialog.asksaveasfilename(
                title="导出 CSV", defaultextension=".csv",
                filetypes=[("CSV 文件", "*.csv"), ("所有文件", "*.*")],
            )
            if not path:
                return
            infos = self._get_filtered_infos()
            content = format_csv(infos, show_trimmed=True)
            with open(path, 'w', encoding='utf-8-sig', newline='') as f:
                f.write(content)
            self.status_var.set(f"已导出 CSV → {path}")

        def _export_json(self):
            if not self.all_entries:
                messagebox.showinfo("提示", "请先执行扫描。")
                return
            path = filedialog.asksaveasfilename(
                title="导出 JSON", defaultextension=".json",
                filetypes=[("JSON 文件", "*.json"), ("所有文件", "*.*")],
            )
            if not path:
                return
            infos = self._get_filtered_infos()
            content = format_json(infos, show_trimmed=True)
            with open(path, 'w', encoding='utf-8') as f:
                f.write(content)
            self.status_var.set(f"已导出 JSON → {path}")

    # ---- 启动 ----
    root = tk.Tk()
    app = MlogScanGUI(root)
    root.mainloop()


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    # 无命令行参数时启动 GUI，否则走 CLI
    if len(sys.argv) <= 1:
        launch_gui()
    else:
        main()
