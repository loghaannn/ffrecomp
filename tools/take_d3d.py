#!/usr/bin/env python3
"""Check the FFR map against D3D.asm, the retail XBE and generated dispatch.
Reads existing pipeline outputs; does not extract or rewrite any functions.
"""
import argparse
import hashlib
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
XBE_SHA256 = 'f991fd72f1efed05bb80ac273b72782bb77ed933e80c0a750f26233fcc7ce0ff'


def verify(xbe, asm, generated):
    if hashlib.sha256(xbe).hexdigest() != XBE_SHA256:
        raise ValueError('expected Fatal Frame NTSC-U default.xbe')
    dispatch = (generated / 'recomp_dispatch.c').read_text()
    lifts = ''.join(p.read_text() for p in sorted(generated.glob('recomp_[0-9]*.c')))
    count = 0
    for line in (ROOT / 'tools/d3d_symbols.txt').read_text().splitlines():
        if not line.startswith('FFR_ENTRY'):
            continue
        start, end, name, words = re.fullmatch(
            r'FFR_ENTRY\(([0-9A-F]{8}), ([0-9A-F]{8}), (\w+), (\d+)\)', line).groups()
        body = asm.split(f'sub_{start}:\n', 1)[1].split('; end of function', 1)[0]
        pc = int(start, 16)
        returns = set()
        for addr, code, instruction in re.findall(r'^  0x(\w+)\s+([0-9a-f]+(?:\.\.)?)\s+([^\n]+)', body, re.M):
            # Section location in the pinned, digest-checked retail executable.
            offset = 0xd7000 + pc - 0xe6b80
            raw = bytes.fromhex(code.removesuffix('..'))
            if code.endswith('..'):
                # The disassembler truncates long encodings for display. Verify
                # the missing bytes by decoding the retail instruction itself.
                from capstone import Cs, CS_ARCH_X86, CS_MODE_32
                decoded = next(Cs(CS_ARCH_X86, CS_MODE_32).disasm(xbe[offset:offset + 15], pc, 1))
                if (not decoded.bytes.startswith(raw) or
                        instruction.split(';')[0].split() != (decoded.mnemonic + ' ' + decoded.op_str).split()):
                    raise ValueError(f'{name}: truncated instruction differs at {pc:08X}')
                raw = decoded.bytes
            if int(addr, 16) != pc or xbe[offset:offset + len(raw)] != raw:
                raise ValueError(f'{name}: disassembly differs from XBE at {pc:08X}')
            pc += len(raw)
            tokens = instruction.split(';', 1)[0].split()
            if tokens[0] == 'ret':
                returns.add(int(tokens[1], 0) if len(tokens) > 1 else 0)
        tail_cull = name == 'FrontFace' and 'jmp      0xe8520' in body and not returns
        if pc != int(end, 16) or (not tail_cull and returns != {4 * int(words)}):
            raise ValueError(f'{name}: boundary or stack cleanup mismatch')
        if f'(recomp_func_t)sub_{start} ' not in dispatch or f'void sub_{start}(void)\n{{' not in lifts:
            raise ValueError(f'{name}: missing lifted body or dispatch')
        count += 1
        print(f'{start}..{end}  {name}  stack {4 * int(words)} bytes')
    if not count:
        raise ValueError('empty function map')


