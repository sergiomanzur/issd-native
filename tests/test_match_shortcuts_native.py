"""Retail exhibition constructor and full-snapshot shortcuts, isolated storage."""
from pathlib import Path
import hashlib
import os
import re
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build/ISSDNative.exe"
ROM = ROOT / "International Superstar Soccer Deluxe (USA).sfc"
pytestmark = pytest.mark.skipif(not EXE.exists() or not ROM.exists(),
                                reason="requires native build and user's retail ROM")
LIVE = (0x1E5A, 0x1E54, 0x1E4A, 0x1E50, 0x1E52, 0x1E64)
MENU = (0x1F88, 0x1F9C, 0x1F92, 0x1F98, 0x1F9A, 0x1FE0)


def word(ram, address):
    return int.from_bytes(ram[address:address + 2], "little")


def run(folder, frames, settings="", actions=(), auto=True):
    folder.mkdir(parents=True, exist_ok=True)
    cfg = folder / "isolated.cfg"
    cfg.write_text("internal_res=0\ntrue_widescreen=0\nengine_mode=0\n" + settings,
                   encoding="ascii")
    command = [str(EXE), "--rom", str(ROM), "--headless", str(frames),
               "--config", str(cfg), "--save-dir", str(folder / "owned-saves"),
               "--mods-dir", str(folder / "empty-mods"),
               "--dump-state", str(folder / "final")]
    entries = {}
    if auto:
        # Timed A/Start mashing can land in Scenario mode ($1648=$007D).
        # Explicitly confirm the original default Exhibition item instead.
        for frame in range(60, 361, 60):
            entries.update({frame: "START", frame + 10: "NONE"})
        entries.update({560: "A", 570: "NONE"})
        for frame in range(800, 3000, 150):
            entries.update({frame: "A", frame + 10: "NONE"})
        entries[3000] = "NONE"
    # Re-entering the original introduction includes its coin-toss confirm.
    for frame, name in actions:
        if name in ("play-favorite", "start-rules"):
            for offset in range(150, 1501, 150):
                entries.update({frame + offset: "A", frame + offset + 10: "NONE"})
    if entries:
        script = "".join(f"{frame} {button}\n" for frame, button in sorted(entries.items()))
        path = folder / "input.txt"
        path.write_text(script, encoding="ascii")
        command += ["--script", str(path)]
    for frame, name in actions:
        command += ["--match-action", f"{frame}:{name}"]
    result = subprocess.run(command, cwd=folder,
                            env=dict(os.environ, SDL_AUDIODRIVER="dummy", SDL_VIDEODRIVER="dummy"),
                            capture_output=True, text=True, timeout=120)
    return result, (folder / "final.wram").read_bytes() if (folder / "final.wram").exists() else None


def successful(result):
    assert result.returncode == 0, result.stdout + result.stderr


def captures(result):
    setup = re.findall(r"\[Match\] setup frame=(\d+)", result.stderr)
    kickoff = re.findall(r"\[Match\] kickoff frame=(\d+)", result.stderr)
    assert setup, "No healthy original preconstructor boundary witnessed\n" + result.stderr
    assert kickoff, "Original constructor did not reach healthy kickoff\n" + result.stderr
    return int(setup[0]), int(kickoff[0])


def action_succeeded(result, frame, name):
    assert f"[MatchAction] frame={frame} action={name}" in result.stderr, result.stderr


@pytest.fixture(scope="module")
def exhibition(tmp_path_factory):
    folder = tmp_path_factory.mktemp("retail-exhibition")
    result, ram = run(folder, 4800)
    successful(result)
    setup, kickoff = captures(result)
    assert setup < kickoff < 4500
    # $1648 is player memory during live play; CODE_85B947 backs its flags
    # up at $DE07 before the original actor constructors reuse it.
    assert word(ram, 0xDE07) == 0 and word(ram, 0x32) == 6
    return setup, kickoff


@pytest.mark.parametrize("preset, settings, expected", [
    (0, "", (1, 2, 0, 0, 0, 1)),
    (1, "", (1, 2, 0, 0, 0, 1)),
    (2, "", (0, 0, 1, 1, 1, 1)),
    (3, "match_duration=2\nmatch_difficulty=4\nmatch_offside=1\nmatch_fouls=0\nmatch_cards=1\nmatch_extra_time=0\n", (2, 4, 1, 0, 1, 0)),
], ids=["original", "classic", "casual", "custom"])
def test_original_constructor_applies_presets(tmp_path, exhibition, preset, settings, expected):
    setup, kickoff = exhibition
    settings = f"match_preset={preset}\n" + settings
    boundary, ram = run(tmp_path / "boundary", setup + 1, settings)
    successful(boundary)
    assert re.search(r"\[Match\] setup frame=\d+", boundary.stderr), boundary.stderr
    assert word(ram, 0x70) == 15 and word(ram, 0x72) == 0
    assert tuple(word(ram, a) for a in LIVE) == expected
    assert tuple(word(ram, a) for a in MENU) == expected
    result, ram = run(tmp_path / "kickoff", kickoff + 2, settings)
    successful(result)
    captures(result)
    assert word(ram, 0x70) == 8
    assert tuple(word(ram, a) for a in LIVE) == expected
    assert ram[0x16D1] == (0x03, 0x05, 0x07)[expected[0]], "Clock constructor used stale duration"
    assert word(ram, 0xEA4) & 2, "Fixture no longer exercises the original CPU opponent"
    assert word(ram, 0xECC) == expected[1], "Actor constructor used stale difficulty"


