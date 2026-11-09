#!/usr/bin/env python3
"""Build tg as one header: tg_single.h.

    python3 tools/amalgamate.py [-o build/tg_single.h]

Use it like stb:

    #define TG_IMPLEMENTATION      // in exactly one .cpp
    #define TG_BACKEND_SOFT        // optional backends, in every file that includes it:
    #define TG_BACKEND_GLFW_GL3    //   GLFW + OpenGL 3 (+ tg::App when TG_APP is defined); link glfw
    #define TG_BACKEND_WIN32       //   Win32 platform, D3D11 and D3D9 renderers (+ tg::App with TG_APP)
    #define TG_BACKEND_VULKAN      //   Vulkan renderer (link vulkan; TG_VULKAN_SHADERC + shaderc for custom shaders)
    #define TG_APP
    #include "tg_single.h"

Project headers are inlined once each (they all use #pragma once). Third-party headers (stb,
nanosvg) are inlined at every include so their own guards and *_IMPLEMENTATION switches behave
as in the normal build; only the first *_IMPLEMENTATION of a library that the preprocessor actually
reaches takes effect (everything is one translation unit).
"""
import argparse
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INCLUDE_DIRS = [os.path.join(ROOT, d) for d in ("include", "backends", "third_party/stb", "third_party/nanosvg")]
THIRD_PARTY = os.path.normcase(os.path.join(ROOT, "third_party"))

# Core sources, in CMake order (src/ then src/widgets/), minus the platform-specific pieces.
def core_sources():
    cm = open(os.path.join(ROOT, "CMakeLists.txt"), encoding="utf-8").read()
    block = cm[cm.index("add_library( tg STATIC"):]
    block = block[: block.index(")")]
    return [l.strip() for l in block.splitlines()[1:] if l.strip().endswith(".cpp")]

BACKENDS = [
    # (macro, headers, sources)
    ("TG_BACKEND_SOFT", ["backends/soft_renderer.h"], ["backends/soft_renderer.cpp"]),
    ("TG_BACKEND_GLFW_GL3", ["backends/gl3_renderer.h", "backends/glfw_platform.h"], ["backends/gl3_renderer.cpp", "backends/glfw_platform.cpp"]),
    ("TG_BACKEND_WIN32", ["backends/win32_platform.h", "backends/d3d11_renderer.h", "backends/d3d9_renderer.h"],
     ["backends/win32_platform.cpp", "backends/d3d11_renderer.cpp", "backends/d3d9_renderer.cpp"]),
    ("TG_BACKEND_VULKAN", ["backends/vk_renderer.h"], ["backends/vk_renderer.cpp"]),  # + TG_VULKAN_SHADERC for custom shaders
]

INC_RE = re.compile(r'^\s*#\s*include\s*"([^"]+)"')
IMPL_RE = re.compile(r'^\s*#\s*define\s+(STB_\w+_IMPLEMENTATION|NANOSVG\w*_IMPLEMENTATION)\b')
ONCE_RE = re.compile(r'^\s*#\s*pragma\s+once\b')
PP_RE = re.compile(r'^\s*#\s*(if|ifdef|ifndef|else|elif|endif)\b(.*)$')


def comment_state(line, in_comment):
    """Is a /* block comment */ still open at the end of this line? (strings / // handled roughly)"""
    i, n = 0, len(line)
    while i < n:
        if in_comment:
            j = line.find("*/", i)
            if j < 0:
                return True
            in_comment, i = False, j + 2
        else:
            c = line[i]
            if c == '"':  # skip a string literal
                i += 1
                while i < n and line[i] != '"':
                    i += 2 if line[i] == "\\" else 1
                i += 1
            elif line.startswith("//", i):
                return False
            elif line.startswith("/*", i):
                in_comment, i = True, i + 2
            else:
                i += 1
    return in_comment