def compare_sdk_signatures(xbe, asm, database):
    """List candidate pairs only. Uses the SDK's first-level masked hash, restricted
    to discovered D3D functions with equal length. Calls and layout may differ.
    """
    from collections import defaultdict
    import tomllib
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32, CS_GRP_BRANCH_RELATIVE

    if hashlib.sha256(xbe).hexdigest() != XBE_SHA256:
        raise ValueError('expected Fatal Frame NTSC-U default.xbe')
    by_shape = defaultdict(list)
    for name, sig in tomllib.loads(database.read_text())['symbols'].items():
        if sig.get('sec') == 'D3D' and 'hash' in sig:
            by_shape[sig['len'], int(sig['hash'], 16)].append(name)
    lengths = {length for length, _ in by_shape}
    decoder = Cs(CS_ARCH_X86, CS_MODE_32)
    decoder.detail = True
    for start, end in re.findall(r'; Start: 0x(\w+)  End: 0x(\w+)', asm):
        va = int(start, 16)
        length = int(end, 16) - va
        if length not in lengths:
            continue
        offset = 0xd7000 + va - 0xe6b80
        raw = xbe[offset:offset + length]
        masked = list(raw)
        decoded = 0
        for ins in decoder.disasm(raw, va):
            for size, field, relative in (
                    (ins.disp_size, ins.disp_offset, False),
                    (ins.imm_size, ins.imm_offset, ins.group(CS_GRP_BRANCH_RELATIVE))):
                at = decoded + field
                if size == 4 and (relative or 0x10000 <= int.from_bytes(raw[at:at + 4], 'little') < 0x4000000):
                    masked[at:at + 4] = [256] * 4
            decoded += ins.size
        if decoded != length:
            continue
        value = 0
        for token in masked:
            value = (value * 0x100000001b3 + token + 1) & 0xffffffffffffffff
        for name in by_shape.get((length, value), ()):
            print(f'candidate {start} {name}')


