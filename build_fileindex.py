# -*- coding: utf-8 -*-
"""
build_fileindex.py — 扫描多个目录生成文件索引，配合 index.html 浏览。

输出两个文件（同一次运行派生，内容同源）：
    fileindex.json    index.html 通过 http 访问时 fetch 它
    fileindex.js      "window.FILEINDEX = {...};" 双击 file:// 打开时的兜底

覆盖旧配置前自动备份，默认只保留最近 5 份。

路径规则：
    所有相对路径都以「本脚本所在目录」为基准，所以整个文件夹可以随便搬，
    不用改任何配置。需要指到别处时再写绝对路径。

用法：
    python build_fileindex.py
    python build_fileindex.py --out ../site/fileindex.json
"""

import argparse
import fnmatch
import json
import os
import shutil
import sys
import time
from datetime import datetime

try:
    from urllib.parse import quote
except ImportError:  # pragma: no cover
    from urllib import quote


# ====================================================================
# 基准目录：脚本所在目录，相对路径全部相对它解析
# ====================================================================

BASE_DIR = os.path.dirname(os.path.abspath(__file__))


# ====================================================================
# 配置区：改这里就够了
# ====================================================================

CONFIG = {
    # 要扫描的根目录，可多个；相对 BASE_DIR
    "roots": ["archive", "assets"],

    # 只收这些后缀（[] 表示所有后缀都收）；大小写不敏感，点可有可无
    "include": [
        ".md", ".txt", ".log",
        ".html", ".htm", ".css", ".scss", ".less",
        ".js", ".mjs", ".cjs", ".ts", ".tsx", ".jsx",
        ".json", ".yaml", ".yml", ".toml", ".xml",
        ".py", ".pyw", ".php", ".rb", ".go", ".rs",
        ".java", ".kt", ".scala", ".c", ".cpp", ".h", ".hpp",
        ".sh", ".bash", ".zsh", ".fish", ".bat", ".cmd", ".ps1",
        ".sql", ".graphql", ".prisma",
        ".jpg", ".jpeg", ".png", ".gif", ".webp", ".svg", ".ico", ".bmp", ".tiff",
        ".mp3", ".wav", ".flac", ".aac", ".ogg", ".m4a",
        ".mp4", ".webm", ".mkv", ".mov", ".avi", ".flv", ".wmv",
        ".zip", ".rar", ".7z", ".tar", ".gz", ".bz2", ".xz",
        ".pdf", ".doc", ".docx", ".xls", ".xlsx", ".ppt", ".pptx",
        ".epub", ".mobi", ".azw3"
        ],

    # 排除规则：
    #   以点开头  -> 按后缀排除，如 ".tmp"
    #   含 * 或 ? -> 按文件名通配排除，如 "~$*"
    #   其它      -> 当作后缀处理，如 "Thumbs.db"
    "exclude": [".tmp", ".bak", "Thumbs.db", "~$*"],

    # include 非空时，是否把「没有后缀的文件」也收进来
    "include_no_extension": True,

    # 不继续递归的目录：进入它、只收这一层文件，不再下钻
    "stop_dirs": ["archive/game/play002-20261114-stroll"],

    # 整个跳过的目录（连子树）：目录名、通配或相对/绝对路径都行
    "skip_dirs": ["node_modules", ".git", "$RECYCLE.BIN"],

    # 输出的 json 路径；.js 写在同目录同名。相对 BASE_DIR
    "out_file": "fileindex.json",

    # 链接生成方式：
    #   "relative" -> 相对 index.html 所在目录，适合整棵树一起搬
    #   "absolute" -> file:///D:/... 绝对链接，index.html 放哪都能点开
    "link_mode": "relative",

    # 手动指定链接前缀（写这里就忽略 link_mode），如 "https://x.com/files"
    "url_prefix": "",

    # 最大递归深度，0 表示不限
    "max_depth": 0,

    # 是否收录隐藏/系统文件
    "include_hidden": False,

    # 旧配置保留几份，0 表示不做备份轮转
    "keep_backups": 5,

    # 备份目录，留空 = <输出目录>/_bak
    "backup_dir": "",

    # 输出带缩进的 JSON（体积更大，便于人看）
    "pretty": False,
}


# ====================================================================
# 工具函数
# ====================================================================

