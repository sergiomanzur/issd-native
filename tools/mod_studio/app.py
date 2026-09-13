"""ISSD Mod Studio - a visual editor for mod packs.

The editor never touches a cartridge. It reads and writes the same `.json`
packs the game loads, and checks them with the same rules `validate_mod.py`
uses - it writes the pack to a scratch file and runs that module, rather than
keeping a second copy of the rules that can disagree with the first.
"""
from __future__ import annotations

import os
import sys
import tempfile
import tkinter as tk
from tkinter import colorchooser, filedialog, messagebox, ttk

from PIL import ImageTk

from . import cartridge, model, preview, repo

APP_NAME = "ISSD Mod Studio"
SETTINGS = os.path.join(os.path.expanduser("~"),
                        ".issd_mod_studio.json")

BG = "#f4f5f7"
PANEL = "#ffffff"
MUTED = "#6b7280"


def _validator():
    """tools/validate_mod.py, imported however the editor happens to be run."""
    try:
        from .. import validate_mod            # type: ignore
        return validate_mod
    except Exception:
        pass
    roots = [os.path.dirname(os.path.dirname(os.path.abspath(__file__)))]
    bundled = getattr(sys, "_MEIPASS", None)   # inside the packaged exe
    if bundled:
        roots.insert(0, bundled)
    for here in roots:
        if here not in sys.path:
            sys.path.insert(0, here)
    try:
        import validate_mod                    # type: ignore
    except Exception:
        return None
    # Packaged, it cannot read the C the formation names come from, so give
    # it the snapshot this editor already carries.
    if not validate_mod.known_formations():
        validate_mod.FORMATION_NAMES = repo.formation_names()
    return validate_mod


