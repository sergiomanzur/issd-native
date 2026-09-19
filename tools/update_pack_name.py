import json
for path in ['mods/liga_mx_expansion.json', 'build/mods/liga_mx_expansion.json']:
    with open(path, 'r', encoding='utf-8') as f:
        d = json.load(f)
    d['name'] = 'Liga MX y Expansion MX'
    with open(path, 'w', encoding='utf-8') as f:
        json.dump(d, f, indent=2, ensure_ascii=False)
    print(f'Updated {path}')
