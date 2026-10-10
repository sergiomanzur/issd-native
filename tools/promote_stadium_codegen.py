"""Promote regenerated functions only when their sole changes are stadium policies.

Full regeneration discovers unrelated width variants. This audit preserves the
existing translation units and dispatch table: a regenerated function is eligible
only if removing the explicit operand reads/opcode callbacks reproduces its
existing body byte-for-byte. Never synthesize or hand-patch instruction bodies.
"""
from pathlib import Path
import argparse
import json
import re

IMMEDIATES = {0x038b27,0x038e1a,0x038eb9,0x038ecc,0x038ed6,0x038edb,
              0x038c11,0x0b82b0}
HOOKS = {0x00b909,0x00b90d,0x008db8,0x0b8cec,0x0b8dc0,0x0b85e3,0x0b86e9}
SIGNATURE = re.compile(r'^RecompReturn (\w+)\(CpuState \*cpu\) \{', re.M)
READ = re.compile(r'cpu_read(8|16)\(cpu, 0x([0-9a-f]+), \(uint16\)\(0x([0-9a-f]+)\)\)')
CALLBACK = re.compile(r'^\s*if \(g_cpu_native_block_hook\) g_cpu_native_block_hook\(cpu, 0x([0-9A-F]+)\);\n', re.M)


def functions(source):
    result = {}
    for match in SIGNATURE.finditer(source):
        position, depth = match.end(), 1
        while depth:
            # Generated comments may contain balanced examples of braces.
            if source.startswith('/*', position):
                position = source.index('*/', position+2)+2
                continue
            if source.startswith('//', position):
                position = source.index('\n', position)+1
                continue
            if source[position] in ('"', "'"):
                quote = source[position]
                position += 1
                while source[position] != quote:
                    position += 2 if source[position] == '\\' else 1
                position += 1
                continue
            depth += (source[position] == '{') - (source[position] == '}')
            position += 1
        result[match.group(1)] = (match.start(), position, source[match.start():position])
    return result


def audit(existing, regenerated, rom):
    regenerated_functions = {}
    for path in regenerated.glob('bank*.c'):
        if path.name[:6] in ('bank00', 'bank03', 'bank0b'):
            regenerated_functions.update(functions(path.read_text()))
    reports, replacements = [], {}
    for path in existing.glob('bank*.c'):
        if path.name[:6] not in ('bank00', 'bank03', 'bank0b'):
            continue
        source = path.read_text()
        edits = []
        for name, (start, end, old) in functions(source).items():
            if name not in regenerated_functions:
                continue
            new = regenerated_functions[name][2]
            sites = set()

            def operand(match):
                pc = ((int(match[2],16)<<16) | int(match[3],16))-1
                if pc & 0x7fffff not in IMMEDIATES:
                    return match[0]
                offset = ((pc>>16)&127)*0x8000 + ((pc+1)&0x7fff)
                width = 1 if match[1] == '8' else 2
                sites.add(pc & 0x7fffff)
                return hex(int.from_bytes(rom[offset:offset+width], 'little'))

            def callback(match):
                pc = int(match[1],16) & 0x7fffff
                if pc not in HOOKS:
                    return match[0]
                sites.add(pc)
                return ''

            normalized = CALLBACK.sub(callback, READ.sub(operand, new))
            if not sites or new == old:
                continue
            accepted = normalized == old
            reports.append({'file': path.name, 'function': name,
                            'sites': [f'{pc:06x}' for pc in sorted(sites)],
                            'eligible': accepted})
            if accepted:
                edits.append((start, end, new))
        if edits:
            for start, end, new in sorted(edits, reverse=True):
                source = source[:start]+new+source[end:]
            replacements[path] = source
    return reports, replacements


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--existing', type=Path, required=True)
    parser.add_argument('--regenerated', type=Path, required=True)
    parser.add_argument('--rom', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    report, edits = audit(args.existing, args.regenerated, args.rom.read_bytes())
    args.report.write_text(json.dumps(report, indent=2))
    print(f'{sum(item["eligible"] for item in report)} eligible regenerated '
          f'functions across {len(edits)} existing units; '
          f'{sum(not item["eligible"] for item in report)} require further audit')
    if args.apply:
        for filename, source in edits.items():
            filename.write_text(source, newline='\n')