FILE_ATTRIBUTE_HIDDEN = 0x2
FILE_ATTRIBUTE_SYSTEM = 0x4


def resolve(path):
    """相对路径 -> 以 BASE_DIR 为基准的绝对路径。"""
    if not path:
        return path
    p = os.path.expanduser(os.path.expandvars(path))
    if os.path.isabs(p):
        return os.path.abspath(p)
    return os.path.abspath(os.path.join(BASE_DIR, p))


def norm_ext(raw):
    """把 'pdf' / '.PDF' / '*.pdf' 统一成 '.pdf'。"""
    t = (raw or "").strip()
    if not t:
        return ""
    if t.startswith("*."):
        t = t[1:]
    if not t.startswith("."):
        t = "." + t
    return t.lower()


def is_hidden(entry):
    """隐藏/系统文件判断；非 Windows 平台返回 False。"""
    try:
        st = entry.stat(follow_symlinks=False)
    except OSError:
        return False
    attrs = getattr(st, "st_file_attributes", 0)
    return bool(attrs & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))


def dir_matches(path, name, specs):
    """目录是否命中 skip / stop 规则。"""
    if not specs:
        return False
    lname = name.lower()
    for spec in specs:
        s = (spec or "").strip()
        if not s:
            continue
        if "*" in s or "?" in s:
            if fnmatch.fnmatch(lname, s.lower()):
                return True
            continue
        if "\\" in s or "/" in s:
            try:
                if os.path.normcase(os.path.abspath(s)) == os.path.normcase(os.path.abspath(path)):
                    return True
            except OSError:
                pass
            continue
        if os.path.normcase(s) == os.path.normcase(name):
            return True
    return False


def path_to_url_prefix(path):
    """把目录变成 file:/// 形式的绝对前缀。"""
    try:
        from pathlib import Path
        return Path(path).as_uri()
    except Exception:
        return "file:///" + quote(path.replace("\\", "/"), safe="/:")


def rel_url_prefix(target_dir, base_dir):
    """target_dir 相对 base_dir 的 URL 前缀；同一层返回空串。"""
    rel = os.path.relpath(target_dir, base_dir).replace(os.sep, "/")
    if rel in (".", ""):
        return ""
    segs = []
    for seg in rel.split("/"):
        segs.append(seg if seg in (".", "..") else quote(seg, safe=""))
    return "/".join(segs)


def build_url(rel_path, prefix):
    """相对路径 + 前缀 -> URL，逐段编码，空格/中文/# 都不会炸。"""
    segs = []
    if prefix:
        segs.extend(prefix.rstrip("/").split("/"))
    for seg in rel_path.split("/"):
        if seg:
            segs.append(quote(seg, safe=""))
    return "/".join(segs)


def backup_old(out_path, backup_dir, keep):
    """把旧配置复制到备份目录并轮转。返回 (备份路径, 保留份数)。"""
    if not os.path.isfile(out_path):
        return None, 0

    bd = backup_dir or os.path.join(os.path.dirname(out_path), "_bak")
    if not os.path.isdir(bd):
        os.makedirs(bd)

    leaf = os.path.basename(out_path)
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    bak = os.path.join(bd, "%s.%s.bak" % (leaf, stamp))

    n = 1
    while os.path.exists(bak):
        bak = os.path.join(bd, "%s.%s-%d.bak" % (leaf, stamp, n))
        n += 1

    shutil.copy2(out_path, bak)

    if keep > 0:
        # 按修改时间倒序（同一秒内生成的多份再用文件名兜底），保证删的是最老的
        olds = [f for f in os.listdir(bd) if f.startswith(leaf + ".") and f.endswith(".bak")]
        olds.sort(
            key=lambda f: (os.path.getmtime(os.path.join(bd, f)), f),
            reverse=True,
        )
        for f in olds[keep:]:
            try:
                os.remove(os.path.join(bd, f))
            except OSError:
                pass

    kept = len([f for f in os.listdir(bd) if f.startswith(leaf + ".") and f.endswith(".bak")])
    return bak, kept


# ====================================================================
# 扫描
# ====================================================================