def test_rematch_preserves_complete_exhibition_setup(tmp_path, exhibition):
    _, kickoff = exhibition
    # Capture logs use the just-run frame index; its state is frame + 1.
    baseline, original = run(tmp_path / "baseline", kickoff + 32)
    successful(baseline)
    frame = kickoff + 100
    result, restored = run(tmp_path / "rematch", frame + 31, actions=[(frame, "rematch")])
    successful(result)
    action_succeeded(result, frame, "rematch")
    retained = (0xDA0, 0xEA0, 0xDA4, 0xEA4, 0x11E6, 0x1E4C, 0x1E58, 0x1E5C, 0x1FA2, *LIVE)
    assert [word(restored, a) for a in retained] == [word(original, a) for a in retained]
    assert restored == original, "Full guest state did not replay identically after kickoff restore"
    assert not list((tmp_path / "rematch/owned-saves").rglob("campaign.sav"))


def test_drill_replays_same_point_independent_of_rematch(tmp_path, exhibition):
    _, kickoff = exhibition
    mark, restart = kickoff + 40, kickoff + 110
    baseline, original = run(tmp_path / "baseline", mark + 21, actions=[(mark, "mark-drill")])
    successful(baseline)
    result, restored = run(tmp_path / "drill", restart + 21,
                           actions=[(mark, "mark-drill"), (kickoff + 75, "rematch"),
                                    (restart, "restart-drill")])
    successful(result)
    for frame, name in [(mark, "mark-drill"), (kickoff + 75, "rematch"), (restart, "restart-drill")]:
        action_succeeded(result, frame, name)
    assert restored == original, "Marked drill did not preserve deterministic guest replay"


def test_favorite_survives_process_restart_and_rejects_changed_ai(tmp_path, exhibition):
    _, kickoff = exhibition
    frame = kickoff + 20
    saved, _ = run(tmp_path, frame + 1, "match_preset=2\n", [(frame, "save-favorite")])
    successful(saved)
    action_succeeded(saved, frame, "save-favorite")
    files = list((tmp_path / "owned-saves").rglob("*.sav"))
    assert len(files) == 1 and files[0].name != "quicksave.sav", files
    favorite = files[0]
    digest = hashlib.sha256(favorite.read_bytes()).digest()
    played, ram = run(tmp_path, 1800, "match_preset=1\n", [(1, "play-favorite")], auto=False)
    successful(played)
    action_succeeded(played, 1, "play-favorite")
    assert tuple(word(ram, a) for a in LIVE) == (0, 0, 1, 1, 1, 1)
    assert word(ram, 0xDE07) == 0 and word(ram, 0x70) == 8
    rejected, _ = run(tmp_path, 2, "gameplay_goalkeeper_ai=1\n", [(1, "play-favorite")], auto=False)
    assert rejected.returncode != 0, rejected.stdout + rejected.stderr
    assert "compatib" in (rejected.stdout + rejected.stderr).lower()
    assert hashlib.sha256(favorite.read_bytes()).digest() == digest


def test_start_rules_runs_original_constructor_again(tmp_path, exhibition):
    _, kickoff = exhibition
    frame = kickoff + 40
    result, ram = run(tmp_path, frame + 1800, "match_preset=3\nmatch_duration=2\nmatch_difficulty=4\n",
                      [(frame, "start-rules")])
    successful(result)
    action_succeeded(result, frame, "start-rules")
    assert len(re.findall(r"\[Match\] kickoff frame=\d+", result.stderr)) == 2, result.stderr
    assert word(ram, 0x1E5A) == 2 and word(ram, 0x1E54) == 4


def test_pause_restart_uses_same_full_kickoff_as_rematch(tmp_path, exhibition):
    _, kickoff = exhibition
    baseline, original = run(tmp_path / "baseline", kickoff + 22)
    successful(baseline)
    frame = kickoff + 100
    result, restored = run(tmp_path / "restart", frame + 21,
                           actions=[(frame, "restart-match")])
    successful(result)
    action_succeeded(result, frame, "restart-match")
    assert restored == original, "Pause restart did not replay the authentic kickoff"


