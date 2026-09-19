for path in ['issd_native.cfg', 'build/issd_native.cfg']:
    with open(path, 'r', encoding='utf-8', errors='ignore') as f:
        lines = f.readlines()
    new_lines = []
    for l in lines:
        if l.startswith('active_mod_packs='):
            new_lines.append('active_mod_packs=Liga MX y Expansion MX\n')
        else:
            new_lines.append(l)
    with open(path, 'w', encoding='utf-8') as f:
        f.writelines(new_lines)
    print(f'Updated {path}')