def scan(cfg, out_dir):
    inc_exts = {norm_ext(x) for x in cfg["include"] if norm_ext(x)}

    exc_exts = set()
    exc_names = []
    for x in cfg["exclude"]:
        s = (x or "").strip()
        if not s:
            continue
        if s.startswith("."):
            exc_exts.add(norm_ext(s))
        elif "*" in s or "?" in s:
            exc_names.append(s.lower())
        else:
            exc_exts.add(norm_ext(s))

    # 含分隔符的规则先解析成绝对路径（相对 BASE_DIR）
    skip_specs = [resolve(s) if ("\\" in s or "/" in s) else s
                  for s in cfg["skip_dirs"] if (s or "").strip()]
    stop_specs = [resolve(s) if ("\\" in s or "/" in s) else s
                  for s in cfg["stop_dirs"] if (s or "").strip()]

    include_no_ext = bool(cfg["include_no_extension"])
    include_hidden = bool(cfg["include_hidden"])
    max_depth = int(cfg["max_depth"] or 0)
    manual_prefix = (cfg["url_prefix"] or "").strip()
    link_mode = (cfg.get("link_mode") or "relative").strip().lower()

    roots_info = []
    files = []
    skipped_dirs = 0
    started = datetime.now()

    for root in cfg["roots"]:
        root_path = resolve(root)
        if not os.path.isdir(root_path):
            print("[warn] 目录不存在，跳过: %s" % root_path)
            continue

        base = root_path.rstrip("\\/") or root_path
        idx = len(roots_info)
        roots_info.append({"i": idx, "name": os.path.basename(base), "path": base})
        print("[scan] %s" % base)

        if manual_prefix:
            prefix = manual_prefix
        elif link_mode == "absolute":
            prefix = path_to_url_prefix(base)
        else:
            prefix = rel_url_prefix(base, out_dir)

        stack = [(base, 0, False)]

        while stack:
            cur_dir, depth, no_recurse = stack.pop()

            try:
                entries = list(os.scandir(cur_dir))
            except OSError:
                skipped_dirs += 1
                continue

            # ---- 本层文件 ----
            for entry in entries:
                try:
                    if not entry.is_file(follow_symlinks=False):
                        continue
                    st = entry.stat(follow_symlinks=False)
                except OSError:
                    continue

                if not include_hidden and is_hidden(entry):
                    continue

                name = entry.name
                ext = os.path.splitext(name)[1].lower()

                if inc_exts:
                    if ext == "":
                        if not include_no_ext:
                            continue
                    elif ext not in inc_exts:
                        continue

                if ext and ext in exc_exts:
                    continue

                lname = name.lower()
                excluded = False
                for pat in exc_names:
                    if fnmatch.fnmatch(lname, pat):
                        excluded = True
                        break
                if excluded:
                    continue

                rel = os.path.relpath(entry.path, base).replace(os.sep, "/")

                files.append({
                    "r": idx,
                    "p": rel,
                    "u": build_url(rel, prefix),
                    "e": ext,
                    "s": st.st_size,
                    "m": datetime.fromtimestamp(st.st_mtime).strftime("%Y-%m-%dT%H:%M:%S"),
                })

            if no_recurse:
                continue
            if max_depth and depth >= max_depth:
                continue

            # ---- 子目录入栈 ----
            for entry in entries:
                try:
                    if not entry.is_dir(follow_symlinks=False):
                        continue
                except OSError:
                    continue

                if not include_hidden and is_hidden(entry):
                    continue

                if dir_matches(entry.path, entry.name, skip_specs):
                    skipped_dirs += 1
                    continue

                stop = dir_matches(entry.path, entry.name, stop_specs)
                stack.append((entry.path, depth + 1, stop))

    files.sort(key=lambda f: (f["r"], f["p"]))
    total = sum(f["s"] for f in files)

    obj = {
        "generatedAt": datetime.now().isoformat(),
        "startedAt": started.isoformat(),
        "roots": roots_info,
        "count": len(files),
        "totalBytes": total,
        "files": files,
    }
    return obj, skipped_dirs


# ====================================================================
# 输出
# ====================================================================