def inspect_inline_capture(directory, fifo_start, *, replay=False):
    """Inspect the first full-screen XYZRHW strip from an unwrapped initialization FIFO."""
    import mmap
    import struct
    from contextlib import ExitStack

    with ExitStack() as cleanup:
        images = {}
        for tag in ('pre', 'post'):
            for region in ('low', 'contig'):
                file = cleanup.enter_context((directory / f'{tag}_{region}.bin').open('rb'))
                images[tag, region] = cleanup.enter_context(mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ))

        def words(tag, address, count):
            region = 'contig' if address >= 0x80000000 else 'low'
            offset = address - (0x80000000 if region == 'contig' else 0)
            return struct.unpack_from(f'<{count}I', images[tag, region], offset)

        def pre(address, count=1):
            return words('pre', address, count)

        regs = dict(line.split(maxsplit=1) for line in (directory / 'pre_regs.txt').read_text().splitlines())
        caller, topology, count, source, stride = pre(int(regs['esp']), 5)
        if (caller, topology, count, stride) != (0xb53ab, 6, 4, 20):
            raise ValueError('expected the B53A6 four-vertex inline triangle strip')
        device, = pre(0xf9b80)
        before, = pre(device)
        end, = words('post', device, 1)
        if not fifo_start < before < end or (end - fifo_start) % 4:
            raise ValueError('expected a contiguous FIFO interval')
        prior = (before - fifo_start) // 4
        if pre(fifo_start, prior) != words('post', fifo_start, prior):
            raise ValueError('previous FIFO contents changed during draw')
        pc = fifo_start
        methods = {}
        payload = []
        object_id = None
        inside = finished = False
        program = []
        constants = {}
        load = None
        while pc < end:
            header, = words('post', pc, 1)
            n = (header >> 18) & 0x7ff
            # Linear method packets only. Refuse jumps, calls and ring wraps.
            if header & 0xa0030003 or pc + 4 + 4 * n > end:
                raise ValueError(f'unsupported FIFO packet at {pc:#x}')
            data = words('post', pc + 4, n)
            method = header & 0x1ffc
            if (header >> 13) & 7 == 0:
                if method == 0 and n == 1:
                    object_id = data[0]
                elif object_id == 0xd:  # startup subchannel-zero Kelvin object
                    if method == 0x17fc and pc >= before:
                        if data == (topology,) and not inside and not finished:
                            inside = True
                        elif inside and data == (0,):
                            inside, finished = False, True
                        else:
                            raise ValueError('unexpected BEGIN_END sequence')
                    elif inside:
                        if method != 0x1818 or not header & 0x40000000:
                            raise ValueError('unexpected inline primitive payload')
                        payload.extend(data)
                    elif not finished:
                        if method == 0x1ea4:
                            load = data[0] * 4
                        elif method == 0xb80:
                            if load is None:
                                raise ValueError('constant upload without load cursor')
                            constants.update(enumerate(data, load))
                            load += n
                        elif method == 0x1e9c:
                            if data == (0,):
                                program.clear()
                            else:
                                raise ValueError('unsupported program load origin')
                        elif method == 0xb00:
                            program.extend(data)
                        else:
                            for i, value in enumerate(data):
                                methods[method + (0 if header & 0x40000000 else 4*i)] = value
            pc += (n + 1) * 4

        if not finished or tuple(payload) != pre(source, count * stride // 4):
            raise ValueError('FIFO did not reproduce guest vertices')
        def surface(offset):
            handle, = pre(device + offset)
            return list(pre(handle, 5)) if handle else None

        result = dict(source=source, stride=stride, topology=topology, vertices=[
            struct.unpack('<4fI', struct.pack('<5I', *payload[i:i+5])) for i in range(0, len(payload), 5)],
            vertex_handle=pre(device+0x384)[0], pixel_handle=pre(device+0x370)[0],
            textures=pre(device+0xb38, 4), color=surface(0x21b4), depth=surface(0x21b8),
            program_words=len(program), program_sha256=hashlib.sha256(struct.pack(f'<{len(program)}I', *program)).hexdigest(),
            constant_words={hex(k): hex(v) for k,v in sorted(constants.items())},
            method_words={hex(k): hex(v) for k,v in sorted(methods.items())})
        if replay:
            result.update(program=program, payload=payload,
                          surfaces=pre(device + 0x21b4, 2))
        return result


def emit_wrapper_inputs(generated, destination):
    """Emit only affected translation units to a fresh build directory.
    Compile these in place of their same-named counterparts, alongside the
    FFR_D3D_WRAPPERS adapters. Calls and dispatch continue using public names.
    """
    manifest = (ROOT / 'tools/d3d_symbols.txt').read_text()
    wanted = set(re.findall(r'^FFR_ENTRY\(([0-9A-F]{8}),', manifest, re.M))
    seen = set()
    chunks = {}
    pattern = re.compile(r'^void sub_([0-9A-F]{8})\(void\)(?=\n\{)', re.M)

    def definition(match):
        va = match[1]
        if va not in wanted:
            return match[0]
        if va in seen:
            raise ValueError(f'duplicate lifted definition at {va}')
        seen.add(va)
        return f'void ffr_orig_{va}(void)'

    for path in sorted(generated.glob('recomp_[0-9]*.c')):
        original = path.read_text()
        renamed = pattern.sub(definition, original)
        if renamed != original:
            chunks[path.name] = renamed
    if seen != wanted:
        raise ValueError(f'missing lifted definitions: {sorted(wanted - seen)}')
    if destination.resolve().is_relative_to(generated.resolve()):
        raise ValueError('wrapper inputs must be outside the generated tree')
    destination.mkdir(exist_ok=False)
    for name, source in chunks.items():
        (destination / name).write_text(source)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('xbe', type=Path)
    parser.add_argument('asm', type=Path, help='tools/disasm/output/asm/D3D.asm')
    parser.add_argument('--generated', type=Path, default=ROOT / 'src/recomp/gen')
    parser.add_argument('--sdk-signatures', type=Path,
                        help='compare SDK signatures.toml and print unverified candidates')
    parser.add_argument('--inline-capture', type=Path,
                        help='inspect pre/post RECOMP_CAPTURE=0xEAB40 at caller B53A6')
    parser.add_argument('--fifo-start', type=lambda x: int(x, 0),
                        help='first FIFO packet VA in the captured initialization sequence')
    parser.add_argument('--wrapper-inputs', type=Path,
                        help='emit separately named original definitions into a fresh build directory')
    args = parser.parse_args()
    if (args.inline_capture is None) != (args.fifo_start is None):
        parser.error('--inline-capture and --fifo-start must be supplied together')
    xbe, asm = args.xbe.read_bytes(), args.asm.read_text()
    verify(xbe, asm, args.generated)
    if args.sdk_signatures:
        compare_sdk_signatures(xbe, asm, args.sdk_signatures)
    if args.inline_capture:
        import json
        print(json.dumps(inspect_inline_capture(args.inline_capture, args.fifo_start), indent=2))
    if args.wrapper_inputs:
        emit_wrapper_inputs(args.generated, args.wrapper_inputs)