def test_pause_back_returns_authentic_main_menu(tmp_path, exhibition):
    _, kickoff = exhibition
    frame = kickoff + 40
    result, ram = run(tmp_path / "back", frame + 31,
                      actions=[(frame, "back-main")])
    successful(result)
    action_succeeded(result, frame, "back-main")
    assert word(ram, 0x32) == 6 and word(ram, 0x70) == 12
    assert word(ram, 0x1538) == 0x9D72 and word(ram, 0x153A) == 0xA4
    assert word(ram, 0x50) == 0, "Match actors remained enabled at the menu"


@pytest.mark.parametrize("world", [False, True], ids=["cup", "world-series"])
@pytest.mark.parametrize("action", ["restart-match", "back-main"])
def test_campaign_pause_actions_keep_single_autosave(tmp_path, world, action):
    script = "".join(f"{f} START\n{f+10} NONE\n" for f in range(60, 361, 60))
    script += "500 DOWN\n510 NONE\n"
    if world:
        script += "530 DOWN\n540 NONE\n"
    script += "560 A\n570 NONE\n"
    script += "".join(f"{f} A\n{f+10} NONE\n" for f in range(800, 1600, 100))
    script += "".join(f"{f} A\n{f+10} NONE\n" for f in range(1800, 2900, 200))
    input_path = tmp_path / "input.txt"
    input_path.write_text(script, encoding="ascii")
    result = subprocess.run(
        [str(EXE), "--rom", str(ROM), "--headless", "4021",
         "--script", str(input_path), "--config", str(tmp_path / "isolated.cfg"),
         "--save-dir", str(tmp_path / "owned-saves"),
         "--mods-dir", str(tmp_path / "empty-mods"),
         "--match-action", f"4000:{action}", "--dump-state", str(tmp_path / "final")],
        cwd=tmp_path, env=dict(os.environ, SDL_AUDIODRIVER="dummy", SDL_VIDEODRIVER="dummy"),
        capture_output=True, text=True, timeout=120)
    successful(result)
    action_succeeded(result, 4000, action)
    ram = (tmp_path / "final.wram").read_bytes()
    saves = list((tmp_path / "owned-saves/campaigns").glob("*/campaign.sav"))
    assert len(saves) == 1
    data = saves[0].read_bytes()
    assert int.from_bytes(data[24:32], "little") == 1, "Pause action duplicated campaign progress"
    if action == "restart-match":
        assert word(ram, 0x70) == 8
        assert word(ram, 0xDE07) & 0x24 == (0x20 if world else 4)
        assert word(ram, 0xDA2) == 0 and word(ram, 0xEA2) == 0
    else:
        assert word(ram, 0x70) == 12 and word(ram, 0x1538) == 0x9D72


@pytest.mark.parametrize("load", ["continue", "manual"], ids=["fresh-continue", "fresh-slot-load"])
def test_pause_back_without_resident_menu_keeps_campaign_bytes(tmp_path, load):
    script = "".join(f"{f} START\n{f+10} NONE\n" for f in range(60, 361, 60))
    script += "500 DOWN\n510 NONE\n560 A\n570 NONE\n"
    script += "".join(f"{f} A\n{f+10} NONE\n" for f in range(800, 1300, 100))
    input_path = tmp_path / "input.txt"
    input_path.write_text(script, encoding="ascii")
    command = [str(EXE), "--rom", str(ROM), "--config", str(tmp_path / "isolated.cfg"),
               "--save-dir", str(tmp_path / "owned-saves"),
               "--mods-dir", str(tmp_path / "empty-mods")]
    env = dict(os.environ, SDL_AUDIODRIVER="dummy", SDL_VIDEODRIVER="dummy")
    saved = subprocess.run(command + ["--headless", "1302", "--script", str(input_path),
                                     "--save-state", "1302"], cwd=tmp_path, env=env,
                           capture_output=True, text=True, timeout=120)
    successful(saved)
    campaigns = list((tmp_path / "owned-saves/campaigns").glob("*/campaign.sav"))
    assert len(campaigns) == 1
    original = campaigns[0].read_bytes()
    load_args = ["--continue"] if load == "continue" else ["--load-state", "1"]
    result = subprocess.run(command + ["--headless", "950", *load_args,
                                      "--match-action", ("0:back-main" if load == "continue" else "1:back-main"),
                                      "--dump-state", str(tmp_path / "returned")],
                            cwd=tmp_path, env=env, capture_output=True, text=True, timeout=120)
    successful(result)
    assert "[MainMenuReturn] ready" in result.stderr, result.stderr
    assert "[MainMenuReturn] timed out" not in result.stderr
    ram = (tmp_path / "returned.wram").read_bytes()
    assert word(ram, 0x32) == 6 and word(ram, 0x70) == 12
    assert word(ram, 0x1538) == 0x9D72 and word(ram, 0x153A) == 0xA4
    assert campaigns[0].read_bytes() == original, "Returning from fresh load rewrote campaign autosave"