def write_output(obj, cfg, out_path):
    out_dir = os.path.dirname(out_path)
    if out_dir and not os.path.isdir(out_dir):
        os.makedirs(out_dir)

    if cfg["pretty"]:
        text = json.dumps(obj, ensure_ascii=False, indent=1)
    else:
        text = json.dumps(obj, ensure_ascii=False, separators=(",", ":"))

    bak, kept = (None, 0)
    if not cfg.get("no_backup"):
        bak_dir = resolve(cfg["backup_dir"]) if cfg["backup_dir"] else ""
        bak, kept = backup_old(out_path, bak_dir, int(cfg["keep_backups"] or 0))

    with open(out_path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text)

    js_path = os.path.splitext(out_path)[0] + ".js"
    with open(js_path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("window.FILEINDEX = " + text + ";")

    return js_path, bak, kept


# ====================================================================
# 入口
# ====================================================================

def main(argv=None):
    ap = argparse.ArgumentParser(description="扫描目录生成 fileindex.json + fileindex.js")
    ap.add_argument("--config", help="外部 json 配置，覆盖脚本内 CONFIG")
    ap.add_argument("--base", help="基准目录，默认脚本所在目录")
    ap.add_argument("--roots", nargs="+", help="根目录，可多个，空格分隔")
    ap.add_argument("--include", nargs="+", help="后缀白名单")
    ap.add_argument("--exclude", nargs="+", help="排除规则")
    ap.add_argument("--no-extension", action="store_true", help="收录无后缀文件")
    ap.add_argument("--skip-dir", nargs="+", help="整棵跳过的目录")
    ap.add_argument("--stop-dir", nargs="+", help="不继续下钻的目录")
    ap.add_argument("--out", help="输出 json 路径（相对则相对基准目录）")
    ap.add_argument("--link-mode", choices=["relative", "absolute"], help="链接生成方式")
    ap.add_argument("--prefix", help="手动链接前缀")
    ap.add_argument("--max-depth", type=int, help="最大递归深度，0 不限")
    ap.add_argument("--include-hidden", action="store_true", help="收录隐藏/系统文件")
    ap.add_argument("--keep-backups", type=int, help="旧配置保留几份")
    ap.add_argument("--backup-dir", help="备份目录")
    ap.add_argument("--no-backup", action="store_true", help="覆盖前不备份")
    ap.add_argument("--pretty", action="store_true", help="输出带缩进的 JSON")
    args = ap.parse_args(argv)

    global BASE_DIR
    if args.base:
        BASE_DIR = os.path.abspath(args.base)

    cfg = dict(CONFIG)

    if args.config:
        with open(resolve(args.config), encoding="utf-8") as fh:
            cfg.update(json.load(fh))

    if args.roots:
        cfg["roots"] = args.roots
    if args.include:
        cfg["include"] = args.include
    if args.exclude:
        cfg["exclude"] = args.exclude
    if args.no_extension:
        cfg["include_no_extension"] = True
    if args.skip_dir:
        cfg["skip_dirs"] = args.skip_dir
    if args.stop_dir:
        cfg["stop_dirs"] = args.stop_dir
    if args.out:
        cfg["out_file"] = args.out
    if args.link_mode:
        cfg["link_mode"] = args.link_mode
    if args.prefix is not None:
        cfg["url_prefix"] = args.prefix
    if args.max_depth is not None:
        cfg["max_depth"] = args.max_depth
    if args.include_hidden:
        cfg["include_hidden"] = True
    if args.keep_backups is not None:
        cfg["keep_backups"] = args.keep_backups
    if args.backup_dir:
        cfg["backup_dir"] = args.backup_dir
    if args.pretty:
        cfg["pretty"] = True
    if args.no_backup:
        cfg["no_backup"] = True

    out_path = resolve(cfg["out_file"])
    out_dir = os.path.dirname(out_path)

    print("[base] %s" % BASE_DIR)

    t0 = time.time()
    obj, skipped = scan(cfg, out_dir)
    js_path, bak, kept = write_output(obj, cfg, out_path)

    size_mb = obj["totalBytes"] / (1024.0 * 1024.0)
    print("")
    print("完成：%d 个文件，%.2f MB，跳过目录 %d 个，用时 %.2fs"
          % (obj["count"], size_mb, skipped, time.time() - t0))
    if bak:
        print("备份：%s（现存 %d 份）" % (bak, kept))
    print("JSON：%s" % out_path)
    print("JS  ：%s" % js_path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
#（注：内容由AI生成）