class Amalgam:
    def __init__(self):
        self.seen = set()       # project headers already inlined
        self.impl_done = set()  # third-party *_IMPLEMENTATION already emitted
        self.stack = []         # files being expanded (a header including itself is left alone)

    def resolve(self, name, from_dir):
        for d in [from_dir] + INCLUDE_DIRS:
            p = os.path.normpath(os.path.join(d, name))
            if os.path.isfile(p):
                return os.path.normcase(os.path.abspath(p))
        return None

    def expand(self, path, out):
        # one spelling per file: Windows mixes "\\" and "/" (and letter case), which would include headers twice
        path = os.path.normcase(os.path.abspath(path))
        rel = os.path.relpath(path, ROOT).replace("\\", "/")
        third = path.startswith(THIRD_PARTY)
        if path in self.stack:
            return
        if not third:
            if path in self.seen:
                return
            self.seen.add(path)
        out.append(f"// ---- {rel}\n")
        d = os.path.dirname(path)
        self.stack.append(path)
        in_comment = False
        conds = []  # per #if level: True = inside "#if 0" (dead example code in stb)
        with open(path, encoding="utf-8") as f:
            for line in f:
                starts_in_comment = in_comment
                in_comment = comment_state(line, in_comment)
                pp = PP_RE.match(line) if not starts_in_comment else None
                if pp:
                    kw, rest = pp.group(1), pp.group(2).strip()
                    if kw in ("if", "ifdef", "ifndef"):
                        conds.append(kw == "if" and rest.split("//")[0].strip() == "0")
                    elif kw in ("else", "elif") and conds and conds[-1]:
                        conds[-1] = False
                    elif kw == "endif" and conds:
                        conds.pop()
                if starts_in_comment or any(conds):
                    out.append(line)
                    continue
                if ONCE_RE.match(line):
                    continue
                m = IMPL_RE.match(line)
                if m:  # at most one implementation per library, decided by the preprocessor (it may sit in an #if)
                    out.append(f"#ifndef TG_AMALGAM_DONE_{m.group(1)}\n{line.rstrip()}\n#endif\n")
                    self.impl_done.add(m.group(1))
                    continue
                m = INC_RE.match(line)
                if m:
                    target = self.resolve(m.group(1), d)
                    if target:
                        self.expand(target, out)
                        if target.startswith(THIRD_PARTY):  # one implementation per library: later includes declare only
                            macro = os.path.splitext(os.path.basename(target))[0].upper() + "_IMPLEMENTATION"
                            if macro in self.impl_done:
                                out.append(f"#ifdef {macro}\n#define TG_AMALGAM_DONE_{macro}\n#undef {macro}\n#endif\n")
                        continue
                out.append(line)
        self.stack.pop()
        if not out[-1].endswith("\n"):
            out.append("\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-o", "--output", default=os.path.join(ROOT, "build", "tg_single.h"))
    a = ap.parse_args()

    A = Amalgam()
    out = []
    version = re.search(r"project\( tg VERSION ([0-9.]+)", open(os.path.join(ROOT, "CMakeLists.txt"), encoding="utf-8").read())
    out.append("// tg_single.h - tg %s as a single header. Generated by tools/amalgamate.py; don't edit.\n" % (version.group(1) if version else ""))
    out.append("//\n//   #define TG_IMPLEMENTATION   (in exactly one .cpp)\n//   #define TG_BACKEND_SOFT / TG_BACKEND_GLFW_GL3 / TG_BACKEND_WIN32, and TG_APP for tg::App\n")
    out.append("//   #include \"tg_single.h\"\n//\n// Licenses of the bundled code (stb: MIT / public domain, nanosvg: zlib, fonts: OFL / ISC) are in the tg repository.\n")
    out.append("#pragma once\n#ifndef TG_SINGLE_HEADER\n#define TG_SINGLE_HEADER 1\n")
    # public API
    A.expand(os.path.join(ROOT, "include", "tg", "tg.h"), out)
    A.expand(os.path.join(ROOT, "include", "tg", "app.h"), out)
    for f in sorted(os.listdir(os.path.join(ROOT, "include", "tg"))):
        if f.endswith(".h") and f != "webview.h":
            A.expand(os.path.join(ROOT, "include", "tg", f), out)
    for macro, headers, _ in BACKENDS:
        out.append(f"#if defined( {macro} )\n")
        for h in headers:
            A.expand(os.path.join(ROOT, h), out)
        out.append(f"#endif // {macro}\n")
    out.append("#endif // TG_SINGLE_HEADER\n\n")

    # implementation
    out.append("#if defined( TG_IMPLEMENTATION ) && !defined( TG_IMPLEMENTATION_DONE )\n#define TG_IMPLEMENTATION_DONE 1\n")
    for src in core_sources():
        A.expand(os.path.join(ROOT, src), out)
    for macro, _, sources in BACKENDS:
        out.append(f"#if defined( {macro} )\n")
        for s in sources:
            A.expand(os.path.join(ROOT, s), out)
        out.append(f"#endif // {macro}\n")
    out.append("#if defined( TG_APP )\n")
    A.expand(os.path.join(ROOT, "backends", "app_common.h"), out)
    out.append("#if defined( TG_BACKEND_WIN32 )\n")
    A.expand(os.path.join(ROOT, "backends", "app_win32.cpp"), out)
    out.append("#elif defined( TG_BACKEND_GLFW_GL3 )\n")
    A.expand(os.path.join(ROOT, "backends", "app.cpp"), out)
    out.append("#endif\n#endif // TG_APP\n")
    out.append("#endif // TG_IMPLEMENTATION\n")

    os.makedirs(os.path.dirname(os.path.abspath(a.output)), exist_ok=True)
    with open(a.output, "w", encoding="utf-8", newline="\n") as f:
        f.writelines(out)
    size = os.path.getsize(a.output)
    print(f"wrote {a.output} ({size / 1e6:.1f} MB)")


if __name__ == "__main__":
    sys.exit(main())