class Studio(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title(APP_NAME)
        self.geometry("1340x820")
        self.minsize(940, 620)
        self.configure(bg=BG)

        self.pack_data = model.new_pack()
        self.path: str | None = None
        self.dirty = False
        self._images: list[ImageTk.PhotoImage] = []   # keep references alive
        self._suspend = False
        self.rom_path = self._remembered_rom()
        self.rom: bytes | None = None

        self._build_style()
        self._build_menu()
        self._build_toolbar()
        self._build_body()
        self._build_status()
        self.refresh_tree(select_root=True)
        self.protocol("WM_DELETE_WINDOW", self.on_quit)

    # ------------------------------------------------------------ chrome --
    def _build_style(self):
        s = ttk.Style(self)
        try:
            s.theme_use("vista")
        except tk.TclError:
            pass
        s.configure("TFrame", background=BG)
        s.configure("Panel.TFrame", background=PANEL, relief="flat")
        s.configure("TLabel", background=BG)
        s.configure("Panel.TLabel", background=PANEL)
        s.configure("Head.TLabel", background=PANEL, font=("Segoe UI", 13, "bold"))
        s.configure("Muted.TLabel", background=PANEL, foreground=MUTED)
        s.configure("Treeview", rowheight=22)

    def _build_menu(self):
        m = tk.Menu(self)
        f = tk.Menu(m, tearoff=0)
        f.add_command(label="New pack", accelerator="Ctrl+N", command=self.on_new)
        f.add_command(label="Open...", accelerator="Ctrl+O", command=self.on_open)
        f.add_separator()
        f.add_command(label="Import from cartridge...",
                      command=self.on_import)
        f.add_separator()
        f.add_command(label="Save", accelerator="Ctrl+S", command=self.on_save)
        f.add_command(label="Save as...", command=self.on_save_as)
        f.add_separator()
        f.add_command(label="Quit", command=self.on_quit)
        m.add_cascade(label="File", menu=f)

        a = tk.Menu(m, tearoff=0)
        a.add_command(label="Add team (new, nothing replaced)",
                      command=lambda: self.on_add_team(added=True))
        a.add_command(label="Add team (replace an existing one)",
                      command=lambda: self.on_add_team(added=False))
        a.add_command(label="Add stadium", command=self.on_add_stadium)
        a.add_separator()
        a.add_command(label="Duplicate selection", command=self.on_duplicate)
        a.add_command(label="Delete selection", accelerator="Del",
                      command=self.on_delete)
        m.add_cascade(label="Edit", menu=a)

        h = tk.Menu(m, tearoff=0)
        h.add_command(label="What the cartridge can and cannot do",
                      command=self.on_about_limits)
        m.add_cascade(label="Help", menu=h)
        self.config(menu=m)

        self.bind_all("<Control-n>", lambda e: self.on_new())
        self.bind_all("<Control-o>", lambda e: self.on_open())
        self.bind_all("<Control-s>", lambda e: self.on_save())
        self.bind_all("<Delete>", lambda e: self.on_delete())

    def _build_toolbar(self):
        bar = ttk.Frame(self, padding=(8, 6))
        bar.pack(side="top", fill="x")
        for text, cmd in (("New", self.on_new), ("Open", self.on_open),
                          ("Save", self.on_save)):
            ttk.Button(bar, text=text, command=cmd, width=8).pack(side="left", padx=2)
        ttk.Separator(bar, orient="vertical").pack(side="left", fill="y", padx=8)
        ttk.Button(bar, text="Add team", width=10,
                   command=lambda: self.on_add_team(True)).pack(side="left", padx=2)
        ttk.Button(bar, text="Add stadium", width=12,
                   command=self.on_add_stadium).pack(side="left", padx=2)
        ttk.Button(bar, text="Delete", width=8,
                   command=self.on_delete).pack(side="left", padx=2)
        ttk.Separator(bar, orient="vertical").pack(side="left", fill="y", padx=8)
        ttk.Button(bar, text="Check pack", width=12,
                   command=self.on_validate).pack(side="left", padx=2)
        ttk.Button(bar, text="Import from cartridge", width=20,
                   command=self.on_import).pack(side="left", padx=2)

    def _build_body(self):
        body = ttk.Frame(self, padding=(8, 0))
        body.pack(side="top", fill="both", expand=True)

        left = ttk.Frame(body, width=290)
        left.pack(side="left", fill="y")
        left.pack_propagate(False)
        self.tree = ttk.Treeview(left, show="tree", selectmode="browse")
        sb = ttk.Scrollbar(left, orient="vertical", command=self.tree.yview)
        self.tree.configure(yscrollcommand=sb.set)
        sb.pack(side="right", fill="y")
        self.tree.pack(side="left", fill="both", expand=True)
        self.tree.bind("<<TreeviewSelect>>", lambda e: self.show_selected())

        right = ttk.Frame(body, padding=(10, 0, 0, 0))
        right.pack(side="left", fill="both", expand=True)
        self.canvas = tk.Canvas(right, bg=PANEL, highlightthickness=1,
                                highlightbackground="#d8dade")
        vs = ttk.Scrollbar(right, orient="vertical", command=self.canvas.yview)
        self.canvas.configure(yscrollcommand=vs.set)
        vs.pack(side="right", fill="y")
        self.canvas.pack(side="left", fill="both", expand=True)
        self.editor = ttk.Frame(self.canvas, style="Panel.TFrame", padding=16)
        self._editor_window = self.canvas.create_window(
            (0, 0), window=self.editor, anchor="nw")
        self.editor.bind("<Configure>", self._resize_editor)
        self.canvas.bind("<Configure>", self._resize_canvas)
        self.canvas.bind_all("<MouseWheel>", self._wheel)

    def _resize_editor(self, _):
        self.canvas.configure(scrollregion=self.canvas.bbox("all"))

    def _resize_canvas(self, event):
        self.canvas.itemconfigure(self._editor_window, width=event.width)

    def _wheel(self, event):
        if self.canvas.winfo_containing(event.x_root, event.y_root) is None:
            return
        self.canvas.yview_scroll(int(-event.delta / 120), "units")

    def _build_status(self):
        self.status = tk.StringVar(value="Ready.")
        bar = ttk.Frame(self, padding=(10, 4))
        bar.pack(side="bottom", fill="x")
        ttk.Label(bar, textvariable=self.status).pack(side="left")

    # -------------------------------------------------------------- tree --
    def refresh_tree(self, select_root=False, select_iid=None):
        remembered = select_iid or (self.tree.selection()[0]
                                    if self.tree.selection() else None)
        self.tree.delete(*self.tree.get_children())
        self.tree.insert("", "end", iid="pack", text="  " + (
            self.pack_data.get("name") or "Untitled Pack"), open=True)

        teams = self.pack_data.get("teams", [])
        self.tree.insert("pack", "end", iid="teams",
                         text="  Teams (%d)" % len(teams), open=True)
        for i, team in enumerate(teams):
            tid = "team:%d" % i
            self.tree.insert("teams", "end", iid=tid,
                             text="  " + model.team_label(team), open=False)
            for j, player in enumerate(team.get("players", [])):
                self.tree.insert(tid, "end", iid="player:%d:%d" % (i, j),
                                 text="  " + model.player_label(player, j))

        stadiums = self.pack_data.get("stadiums", [])
        self.tree.insert("pack", "end", iid="stadiums",
                         text="  Stadiums (%d)" % len(stadiums), open=True)
        for i, st in enumerate(stadiums):
            self.tree.insert("stadiums", "end", iid="stadium:%d" % i,
                             text="  " + model.stadium_label(st))

        target = "pack" if select_root else remembered
        if target and self.tree.exists(target):
            self.tree.selection_set(target)
            self.tree.see(target)
        else:
            self.tree.selection_set("pack")
        self.show_selected()

    def selection(self):
        sel = self.tree.selection()
        return sel[0] if sel else "pack"

    # ------------------------------------------------------------ editors --
    def _clear_editor(self):
        for child in self.editor.winfo_children():
            child.destroy()
        self._images.clear()

    def show_selected(self):
        iid = self.selection()
        self._clear_editor()
        if iid.startswith("player:"):
            _, t, p = iid.split(":")
            self.editor_player(int(t), int(p))
        elif iid.startswith("team:"):
            self.editor_team(int(iid.split(":")[1]))
        elif iid.startswith("stadium:"):
            self.editor_stadium(int(iid.split(":")[1]))
        elif iid == "teams":
            self.editor_overview("teams")
        elif iid == "stadiums":
            self.editor_overview("stadiums")
        else:
            self.editor_pack()
        # The frame has only just been filled, so its size is not known until
        # tk has laid it out. Without this the scroll bar thinks the pane is
        # one screen tall and the ratings cannot be reached.
        self.editor.update_idletasks()
        self.canvas.configure(scrollregion=self.canvas.bbox("all"))
        self.canvas.yview_moveto(0)

    # -- small builders -----------------------------------------------------
    def _head(self, parent, text, sub=""):
        ttk.Label(parent, text=text, style="Head.TLabel").grid(
            row=0, column=0, columnspan=4, sticky="w")
        if sub:
            ttk.Label(parent, text=sub, style="Muted.TLabel", wraplength=640,
                      justify="left").grid(row=1, column=0, columnspan=4,
                                           sticky="w", pady=(2, 10))

    def _bind_entry(self, parent, row, label, target, key, width=34,
                    limit=None, note="", on_change=None):
        ttk.Label(parent, text=label, style="Panel.TLabel").grid(
            row=row, column=0, sticky="w", pady=3)
        var = tk.StringVar(value=str(target.get(key, "") or ""))
        entry = ttk.Entry(parent, textvariable=var, width=width)
        entry.grid(row=row, column=1, sticky="w", pady=3)
        hint = ttk.Label(parent, text=note, style="Muted.TLabel")
        hint.grid(row=row, column=2, sticky="w", padx=8)

        def changed(*_):
            if self._suspend:
                return
            value = var.get()
            if limit and len(value) > limit:
                value = value[:limit]
                var.set(value)
            target[key] = value
            self.mark_dirty()
            if on_change:
                on_change()
        var.trace_add("write", changed)
        return var, entry, hint

    def _bind_int(self, parent, row, label, target, key, lo, hi, note="",
                  on_change=None, width=8):
        ttk.Label(parent, text=label, style="Panel.TLabel").grid(
            row=row, column=0, sticky="w", pady=3)
        var = tk.StringVar(value=str(target.get(key, lo)))
        spin = ttk.Spinbox(parent, from_=lo, to=hi, textvariable=var, width=width)
        spin.grid(row=row, column=1, sticky="w", pady=3)
        if note:
            ttk.Label(parent, text=note, style="Muted.TLabel").grid(
                row=row, column=2, sticky="w", padx=8)

        def changed(*_):
            if self._suspend:
                return
            try:
                v = int(float(var.get()))
            except ValueError:
                return
            target[key] = max(lo, min(hi, v))
            self.mark_dirty()
            if on_change:
                on_change()
        var.trace_add("write", changed)
        return var

    def _bind_combo(self, parent, row, label, target, key, values, note="",
                    on_change=None, width=24):
        ttk.Label(parent, text=label, style="Panel.TLabel").grid(
            row=row, column=0, sticky="w", pady=3)
        var = tk.StringVar(value=str(target.get(key, "") or ""))
        combo = ttk.Combobox(parent, textvariable=var, values=list(values),
                             state="readonly", width=width)
        combo.grid(row=row, column=1, sticky="w", pady=3)
        if note:
            ttk.Label(parent, text=note, style="Muted.TLabel").grid(
                row=row, column=2, sticky="w", padx=8)

        def changed(*_):
            if self._suspend:
                return
            target[key] = var.get()
            self.mark_dirty()
            if on_change:
                on_change()
        var.trace_add("write", changed)
        return var

    def _image(self, parent, im, **grid):
        photo = ImageTk.PhotoImage(im)
        self._images.append(photo)
        label = ttk.Label(parent, image=photo, style="Panel.TLabel")
        label.grid(**grid)
        return label

    # -- pack ---------------------------------------------------------------
    def editor_pack(self):
        f = self.editor
        self._head(f, "Pack",
                   "What the Mods page shows, and what the saved list "
                   "remembers. Changing the name loses the tick next to it.")
        self._bind_entry(f, 2, "Name", self.pack_data, "name",
                         on_change=lambda: self.refresh_tree(select_iid="pack"))
        self._bind_entry(f, 3, "Author", self.pack_data, "author")
        self._bind_entry(f, 4, "Version", self.pack_data, "version", width=14)
        self._bind_entry(f, 5, "Description", self.pack_data, "description",
                         width=60)

        ttk.Separator(f, orient="horizontal").grid(row=6, column=0, columnspan=4,
                                                   sticky="ew", pady=12)
        ttk.Label(f, text="Stadium slots", style="Head.TLabel").grid(
            row=7, column=0, columnspan=3, sticky="w")
        ttk.Label(f, text="The cartridge ships 8. Raise this and its tables are "
                          "relocated so more will fit - up to %d."
                          % repo.STADIUM_MAX,
                  style="Muted.TLabel", wraplength=640, justify="left").grid(
            row=8, column=0, columnspan=4, sticky="w", pady=(2, 8))
        self._bind_int(f, 9, "stadium_count", self.pack_data, "stadium_count",
                       0, repo.STADIUM_MAX,
                       note="0 leaves the cartridge's eight")

        unlock = tk.BooleanVar(value=bool(self.pack_data.get("unlock_bonus_teams")))

        def toggled():
            self.pack_data["unlock_bonus_teams"] = bool(unlock.get())
            self.mark_dirty()
        ttk.Checkbutton(f, text="Show the seventh group even if this pack adds "
                                "no teams", variable=unlock, command=toggled,
                        style="TCheckbutton").grid(row=10, column=0, columnspan=3,
                                                   sticky="w", pady=(10, 0))

        added = sum(1 for t in self.pack_data.get("teams", []) if t.get("new_team"))
        ttk.Label(f, text="This pack adds %d team(s) and replaces %d."
                          % (added, len(self.pack_data.get("teams", [])) - added),
                  style="Muted.TLabel").grid(row=11, column=0, columnspan=3,
                                             sticky="w", pady=(14, 0))

    def editor_overview(self, which):
        f = self.editor
        if which == "teams":
            self._head(f, "Teams",
                       "A pack can replace any of the 36 squads, and add up to "
                       "%d more. An added team takes one of the seventh "
                       "group's cells and nothing is replaced."
                       % repo.ADDED_SLOTS)
            ttk.Button(f, text="Add a new team",
                       command=lambda: self.on_add_team(True)).grid(
                row=2, column=0, sticky="w", pady=4)
            ttk.Button(f, text="Replace an existing team",
                       command=lambda: self.on_add_team(False)).grid(
                row=3, column=0, sticky="w", pady=4)
        else:
            self._head(f, "Stadiums",
                       "Replace any of the eight, or raise stadium_count on the "
                       "Pack page and add more. A ground you name gets a plate "
                       "the game draws for it.")
            ttk.Button(f, text="Add a stadium", command=self.on_add_stadium).grid(
                row=2, column=0, sticky="w", pady=4)

    # -- team ---------------------------------------------------------------
    def editor_team(self, index):
        team = self.pack_data["teams"][index]
        f = self.editor
        self._head(f, "Team - " + (team.get("name") or "unnamed"))

        mode = tk.StringVar(value="add" if team.get("new_team") else "replace")
        names = repo.team_names()
        slot_values = ["%d - %s" % (i, n) for i, n in enumerate(names)]

        def set_mode():
            if self._suspend:
                return
            if mode.get() == "add":
                team["new_team"] = True
                team.pop("team_id", None)
            else:
                team.pop("new_team", None)
                team.setdefault("team_id", 0)
            self.mark_dirty()
            self.refresh_tree(select_iid="team:%d" % index)

        box = ttk.Frame(f, style="Panel.TFrame")
        box.grid(row=2, column=0, columnspan=4, sticky="w", pady=(0, 6))
        ttk.Radiobutton(box, text="Add as a new team - replaces nobody",
                        value="add", variable=mode, command=set_mode).grid(
            row=0, column=0, sticky="w")
        ttk.Radiobutton(box, text="Replace an existing team", value="replace",
                        variable=mode, command=set_mode).grid(row=1, column=0,
                                                              sticky="w")

        if mode.get() == "replace":
            tid = team.get("team_id", 0)
            tid = tid if isinstance(tid, int) and 0 <= tid < len(names) else 0
            slot = tk.StringVar(value=slot_values[tid])
            ttk.Label(f, text="Which team", style="Panel.TLabel").grid(
                row=3, column=0, sticky="w", pady=3)
            combo = ttk.Combobox(f, textvariable=slot, values=slot_values,
                                 state="readonly", width=30)
            combo.grid(row=3, column=1, sticky="w", pady=3)

            warn = ttk.Label(f, text="", style="Muted.TLabel", wraplength=420,
                             justify="left")
            warn.grid(row=3, column=2, sticky="w", padx=8)

            def slot_changed(*_):
                if self._suspend:
                    return
                team["team_id"] = int(slot.get().split(" - ")[0])
                self._team_slot_note(team, warn)
                self.mark_dirty()
                self.refresh_tree(select_iid="team:%d" % index)
            slot.trace_add("write", slot_changed)
            self._team_slot_note(team, warn)

            def load_from_rom():
                index = self.import_team(int(team.get("team_id") or 0))
                if index is None:
                    return
                self.mark_dirty()
                self.refresh_tree(select_iid="team:%d" % index)
                self.status.set("Loaded this team from the cartridge.")
            ttk.Button(f, text="Load this team from the cartridge",
                       command=load_from_rom).grid(row=3, column=3, sticky="w")

        self._bind_entry(f, 4, "Name", team, "name",
                         note="for the log and this editor",
                         on_change=lambda: self.refresh_tree(
                             select_iid="team:%d" % index))
        self._bind_entry(f, 5, "Plate name", team, "plate_name",
                         limit=repo.PLATE_CHARS, width=18,
                         note="what the select screen's plate reads (%d max)"
                              % repo.PLATE_CHARS)

        ttk.Separator(f, orient="horizontal").grid(row=6, column=0, columnspan=4,
                                                   sticky="ew", pady=12)

        shape_box = ttk.Frame(f, style="Panel.TFrame")
        shape_box.grid(row=7, column=0, columnspan=4, sticky="w")
        shape_fields = ttk.Frame(shape_box, style="Panel.TFrame")
        shape_fields.grid(row=0, column=0, sticky="nw")
        shape_img = ttk.Label(shape_box, style="Panel.TLabel")
        shape_img.grid(row=1, column=0, sticky="w", pady=(10, 0))

        def redraw_shape():
            im = preview.formation(team.get("formation") or "4-4-2",
                                   team.get("tactics") or "balanced")
            photo = ImageTk.PhotoImage(im)
            self._images.append(photo)
            shape_img.configure(image=photo)

        ttk.Label(shape_fields, text="Shape", style="Head.TLabel").grid(
            row=0, column=0, columnspan=2, sticky="w", pady=(0, 6))
        self._bind_combo(shape_fields, 1, "Formation", team, "formation",
                         repo.formation_names(), on_change=redraw_shape)
        self._bind_combo(shape_fields, 2, "Tactics", team, "tactics", repo.TACTICS,
                         note="shifts every line by six", on_change=redraw_shape,
                         width=14)
        blurb = ttk.Label(shape_fields, text="", style="Muted.TLabel",
                          wraplength=320, justify="left")
        blurb.grid(row=3, column=0, columnspan=2, sticky="w", pady=(6, 0))
        shape = repo.formation(team.get("formation") or "")
        if shape:
            blurb.configure(text="%s   printed on screen as %s"
                                 % (shape["blurb"], shape["label"]))
        redraw_shape()

        ttk.Separator(f, orient="horizontal").grid(row=8, column=0, columnspan=4,
                                                   sticky="ew", pady=12)

        kit_box = ttk.Frame(f, style="Panel.TFrame")
        kit_box.grid(row=9, column=0, columnspan=4, sticky="w")
        kit_fields = ttk.Frame(kit_box, style="Panel.TFrame")
        kit_fields.grid(row=0, column=0, sticky="nw")
        ttk.Label(kit_fields, text="Strip", style="Head.TLabel").grid(
            row=0, column=0, columnspan=3, sticky="w")
        ttk.Label(kit_fields, text="One colour per part - the cartridge derives the "
                               "other two shades. It cannot draw stripes or "
                               "hoops; those are in the sprite graphics.",
                  style="Muted.TLabel", wraplength=430, justify="left").grid(
            row=1, column=0, columnspan=3, sticky="w", pady=(2, 8))
        kit_img = ttk.Label(kit_box, style="Panel.TLabel")
        kit_img.grid(row=1, column=0, sticky="w", pady=(10, 0))

        def redraw_kit():
            im = preview.kit(team.get("shirt"), team.get("shorts"),
                             team.get("socks"))
            photo = ImageTk.PhotoImage(im)
            self._images.append(photo)
            kit_img.configure(image=photo)

        for i, key in enumerate(("shirt", "shorts", "socks")):
            self._colour_row(kit_fields, 2 + i, key.title(), team, key, redraw_kit)
        redraw_kit()

        self._bind_int(kit_fields, 5, "kit_record", team, "kit_record", -1,
                       repo.KIT_RECORDS - 1,
                       note="-1 uses the measured table; only needed for a team "
                            "it does not cover")

        ttk.Separator(f, orient="horizontal").grid(row=10, column=0, columnspan=4,
                                                   sticky="ew", pady=12)
        self._photo_row(f, 11, team)

    def _team_slot_note(self, team, label):
        tid = team.get("team_id")
        if not isinstance(tid, int):
            label.configure(text="")
            return
        notes = []
        if tid >= repo.STOCK_TEAMS:
            notes.append("This is an all-star side: it picks its players from "
                         "its group when the match loads, so a squad here is "
                         "refused. Use 'Add as a new team' instead.")
        if tid in repo.SHARED_KIT_TEAMS:
            notes.append("This team shares its strip with others, so "
                         "recolouring it recolours them too.")
        label.configure(text="\n".join(notes))

    def _colour_row(self, parent, row, label, target, key, on_change):
        ttk.Label(parent, text=label, style="Panel.TLabel").grid(
            row=row, column=0, sticky="w", pady=3)
        var = tk.StringVar(value=str(target.get(key, "") or ""))
        entry = ttk.Entry(parent, textvariable=var, width=12)
        entry.grid(row=row, column=1, sticky="w", pady=3)
        swatch = tk.Label(parent, width=4, relief="solid", borderwidth=1)
        swatch.grid(row=row, column=2, sticky="w", padx=6)

        def repaint():
            rgb = model.parse_colour(var.get())
            swatch.configure(bg="#%02X%02X%02X" % rgb if rgb else PANEL)

        def changed(*_):
            if self._suspend:
                return
            target[key] = var.get()
            repaint()
            on_change()
            self.mark_dirty()
        var.trace_add("write", changed)

        def pick():
            rgb = model.parse_colour(var.get()) or (255, 255, 255)
            chosen = colorchooser.askcolor(
                color="#%02X%02X%02X" % rgb, parent=self, title=label)
            if chosen and chosen[1]:
                var.set(chosen[1].upper())
        ttk.Button(parent, text="Pick...", width=8, command=pick).grid(
            row=row, column=3, sticky="w")
        repaint()

    def _photo_row(self, parent, row, team):
        ttk.Label(parent, text="Squad photograph", style="Head.TLabel").grid(
            row=row, column=0, columnspan=3, sticky="w")
        ttk.Label(parent, text="A 32-bit .bmp beside the pack. Any size; it is "
                               "sampled into the 96x72 the frame leaves.",
                  style="Muted.TLabel", wraplength=520, justify="left").grid(
            row=row + 1, column=0, columnspan=4, sticky="w", pady=(2, 8))
        var, entry, _ = self._bind_entry(parent, row + 2, "File", team, "photo",
                                         width=44)
        shown = ttk.Label(parent, style="Panel.TLabel")
        shown.grid(row=row + 3, column=1, sticky="w", pady=6)

        def redraw():
            path = team.get("photo") or ""
            base = os.path.dirname(self.path) if self.path else os.getcwd()
            full = os.path.join(base, path)
            if path and os.path.isfile(full):
                try:
                    from PIL import Image
                    im = Image.open(full).convert("RGB").resize((192, 144))
                    photo = ImageTk.PhotoImage(im)
                    self._images.append(photo)
                    shown.configure(image=photo, text="")
                    return
                except Exception as exc:
                    shown.configure(image="", text="cannot read it: %s" % exc)
                    return
            shown.configure(image="",
                            text="(none - the cartridge's own photograph stands)"
                            if not path else "(not found beside the pack yet)")
        var.trace_add("write", lambda *_: redraw())

        def browse():
            base = os.path.dirname(self.path) if self.path else os.getcwd()
            chosen = filedialog.askopenfilename(
                parent=self, title="Squad photograph",
                initialdir=base, filetypes=[("32-bit bitmap", "*.bmp")])
            if not chosen:
                return
            try:
                rel = os.path.relpath(chosen, base).replace(os.sep, "/")
            except ValueError:
                rel = chosen
            var.set(rel)
        ttk.Button(parent, text="Browse...", command=browse).grid(
            row=row + 2, column=2, sticky="w", padx=8)
        redraw()

    # -- player -------------------------------------------------------------
    def editor_player(self, team_index, slot):
        team = self.pack_data["teams"][team_index]
        player = team["players"][slot]
        f = self.editor

        role = {0: "the goalkeeper", repo.RESERVE_KEEPER: "the reserve keeper"}.get(
            slot, "one of the starting eleven" if slot < repo.STARTING_XI
            else "a substitute")
        self._head(f, "Slot %d - %s" % (slot, player.get("name") or "unnamed"),
                   "Position in the squad decides who this is: slot %d is %s. "
                   "The shirt number is only a label." % (slot, role))

        def retitle():
            self.tree.item("player:%d:%d" % (team_index, slot),
                           text="  " + model.player_label(player, slot))

        bad = ttk.Label(f, text="", style="Muted.TLabel", foreground="#b02020")
        bad.grid(row=2, column=3, sticky="w", padx=8)

        def name_checked():
            name = player.get("name") or ""
            wrong = sorted({c for c in name if c not in repo.NAME_CHARS})
            bad.configure(text="cannot be shown: %s" % " ".join(wrong)
                          if wrong else "")
            retitle()
        self._bind_entry(f, 2, "Name", player, "name", width=18,
                         limit=repo.NAME_MAX,
                         note="%d characters, letters and spaces"
                              % repo.NAME_MAX,
                         on_change=name_checked)
        name_checked()

        self._bind_int(f, 3, "Shirt number", player, "shirt_number", 1, 99)
        self._bind_combo(f, 4, "Position", player, "position", repo.POSITIONS,
                         width=8, on_change=retitle)

        ttk.Separator(f, orient="horizontal").grid(row=5, column=0, columnspan=4,
                                                   sticky="ew", pady=12)

        look = ttk.Frame(f, style="Panel.TFrame")
        look.grid(row=6, column=0, columnspan=4, sticky="w")
        ttk.Label(look, text="Appearance", style="Head.TLabel").grid(
            row=0, column=0, columnspan=3, sticky="w")
        ttk.Label(look, text="Skin tone and hair style are the two appearance "
                            "fields the cartridge keeps per player. There is no "
                            "hair colour: forcing a squad's tone changes the "
                            "picture without the game reading a single different "
                            "byte, so the colours come from a palette already "
                            "loaded, not from a table a pack could point at.",
                  style="Muted.TLabel", wraplength=470, justify="left").grid(
            row=1, column=0, columnspan=3, sticky="w", pady=(2, 8))
        head = ttk.Label(look, style="Panel.TLabel")
        head.grid(row=4, column=0, columnspan=3, sticky="w", pady=(10, 0))

        def redraw_head():
            im = preview.player_head(player.get("skin_tone", 0),
                                     player.get("hair_style", 0))
            photo = ImageTk.PhotoImage(im)
            self._images.append(photo)
            head.configure(image=photo)

        self._bind_int(look, 2, "Skin tone", player, "skin_tone", 0, 2,
                       note="0 light, 1 medium, 2 dark", on_change=redraw_head)
        self._bind_int(look, 3, "Hair style", player, "hair_style", 0, 15,
                       note="0-15; the cartridge's own squads use 0-13",
                       on_change=redraw_head)
        redraw_head()

        ttk.Separator(f, orient="horizontal").grid(row=7, column=0, columnspan=4,
                                                   sticky="ew", pady=12)
        ttk.Label(f, text="Ratings", style="Head.TLabel").grid(
            row=8, column=0, columnspan=3, sticky="w")
        ttk.Label(f, text="A rating is 0-99 in the pack and four bits in the "
                          "cartridge, and only 2-9 of those are used. The pale "
                          "band behind each bar is the range that stores as the "
                          "same value - two players inside one band play "
                          "identically.",
                  style="Muted.TLabel", wraplength=640, justify="left").grid(
            row=9, column=0, columnspan=4, sticky="w", pady=(2, 10))

        stats = ttk.Frame(f, style="Panel.TFrame")
        stats.grid(row=10, column=0, columnspan=4, sticky="w")
        for i, key in enumerate(model.ATTRIBUTES):
            self._stat_row(stats, i, player, key)

    def _stat_row(self, parent, row, player, key):
        written = key in model.ATTRIBUTES_WRITTEN
        label = key.title() if written else key.title() + " *"
        ttk.Label(parent, text=label, style="Panel.TLabel", width=14).grid(
            row=row, column=0, sticky="w", pady=2)

        value = tk.IntVar(value=int(player.get(key, 50) or 0))
        scale = ttk.Scale(parent, from_=0, to=99, orient="horizontal", length=200,
                          variable=value)
        scale.grid(row=row, column=1, sticky="w", padx=(0, 8))
        spin = ttk.Spinbox(parent, from_=0, to=99, width=5, textvariable=value)
        spin.grid(row=row, column=2, sticky="w")
        bar = ttk.Label(parent, style="Panel.TLabel")
        bar.grid(row=row, column=3, padx=8)
        note = ttk.Label(parent, text="", style="Muted.TLabel", width=22)
        note.grid(row=row, column=4, sticky="w")

        def redraw(*_):
            if self._suspend:
                return
            v = max(0, min(99, int(value.get())))
            player[key] = v
            im = preview.stat_bar(v)
            photo = ImageTk.PhotoImage(im)
            self._images.append(photo)
            bar.configure(image=photo)
            nibble = repo.rating_to_nibble(v)
            lo, hi = repo.nibble_band(nibble)
            note.configure(text="stored as %d   (%d-%d)" % (nibble, lo, hi)
                           if written else "not written to the cartridge")
            self.mark_dirty()
        value.trace_add("write", redraw)
        redraw()

    # -- stadium ------------------------------------------------------------
    def editor_stadium(self, index):
        st = self.pack_data["stadiums"][index]
        f = self.editor
        self._head(f, "Stadium - " + (st.get("display_name")
                                      or st.get("name") or "unnamed"))

        stock = {s["id"]: s for s in repo.load()["stock_stadiums"]}
        note = ttk.Label(f, text="", style="Muted.TLabel", wraplength=560,
                         justify="left")
        note.grid(row=2, column=2, sticky="w", padx=8)

        def slot_note():
            sid = st.get("stadium_id")
            if isinstance(sid, int) and sid in stock:
                note.configure(text="Replaces %s (%d x %d yards)."
                                    % (stock[sid]["name"], stock[sid]["length"],
                                       stock[sid]["width"]))
            elif isinstance(sid, int) and sid >= repo.STOCK_STADIUMS:
                note.configure(text="An added slot. Raise stadium_count on the "
                                    "Pack page to at least %d or this ground is "
                                    "refused." % (sid + 1))
            else:
                note.configure(text="")

        self._bind_int(f, 2, "Slot", st, "stadium_id", 0, repo.STADIUM_MAX - 1,
                       on_change=lambda: (slot_note(), self.refresh_tree(
                           select_iid="stadium:%d" % index)))
        slot_note()

        self._bind_entry(f, 3, "Name", st, "name",
                         limit=repo.STADIUM_NAME_CHARS, width=12,
                         note="what the pre-match screen prints (%d characters)"
                              % repo.STADIUM_NAME_CHARS,
                         on_change=lambda: self.refresh_tree(
                             select_iid="stadium:%d" % index))
        self._bind_entry(f, 4, "Plate name", st, "display_name",
                         limit=repo.DISPLAY_NAME_CHARS, width=18,
                         note="what the select screen's plate reads (%d)"
                              % repo.DISPLAY_NAME_CHARS,
                         on_change=lambda: self.refresh_tree(
                             select_iid="stadium:%d" % index))

        ttk.Separator(f, orient="horizontal").grid(row=5, column=0, columnspan=4,
                                                   sticky="ew", pady=12)
        size_box = ttk.Frame(f, style="Panel.TFrame")
        size_box.grid(row=6, column=0, columnspan=4, sticky="w")
        ttk.Label(size_box, text="Pitch", style="Head.TLabel").grid(
            row=0, column=0, columnspan=3, sticky="w")
        ttk.Label(size_box, text="These are the numbers the screens print. The "
                                "playfield itself does not change size - the "
                                "same match on a 138x90 and a 115x74 pitch "
                                "leaves memory identical 1400 frames in.",
                  style="Muted.TLabel", wraplength=440, justify="left").grid(
            row=1, column=0, columnspan=3, sticky="w", pady=(2, 8))
        shown = ttk.Label(size_box, style="Panel.TLabel")
        shown.grid(row=4, column=0, columnspan=3, sticky="w", pady=(10, 0))

        def redraw():
            im = preview.pitch_size(st.get("pitch_length") or 0,
                                    st.get("pitch_width") or 0)
            photo = ImageTk.PhotoImage(im)
            self._images.append(photo)
            shown.configure(image=photo)

        self._bind_int(size_box, 2, "Length", st, "pitch_length",
                       0, repo.PITCH_LENGTH[1],
                       note="%d-%d yards; 0 keeps the cartridge's"
                            % repo.PITCH_LENGTH, on_change=redraw)
        self._bind_int(size_box, 3, "Width", st, "pitch_width",
                       0, repo.PITCH_WIDTH[1],
                       note="%d-%d yards; 0 keeps the cartridge's"
                            % repo.PITCH_WIDTH, on_change=redraw)
        redraw()

    # --------------------------------------------------------- cartridge --
    def _remembered_rom(self):
        """Where the cartridge was last time. It is the user's own dump and
        lives wherever they keep it, so asking once is enough.
        """
        try:
            import json
            with open(SETTINGS, encoding="utf-8") as f:
                return json.load(f).get("rom_path") or None
        except Exception:
            return None

    def _remember_rom(self, path):
        try:
            import json
            with open(SETTINGS, "w", encoding="utf-8") as f:
                json.dump({"rom_path": path}, f)
        except Exception:
            pass

    def cartridge_bytes(self, ask=True):
        """The cartridge, asked for once and kept."""
        if self.rom is not None:
            return self.rom
        path = self.rom_path
        if path and not os.path.isfile(path):
            path = None
        if not path:
            if not ask:
                return None
            path = filedialog.askopenfilename(
                parent=self, title="Your International Superstar Soccer Deluxe (USA) dump",
                filetypes=[("SNES cartridge", "*.sfc *.smc"),
                           ("All files", "*.*")])
            if not path:
                return None
        try:
            self.rom = cartridge.load_rom(path)
        except Exception as exc:
            messagebox.showerror(APP_NAME, "%s" % exc, parent=self)
            self.rom_path = None
            return None
        self.rom_path = path
        self._remember_rom(path)
        self.status.set("Cartridge: %s" % os.path.basename(path))
        return self.rom

    def on_import(self):
        rom = self.cartridge_bytes()
        if rom is None:
            return
        ImportDialog(self, rom)

    def import_team(self, team_id):
        """Pull one team in, replacing whatever the pack had for that slot."""
        rom = self.cartridge_bytes()
        if rom is None:
            return None
        entry = cartridge.read_team(rom, team_id)
        entry.pop("_label", None)
        teams = self.pack_data.setdefault("teams", [])
        for i, t in enumerate(teams):
            if not t.get("new_team") and t.get("team_id") == team_id:
                # Keep what the pack added on top: a plate, a photograph,
                # a shape it chose. Only the cartridge's own fields land.
                for key, value in entry.items():
                    if key == "formation" and t.get("formation"):
                        continue
                    t[key] = value
                return i
        teams.append(entry)
        return len(teams) - 1

    def import_stadium(self, slot):
        rom = self.cartridge_bytes()
        if rom is None:
            return None
        entry = cartridge.read_stadium(rom, slot)
        stadiums = self.pack_data.setdefault("stadiums", [])
        for i, st in enumerate(stadiums):
            if st.get("stadium_id") == slot:
                st.update(entry)
                return i
        stadiums.append(entry)
        return len(stadiums) - 1

    # ----------------------------------------------------------- commands --
    def mark_dirty(self):
        if not self.dirty:
            self.dirty = True
            self.title("%s - %s *" % (APP_NAME, self.path or "untitled"))

    def _settle(self):
        self.dirty = False
        self.title("%s - %s" % (APP_NAME, self.path or "untitled"))

    def on_new(self):
        if not self._confirm_discard():
            return
        self.pack_data = model.new_pack()
        self.path = None
        self._settle()
        self.refresh_tree(select_root=True)
        self.status.set("New pack.")

    def on_open(self):
        if not self._confirm_discard():
            return
        chosen = filedialog.askopenfilename(
            parent=self, title="Open a mod pack",
            filetypes=[("Mod pack", "*.json"), ("All files", "*.*")])
        if not chosen:
            return
        try:
            self.pack_data = model.load(chosen)
        except Exception as exc:
            messagebox.showerror(APP_NAME, "Cannot read that pack:\n\n%s" % exc,
                                 parent=self)
            return
        self.path = chosen
        self._settle()
        self.refresh_tree(select_root=True)
        self.status.set("Opened %s" % os.path.basename(chosen))

    def on_save(self):
        if not self.path:
            return self.on_save_as()
        try:
            model.save(self.pack_data, self.path)
        except Exception as exc:
            messagebox.showerror(APP_NAME, "Cannot write it:\n\n%s" % exc,
                                 parent=self)
            return
        self._settle()
        self.status.set("Saved %s" % os.path.basename(self.path))

    def on_save_as(self):
        chosen = filedialog.asksaveasfilename(
            parent=self, title="Save the pack", defaultextension=".json",
            filetypes=[("Mod pack", "*.json")])
        if not chosen:
            return
        self.path = chosen
        self.on_save()

    def on_add_team(self, added=True):
        team = model.new_team(added=added)
        self.pack_data.setdefault("teams", []).append(team)
        self.mark_dirty()
        self.refresh_tree(select_iid="team:%d" % (len(self.pack_data["teams"]) - 1))

    def on_add_stadium(self):
        stadiums = self.pack_data.setdefault("stadiums", [])
        used = {s.get("stadium_id") for s in stadiums}
        nxt = repo.STOCK_STADIUMS
        while nxt in used and nxt < repo.STADIUM_MAX - 1:
            nxt += 1
        stadiums.append(model.new_stadium(nxt))
        want = max(int(self.pack_data.get("stadium_count") or 0), nxt + 1)
        if nxt >= repo.STOCK_STADIUMS:
            self.pack_data["stadium_count"] = want
        self.mark_dirty()
        self.refresh_tree(select_iid="stadium:%d" % (len(stadiums) - 1))

    def on_duplicate(self):
        iid = self.selection()
        if iid.startswith("team:"):
            i = int(iid.split(":")[1])
            self.pack_data["teams"].append(
                model.clone_team(self.pack_data["teams"][i]))
            self.mark_dirty()
            self.refresh_tree(select_iid="team:%d"
                                         % (len(self.pack_data["teams"]) - 1))
        elif iid.startswith("stadium:"):
            i = int(iid.split(":")[1])
            self.pack_data["stadiums"].append(
                model.clone_team(self.pack_data["stadiums"][i]))
            self.mark_dirty()
            self.refresh_tree(select_iid="stadium:%d"
                                         % (len(self.pack_data["stadiums"]) - 1))

    def on_delete(self):
        iid = self.selection()
        if iid.startswith("team:"):
            i = int(iid.split(":")[1])
            name = model.team_label(self.pack_data["teams"][i])
            if not messagebox.askyesno(APP_NAME, "Remove %s from the pack?" % name,
                                       parent=self):
                return
            del self.pack_data["teams"][i]
        elif iid.startswith("stadium:"):
            i = int(iid.split(":")[1])
            del self.pack_data["stadiums"][i]
        elif iid.startswith("player:"):
            _, t, p = (int(x) if x.isdigit() else x for x in iid.split(":"))
            players = self.pack_data["teams"][t]["players"]
            if len(players) <= 1:
                self.status.set("A squad needs at least one player.")
                return
            del players[p]
        else:
            return
        self.mark_dirty()
        self.refresh_tree(select_root=True)

    def on_validate(self):
        v = _validator()
        if v is None:
            messagebox.showwarning(APP_NAME, "validate_mod.py is not beside this "
                                             "editor, so the pack cannot be "
                                             "checked here.", parent=self)
            return
        tmp = os.path.join(tempfile.gettempdir(), "issd_mod_studio_check.json")
        model.save(self.pack_data, tmp)
        report = v.Report()
        v.validate(tmp, report)
        try:
            os.remove(tmp)
        except OSError:
            pass
        self._show_report(report)

    def _show_report(self, report):
        win = tk.Toplevel(self)
        win.title("Check - " + (self.pack_data.get("name") or "pack"))
        win.geometry("720x420")
        text = tk.Text(win, wrap="word", padx=10, pady=8)
        sb = ttk.Scrollbar(win, orient="vertical", command=text.yview)
        text.configure(yscrollcommand=sb.set)
        sb.pack(side="right", fill="y")
        text.pack(side="left", fill="both", expand=True)
        text.tag_configure("err", foreground="#b02020")
        text.tag_configure("warn", foreground="#a06000")
        text.tag_configure("ok", foreground="#207040")

        if not report.errors and not report.warnings:
            text.insert("end", "Looks good. Nothing here will be silently "
                               "dropped when the game loads it.\n", "ok")
        for e in report.errors:
            text.insert("end", "ERROR    %s\n" % e, "err")
        for w in report.warnings:
            text.insert("end", "warning  %s\n" % w, "warn")
        text.configure(state="disabled")
        self.status.set("%d error(s), %d warning(s)."
                        % (len(report.errors), len(report.warnings)))

    def on_about_limits(self):
        messagebox.showinfo(
            APP_NAME,
            "What a pack can do:\n\n"
            "  - replace any of the 36 squads, completely\n"
            "  - add up to %d teams, replacing nobody\n"
            "  - up to %d stadiums\n"
            "  - kit colours, name plates and squad photographs\n\n"
            "What it cannot:\n\n"
            "  - more than 20 players in a squad\n"
            "  - names longer than %d characters, or with digits or accents\n"
            "  - a squad for one of the all-star sides: they pick their "
            "players from their group when the match loads\n"
            "  - striped or hooped shirts - a kit is one colour per part\n"
            "  - per-player hair colour: the cartridge has no such field\n"
            "  - the team name in the match HUD, or the flag beside it"
            % (repo.ADDED_SLOTS, repo.STADIUM_MAX, repo.NAME_MAX),
            parent=self)

    def _confirm_discard(self):
        if not self.dirty:
            return True
        answer = messagebox.askyesnocancel(
            APP_NAME, "Save the changes to this pack first?", parent=self)
        if answer is None:
            return False
        if answer:
            self.on_save()
            return not self.dirty
        return True

    def on_quit(self):
        if self._confirm_discard():
            self.destroy()


def selftest(report_path: str, pack_path: str | None = None) -> int:
    """Open a window, build every pane, run the checker, and close.

    A packaged .exe can be broken in ways the source is not - a missing
    baked.json, a Pillow that cannot talk to tk - and none of that shows up
    until somebody clicks the thing that needs it. This clicks them all and
    writes what happened to a file, because a windowed build has no console
    to print to.
    """
    lines = []
    ok = True
    try:
        studio = Studio()
        if pack_path:
            studio.pack_data = model.load(pack_path)
            studio.path = pack_path
        else:
            studio.pack_data = model.new_pack()
            studio.pack_data["teams"] = [model.new_team(True),
                                           model.new_team(False)]
            studio.pack_data["stadiums"] = [model.new_stadium(8)]
        studio.refresh_tree(select_root=True)

        panes = 0
        stack = list(studio.tree.get_children(""))
        while stack:
            iid = stack.pop()
            stack.extend(studio.tree.get_children(iid))
            studio.tree.selection_set(iid)
            studio.show_selected()
            studio.update_idletasks()
            panes += 1
        lines.append("panes built: %d" % panes)

        data = repo.load()
        lines.append("formations: %d, team names: %d, stadiums: %d"
                     % (len(data["formations"]),
                        len(data["team_names"]),
                        len(data["stock_stadiums"])))
        if len(data["formations"]) < 10:
            ok = False
            lines.append("the baked cartridge data is missing")

        v = _validator()
        if v is None:
            ok = False
            lines.append("validate_mod.py did not travel with the build")
        else:
            tmp = os.path.join(tempfile.gettempdir(),
                               "issd_mod_studio_selftest.json")
            model.save(studio.pack_data, tmp)
            report = v.Report()
            v.validate(tmp, report)
            os.remove(tmp)
            lines.append("checker ran: %d error(s), %d warning(s)"
                         % (len(report.errors), len(report.warnings)))

        studio.destroy()
    except Exception as exc:                      # noqa: BLE001
        import traceback
        ok = False
        lines.append("FAILED: %s" % exc)
        lines.append(traceback.format_exc())

    lines.insert(0, "ISSD Mod Studio self-test: %s"
                 % ("ok" if ok else "FAILED"))
    with open(report_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    return 0 if ok else 1


class ImportDialog(tk.Toplevel):
    """Pick what to take out of the cartridge.

    Everything the cartridge has for a team comes across: twenty names,
    their positions, skin tones, hair styles and ratings, the shape it
    plays and the colours it wears. Edit from there rather than from a
    blank sheet.
    """
    def __init__(self, studio, rom):
        super().__init__(studio)
        self.studio = studio
        self.rom = rom
        self.title("Import from cartridge")
        self.geometry("760x560")
        self.transient(studio)
        self.grab_set()

        ttk.Label(self, text=os.path.basename(studio.rom_path or ""),
                  padding=(10, 8)).pack(anchor="w")

        note = ("Ticking a team brings its twenty players across with their "
                "positions, skin tones, hair styles and ratings, plus its "
                "shape and its colours.")
        ttk.Label(self, text=note, wraplength=720, justify="left",
                  padding=(10, 0, 10, 8)).pack(anchor="w")

        book = ttk.Notebook(self)
        book.pack(fill="both", expand=True, padx=10)

        team_page = ttk.Frame(book)
        book.add(team_page, text="Teams")
        # exportselection off, or the two lists fight over the selection:
        # tick some teams, tick a stadium, and the teams quietly clear.
        self.teams = tk.Listbox(team_page, selectmode="extended",
                                exportselection=False,
                                font=("Consolas", 9))
        ts = ttk.Scrollbar(team_page, orient="vertical",
                           command=self.teams.yview)
        self.teams.configure(yscrollcommand=ts.set)
        ts.pack(side="right", fill="y")
        self.teams.pack(side="left", fill="both", expand=True)
        for tid, name, who in cartridge.summary(rom):
            self.teams.insert("end", "%2d  %-18s %s" % (tid, name, who))

        stad_page = ttk.Frame(book)
        book.add(stad_page, text="Stadiums")
        self.stadiums = tk.Listbox(stad_page, selectmode="extended",
                                   exportselection=False,
                                   font=("Consolas", 9))
        self.stadiums.pack(fill="both", expand=True)
        for slot in range(cartridge.stadium_count(rom)):
            st = cartridge.read_stadium(rom, slot)
            self.stadiums.insert("end", "%d  %-9s %d x %d yards"
                                 % (slot, st["name"],
                                    st["pitch_length"], st["pitch_width"]))

        bar = ttk.Frame(self, padding=10)
        bar.pack(fill="x")
        ttk.Button(bar, text="Import", command=self.take).pack(side="right")
        ttk.Button(bar, text="Cancel", command=self.destroy).pack(
            side="right", padx=6)
        ttk.Button(bar, text="Select all teams",
                   command=lambda: self.teams.select_set(0, "end")).pack(
            side="left")

    def take(self):
        teams = [int(self.teams.get(i).split()[0])
                 for i in self.teams.curselection()]
        stadiums = [int(self.stadiums.get(i).split()[0])
                    for i in self.stadiums.curselection()]
        if not teams and not stadiums:
            self.destroy()
            return
        for tid in teams:
            self.studio.import_team(tid)
        for slot in stadiums:
            self.studio.import_stadium(slot)
        if stadiums:
            highest = max(stadiums) + 1
            have = int(self.studio.pack_data.get("stadium_count") or 0)
            if highest > repo.STOCK_STADIUMS and highest > have:
                self.studio.pack_data["stadium_count"] = highest
        self.studio.mark_dirty()
        self.studio.refresh_tree(select_root=True)
        self.studio.status.set("Imported %d team(s) and %d stadium(s)."
                               % (len(teams), len(stadiums)))
        self.destroy()


def main():
    argv = sys.argv[1:]
    if argv and argv[0] == "--selftest":
        out = argv[1] if len(argv) > 1 else "selftest.txt"
        pack = argv[2] if len(argv) > 2 else None
        raise SystemExit(selftest(out, pack))
    studio = Studio()
    if argv and os.path.isfile(argv[0]):
        try:
            studio.pack_data = model.load(argv[0])
            studio.path = argv[0]
            studio._settle()
            studio.refresh_tree(select_root=True)
        except Exception as exc:                  # noqa: BLE001
            messagebox.showerror(APP_NAME,
                                 "Cannot read that pack:\n\n%s" % exc,
                                 parent=studio)
    studio.mainloop()


if __name__ == "__main__":
    main()
