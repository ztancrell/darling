# Read memory and name addresses in a Darling guest's ELF core.
# Memory comes from the core's PT_LOAD segments, falling back to the mapped file (NT_FILE note)
# for pages the kernel did not dump. Symbols come from llvm-nm on the mapped Mach-O.
import re, struct, subprocess, bisect, functools, os

ARCH = "x86_64"

def run(*a):
    return subprocess.run(a, capture_output=True, text=True).stdout

class Core:
    def __init__(self, path):
        self.path = path
        self.f = open(path, "rb")
        self.loads = []          # (vaddr, filesz, offset)
        for l in run("readelf", "-lW", path).splitlines():
            p = l.split()
            if p and p[0] == "LOAD":
                self.loads.append((int(p[2], 16), int(p[4], 16), int(p[1], 16)))
        self.maps = []           # (start, end, file page offset, path)
        lines = run("readelf", "-n", path).splitlines()
        psz = 4096
        for i, l in enumerate(lines):
            m = re.match(r"\s+Page size: (\d+)", l)
            if m: psz = int(m[1])
            m = re.match(r"\s+(0x[0-9a-f]+)\s+(0x[0-9a-f]+)\s+(0x[0-9a-f]+)\s*$", l)
            if m and i + 1 < len(lines):
                self.maps.append((int(m[1], 16), int(m[2], 16), int(m[3], 16) * psz, lines[i + 1].strip()))
        self.maps.sort()

    def mapping(self, addr):
        for s, e, off, path in self.maps:
            if s <= addr < e:
                return s, e, off, path
        return None

    def read(self, addr, n):
        for v, fsz, off in self.loads:
            if v <= addr < v + fsz:
                self.f.seek(off + addr - v)
                return self.f.read(min(n, v + fsz - addr))
        m = self.mapping(addr)
        if m and os.path.exists(m[3]):
            with open(m[3], "rb") as g:
                g.seek(m[2] + addr - m[0])
                return g.read(n)
        return None

    def q(self, addr):
        b = self.read(addr, 8)
        return struct.unpack("<Q", b)[0] if b and len(b) == 8 else None

    def cstr(self, addr, limit=200):
        b = self.read(addr, limit)
        return None if b is None else b.split(b"\0")[0].decode("latin1")

    def base(self, path):
        return min(s for s, e, off, p in self.maps if p == path)

    def name(self, addr):
        """'lib`symbol + off' for an address, or None."""
        m = self.mapping(addr)
        if not m:
            return None
        path = m[3]
        rel = addr - self.base(path)
        short = os.path.basename(path)
        addrs, names, text0 = symbols(path)
        if not addrs:
            return f"{short} + {rel:#x}"
        va = rel + text0
        i = bisect.bisect_right(addrs, va) - 1
        if i < 0:
            return f"{short} + {rel:#x}"
        return f"{short}`{names[i]} + {va - addrs[i]:#x}"

@functools.lru_cache(maxsize=None)
def symbols(path):
    """Sorted symbol addresses/names for the x86_64 slice, and the slice's lowest segment vmaddr."""
    if not os.path.exists(path):
        return [], [], 0
    out = run("llvm-otool", "-l", path)
    parts = re.split(r"^\S.*\(architecture (\S+)\):$", out, flags=re.M)
    if len(parts) > 1:
        out = next((parts[i + 1] for i in range(1, len(parts), 2) if parts[i] == ARCH), "")
    segs = [int(v, 16) for n, v in re.findall(r"segname (\S+)\n\s+vmaddr (0x[0-9a-f]+)", out) if n != "__PAGEZERO"]
    text0 = min(segs) if segs else 0
    pairs = []
    for l in run("llvm-nm", f"--arch={ARCH}", "-n", path).splitlines():
        p = l.split(" ", 2)
        if len(p) == 3 and p[1] not in "Uu" and p[0].strip():
            try: pairs.append((int(p[0], 16), p[2]))
            except ValueError: pass
    pairs += breakpad_functions(path)
    pairs.sort()
    return [a for a, _ in pairs], [n for _, n in pairs], text0

BREAKPAD = os.path.expanduser("~/.local/share/darling/macos-apps/electron-22.3.27/symbols/breakpad_symbols")

def breakpad_functions(path):
    """(address, name) of every function in Electron's published symbol file, when this image is that build.
    The file is matched by the image's LC_UUID, so a differently built Electron Framework gets nothing."""
    name = os.path.basename(path)
    uuid = re.search(r"uuid ([0-9A-F-]+)", run("llvm-objdump", "--macho", "--private-headers", f"--arch={ARCH}", path))
    sym = uuid and os.path.join(BREAKPAD, name, uuid[1].replace("-", "") + "0", name + ".sym")
    if not sym or not os.path.exists(sym):
        return []
    cache = sym + ".funcs"
    if not os.path.exists(cache):   # FUNC [m] <address> <size> <parameter size> <name>
        with open(sym, errors="replace") as f, open(cache, "w") as out:
            for l in f:
                if l.startswith("FUNC "):
                    p = l.split(" ", 5 if l.startswith("FUNC m ") else 4)
                    out.write(f"{p[-4]} {p[-1]}")
    pairs = []
    for l in open(cache):
        a, n = l.split(" ", 1)
        pairs.append((int(a, 16), n.strip()[:160]))
    return pairs
