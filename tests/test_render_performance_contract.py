from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]


def test_nearest_and_linear_present_logical_texture_directly():
    main_c = (ROOT / "ISSDNative" / "main.c").read_text(encoding="utf-8")
    non_crt = re.search(
        r"if\s*\(\s*cur_filter\s*==\s*ISSD_FILTER_CRT\s*\)\s*\{.*?\}\s*else\s*\{(?P<body>.*?)\n\s*\}\s*\n\s*/\* Calculate",
        main_c,
        re.S,
    )
    assert non_crt, "render loop must have a distinct non-CRT presentation path"
    body = non_crt.group("body")

    assert "SDL_SetRenderTarget(renderer, texture)" not in body
    assert "SDL_RenderCopy(renderer, source_texture, NULL, NULL)" not in body
    assert "present_texture = source_texture;" in body
    assert "SDL_RenderCopy(renderer, present_texture, NULL, &dst_rect)" in main_c
