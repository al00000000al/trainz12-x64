"""Lift a straight-line block of x86 SSE scalar code (dumpbin /disasm output) into C++ statements in SSA form.

Used for reconstructing float math kernels exactly: every SSE operation becomes one `const float tN = a op b;`
in program order, so the C++ (compiled with /fp:precise, no FMA) performs the same single-precision operations
in the same order and gives the same bits. Loads from the object or argument pointers become reads of named
arrays, stores become writes, in program order, so a later load sees an earlier store as in the original.

Supported: movss/movaps (reg, reg|mem; mem, reg), mulss/addss/subss/divss (reg, reg|mem), xorps r,r (zero),
mov r32,[esp+N] (pointer arguments), mov r32,r32, lea r32,[esp+N], mov r32,[mem] / mov [mem],r32 (raw float
copies), push/pop/sub esp/add esp, mov [mem],imm. Anything else stops the lift with an error.

usage: python sse_lift.py <asm.txt> <first addr> <last addr> <arg names, e.g. "this"> [init...]
  The block runs from <first addr> up to (not including) <last addr>. Argument k (k = 0 at [esp+4] on entry)
  gets the k-th name; [name+off] is written as name[off/4]. Constants ds:[addr] become K_<addr> and are listed.
  init items set the state at <first addr> for a block inside a function:
    esp=-48      esp relative to the entry esp (hex)
    ecx=*this    the register points at the array `this`
    xmm7=ca      the register holds the value `ca`
    s4=sb        the stack slot at entry esp + 4 holds `sb` (s-48: entry esp - 0x48)
"""
import re
import sys

asm_path, first, last, names = sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3], 16), sys.argv[4].split(",")
inits = [a for a in sys.argv[5:] if not a.startswith("--")]
show_stack = "--stack" in sys.argv[5:]
as_expr = "--expr" in sys.argv[5:]

lines = []
for raw in open(asm_path, encoding="utf-8", errors="replace"):
    m = re.match(r"\s*([0-9A-F]{8}):?\s+(?:[0-9A-F]{2} )*\s*([a-z].*)$", raw.rstrip())
    if not m:
        m = re.match(r"\s*([0-9A-F]{8})\s+([a-z].*)$", raw.rstrip())
    if not m:
        continue
    addr = int(m.group(1), 16)
    if first <= addr < last:
        lines.append((addr, re.sub(r"\s+", " ", m.group(2).strip())))

esp = 0          # offset from the entry esp (the return address is at 0, argument k at 4 + 4k)
regs32 = {}      # r32 -> ("esp", off) | ("arg", k) | ("val", expr)
xmm = {}         # xmm reg -> expression name
stack = {}       # esp offset -> expression name (floats spilled to the stack)
out = []
consts = set()
counter = [0]
for item in inits:
    key, val = item.split("=")
    if key == "esp":
        esp = -int(val[1:], 16) if val.startswith("-") else int(val, 16)
    elif key.startswith("xmm"):
        xmm[key] = val
    elif key.startswith("s"):
        stack[-int(key[2:], 16) if key[1] == "-" else int(key[1:], 16)] = val
    elif val.startswith("*"):
        regs32[key] = ("ptr", val[1:], 0)
    else:
        regs32[key] = ("val", val)


def new(expr):
    counter[0] += 1
    name = f"t{counter[0]}"
    out.append(f"const float {name} = {expr};")
    return name


def parse_mem(op):
    m = re.match(r"(?:dword|mmword|qword) ptr (?:ds:)?\[(.*)\]", op)
    if not m:
        return None
    inner = m.group(1).replace("h", "")
    if re.fullmatch(r"[0-9A-F]+", inner):
        return ("const", int(inner, 16))
    m2 = re.fullmatch(r"([a-z]{3})(?:([+-])([0-9A-F]+))?", inner)
    if not m2:
        raise SystemExit(f"unsupported address {op}")
    base, sign, off = m2.group(1), m2.group(2), m2.group(3)
    off = int(off, 16) if off else 0
    if sign == "-":
        off = -off
    if base == "esp":
        return ("stack", esp + off)
    kind = regs32.get(base)
    if kind is None:
        raise SystemExit(f"unknown base register {base}")
    if kind[0] == "esp":
        return ("stack", kind[1] + off)
    if kind[0] == "arg":
        return ("obj", names[kind[1]], off)
    if kind[0] == "ptr":
        return ("obj", kind[1], kind[2] + off)
    raise SystemExit(f"base {base} holds a value, not a pointer")


def load(op):
    if op.startswith("xmm"):
        return xmm[op]
    mem = parse_mem(op)
    if mem[0] == "const":
        consts.add(mem[1])
        return f"K_{mem[1]:08X}"
    if mem[0] == "stack":
        off = mem[1]
        if off >= 4 and off not in stack:  # an incoming float argument
            return names[(off - 4) // 4]
        return stack[off]
    _, arr, off = mem
    return new(f"{arr}[{off // 4}]")


def store(op, value):
    mem = parse_mem(op)
    if mem[0] == "stack":
        stack[mem[1]] = value
        if show_stack:  # entry esp - 0x100 + 4k is written as S[k]
            out.append(f"S[{(mem[1] + 0x100) // 4}] = {value};")
    elif mem[0] == "obj":
        out.append(f"{mem[1]}[{mem[2] // 4}] = {value};")
    else:
        raise SystemExit(f"store to a constant: {op}")


ops = {"mulss": "*", "addss": "+", "subss": "-", "divss": "/"}
for addr, text in lines:
    mnem, _, rest = text.partition(" ")
    args = [a.strip() for a in rest.split(",")] if rest else []
    if mnem in ("movss", "movaps"):
        dst, src = args
        if dst.startswith("xmm"):
            xmm[dst] = load(src)
        else:
            store(dst, xmm[src])
    elif mnem in ops:
        dst, src = args
        xmm[dst] = new(f"{xmm[dst]} {ops[mnem]} {load(src)}")
    elif mnem == "xorps":
        if args[0] != args[1]:
            raise SystemExit(f"{addr:08X}: xorps of different registers")
        xmm[args[0]] = "0.0f"
    elif mnem == "push":
        esp -= 4
        if args[0] in regs32 and regs32[args[0]][0] == "val":
            stack[esp] = regs32[args[0]][1]
    elif mnem == "pop":
        esp += 4
    elif mnem == "sub" and args[0] == "esp":
        esp -= int(args[1].rstrip("h"), 16)
    elif mnem == "add" and args[0] == "esp":
        esp += int(args[1].rstrip("h"), 16)
    elif mnem in ("add", "sub") and regs32.get(args[0], ("",))[0] == "ptr" and re.fullmatch(r"[0-9A-F]+h?", args[1]):
        k = regs32[args[0]]
        step = int(args[1].rstrip("h"), 16)
        regs32[args[0]] = ("ptr", k[1], k[2] + (step if mnem == "add" else -step))
    elif mnem == "lea" and args[1].startswith("["):
        m = re.fullmatch(r"\[esp(?:\+([0-9A-F]+)h?)?\]", args[1])
        if not m:  # a pointer into an object
            m = re.fullmatch(r"\[(e[a-z]{2})(?:\+([0-9A-F]+)h?)?\]", args[1])
            kind = regs32.get(m.group(1)) if m else None
            if kind and kind[0] in ("arg", "ptr"):
                arr, base = (names[kind[1]], 0) if kind[0] == "arg" else (kind[1], kind[2])
                regs32[args[0]] = ("ptr", arr, base + int(m.group(2) or "0", 16))
            else:
                regs32.pop(args[0], None)
            continue
        regs32[args[0]] = ("esp", esp + int(m.group(1) or "0", 16))
    elif mnem == "mov":
        dst, src = args
        if re.fullmatch(r"e[a-z]{2}", dst):
            if re.fullmatch(r"e[a-z]{2}", src):
                regs32[dst] = regs32[src]
                continue
            mem = parse_mem(src)
            if mem and mem[0] == "stack" and mem[1] >= 4 and mem[1] not in stack:
                regs32[dst] = ("arg", (mem[1] - 4) // 4)  # a pointer argument
            elif mem:
                regs32[dst] = ("val", load(src))
            else:
                raise SystemExit(f"{addr:08X}: unsupported {text}")
        else:
            if src in regs32 and regs32[src][0] == "val":
                store(dst, regs32[src][1])
            elif re.fullmatch(r"[0-9A-F]+h?", src):
                v = int(src.rstrip("h"), 16)
                store(dst, "0.0f" if v == 0 else ("1.0f" if v == 0x3F800000 else f"BitsToFloat(0x{v:08X})"))
            else:
                raise SystemExit(f"{addr:08X}: unsupported {text}")
    elif mnem.startswith("f"):  # x87: not lifted, the memory operand's value is noted for the reader
        mem = parse_mem(args[-1]) if args else None
        note = load(args[-1]) if mem else ""
        out.append(f"// {text}  [{note}]")
    elif mnem in ("ret", "nop"):
        break
    else:
        raise SystemExit(f"{addr:08X}: unsupported instruction {text}")



def to_expressions(lines):
    """Inline the temporaries used once; loads of an array slot before any store to it become `<arr><index>`,
    loads after a store become the stored value. The parenthesisation keeps the order of every operation."""
    defs, uses, order = {}, {}, []
    alias = {}        # temp -> name it stands for (a load)
    current = {}      # "m[3]" -> the value now in that slot
    for line in lines:
        if line.startswith("//"):
            order.append(("note", line))
            continue
        m = re.fullmatch(r"const float (t\d+) = (.*);", line)
        if m:
            name, expr = m.groups()
            load = re.fullmatch(r"(\w+)\[(\d+)\]", expr)
            if load:
                alias[name] = current.get(expr, f"{load.group(1)}{load.group(2)}")
                continue
            parts = expr.split(" ")
            parts = [alias.get(x, x) for x in parts]
            for x in (parts[0], parts[2]):
                uses[x] = uses.get(x, 0) + 1
            defs[name] = parts
            order.append(("def", name))
            continue
        m = re.fullmatch(r"(\w+\[\d+\]) = (.*);", line)
        value = alias.get(m.group(2), m.group(2))
        uses[value] = uses.get(value, 0) + 1
        current[m.group(1)] = value
        order.append(("store", m.group(1), value))
    text = {}

    def render(x, top=False):
        if x in text:
            return text[x]
        return x

    result = []
    for item in order:
        if item[0] == "note":
            note = re.sub(r"\[(t\d+)\]$", lambda mm: "[" + render(alias.get(mm.group(1), mm.group(1))) + "]", item[1])
            result.append(note)
        elif item[0] == "def":
            name = item[1]
            a, op, b = defs[name]
            e = f"{render(a)} {op} {render(b)}"
            if uses.get(name, 0) == 1:
                text[name] = f"({e})"
            else:
                result.append(f"const float {name} = {e};")
        else:
            v = render(item[2])
            if v.startswith("(") and v.endswith(")"):
                v = v[1:-1]
            result.append(f"{item[1]} = {v};")
    return result


for line in (to_expressions(out) if as_expr else out):
    print("    " + line)
print("    // constants: " + ", ".join(f"K_{c:08X}" for c in sorted(consts)))
for r in sorted(xmm):
    print(f"    // {r} = {xmm[r]}")
