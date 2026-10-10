"""Stadium authoring state and the shared canvas coordinate system."""
import copy
import re
import math
import json
import uuid
from pathlib import Path
try:
    from ..stadium_profile import Diagnostic, TEMPLATES, CAMERA_FIELDS, validate_profile
except ImportError:
    from stadium_profile import Diagnostic, TEMPLATES, CAMERA_FIELDS, validate_profile


def world_to_canvas(u, v, scale=1, origin=(0, 0)):
    return origin[0]+(u+v)*scale, origin[1]+v*scale


def canvas_to_world(x, y, scale=1, origin=(0, 0)):
    if scale <= 0:
        raise ValueError('Canvas scale must be positive')
    v = (y-origin[1])/scale
    return (x-origin[0])/scale-v, v


def template_profile(base):
    if type(base) is not int or not 0 <= base < 8:
        raise ValueError('Choose an original layout from 0 to 7')
    length, width, *camera = TEMPLATES[base]
    return {'version': 1, 'base_layout': base, 'geometry': {
        'length_units': length, 'width_units': width,
        'camera': dict(zip(CAMERA_FIELDS, camera))}}


class StadiumDraft:
    def __init__(self, document, index, pack_dir=None):
        self.document, self.index = document, index
        self.pack_dir = Path(pack_dir) if pack_dir else Path('.')
        self.raw = {}
        self._dragging = False

    @property
    def stadium(self):
        return self.document.pack['stadiums'][self.index]

    @property
    def profile(self):
        return self.stadium.get('stadium_profile')

    @property
    def geometry(self):
        return self.profile['geometry']

    @property
    def prefix(self):
        return ('stadiums', self.index, 'stadium_profile')

    def enable_profile(self, base):
        self.document.set_value(self.prefix, template_profile(base))
        self.raw.clear()

    def attach_artwork(self, name):
        if not self.profile or self.pack_dir is None:
            raise ValueError('Save the pack and create an independent profile before attaching artwork')
        candidate = copy.deepcopy(self.stadium)
        candidate['stadium_profile']['artwork'] = name
        errors = validate_profile(candidate,self.pack_dir)
        if errors:
            raise ValueError('\n'.join(d.message for d in errors))
        self.document.set_value(self.prefix+('artwork',),name)

    def _path(self, field):
        if field in ('length_units', 'width_units'):
            return self.prefix+('geometry', field)
        if field in CAMERA_FIELDS:
            return self.prefix+('geometry', 'camera', field)
        raise ValueError('Unknown geometry field')

    def set_text(self, field, text):
        self._path(field)
        self.raw[field] = text
        if not self.profile or not re.fullmatch(r'[0-9]+', text):
            return False
        candidate = copy.deepcopy(self.profile)
        candidate.pop('artwork', None)
        geometry = candidate['geometry']
        if field in CAMERA_FIELDS:
            geometry.setdefault('camera', {})[field] = int(text)
        else:
            geometry[field] = int(text)
        if validate_profile({'stadium_profile': candidate}, self.pack_dir):
            return False
        if field in CAMERA_FIELDS and 'camera' not in self.geometry:
            self.document.set_value(self.prefix+('geometry', 'camera'), {})
        self.document.set_value(self._path(field), int(text))
        return True

    def diagnostics(self):
        errors = []
        for field, text in self.raw.items():
            if not re.fullmatch(r'[0-9]+', text):
                errors.append(Diagnostic('error', self._path(field), 'Enter a whole engine-unit value'))
                continue
            candidate = copy.deepcopy(self.profile)
            candidate.pop('artwork', None)
            geometry = candidate['geometry']
            if field in CAMERA_FIELDS:
                geometry.setdefault('camera', {})[field] = int(text)
            else:
                geometry[field] = int(text)
            for diagnostic in validate_profile({'stadium_profile': candidate}, self.pack_dir):
                errors.append(Diagnostic(diagnostic.severity,
                    ('stadiums', self.index)+diagnostic.path, diagnostic.message))
        if not errors:
            errors = [Diagnostic(d.severity, ('stadiums', self.index)+d.path, d.message)
                      for d in validate_profile(self.stadium, self.pack_dir)]
        return errors

    @property
    def ready_for_test(self):
        return not any(d.severity == 'error' for d in self.diagnostics())

    def begin_drag(self):
        self.document.begin_edit()
        self._dragging = True

    def drag_dimension(self, field, value):
        if field not in ('length_units', 'width_units'):
            raise ValueError('Only pitch dimensions can be dragged')
        snapped = int(round(value/32))*32
        self.set_text(field, str(snapped))
        return snapped

    def end_drag(self):
        if self._dragging:
            self.document.commit_edit()
            self._dragging = False

    def reset_geometry(self):
        original = template_profile(self.profile['base_layout'])['geometry']
        geometry = copy.deepcopy(self.geometry)
        camera = dict(geometry.get('camera', {}))
        camera.update(original.pop('camera'))
        geometry.update(original)
        geometry['camera'] = camera
        self.document.set_value(self.prefix+('geometry',), geometry)
        self.raw.clear()


import tkinter as tk
from tkinter import ttk, filedialog
from PIL import Image, ImageTk
from .stadium_assets import compile_manifest, import_image, create_artwork, recompile_artwork, SLOTS
from .stadium_geometry import preview_artwork_scene


class StadiumWorkspace(ttk.Frame):
    """Persistent geometry canvas bound to the document's undo transactions."""
    def __init__(self, parent, document, index, on_change, pack_dir=None, draft=None, get_rom=None):
        super().__init__(parent)
        self.draft = draft or StadiumDraft(document, index, pack_dir)
        self.on_change = on_change
        self.get_rom = get_rom
        self.controls, self.variables = {}, {}
        self._syncing = False
        self._drag_field = None
        self._scale, self._origin = .2, (36, 36)
        toolbar = ttk.Frame(self)
        toolbar.pack(fill='x')
        self.base = tk.StringVar(value=str((self.draft.profile or {}).get('base_layout',
                                                self.draft.stadium.get('stadium_id', 0)%8)))
        ttk.Label(toolbar, text='Original layout').pack(side='left')
        ttk.Combobox(toolbar, textvariable=self.base, values=list(range(8)),
                     width=4, state='readonly').pack(side='left', padx=6)
        ttk.Button(toolbar, text='Create independent profile', command=self._enable).pack(side='left')
        ttk.Button(toolbar, text='Reset geometry', command=self._reset).pack(side='left', padx=6)
        self.notebook = ttk.Notebook(self)
        self.notebook.pack(fill='x',pady=8)
        self.geometry_tab,self.artwork_tab = ttk.Frame(self.notebook),ttk.Frame(self.notebook)
        self.notebook.add(self.geometry_tab,text='Geometry')
        self.notebook.add(self.artwork_tab,text='Artwork')
        self.canvas = tk.Canvas(self.geometry_tab, height=230, background='#153326',
                                highlightthickness=0, takefocus=True)
        self.canvas.pack(fill='x', pady=8)
        self.canvas.bind('<Configure>', lambda event: self.redraw())
        self.canvas.bind('<ButtonPress-1>', self._press)
        self.canvas.bind('<B1-Motion>', self._drag)
        self.canvas.bind('<ButtonRelease-1>', self._release)
        self.canvas.bind('<Escape>', self._cancel_drag)
        ttk.Label(self.geometry_tab, text='Geometry overlay · drag an edge · 32 engine units per step · dashed box: initial native camera').pack(anchor='w')
        fields = ttk.Frame(self.geometry_tab)
        fields.pack(fill='x', pady=8)
        for row, field in enumerate(('length_units', 'width_units')+CAMERA_FIELDS):
            ttk.Label(fields, text=field.replace('_', ' ').capitalize()).grid(row=row,column=0,sticky='w',padx=(0,10))
            variable = tk.StringVar()
            control = ttk.Entry(fields, textvariable=variable, width=12)
            control.grid(row=row,column=1,sticky='w',pady=2)
            self.controls[field], self.variables[field] = control, variable
            variable.trace_add('write', lambda *args, name=field: self._typed(name))
        self.validation = tk.StringVar()
        ttk.Label(self,textvariable=self.validation,wraplength=620,justify='left').pack(anchor='w',pady=6)
        self.artwork = tk.StringVar()
        ttk.Label(self.artwork_tab,textvariable=self.artwork,wraplength=620,justify='left').pack(anchor='w')
        art = ttk.Frame(self.artwork_tab)
        art.pack(fill='x',pady=6)
        ttk.Button(art,text='Create editable artwork',command=self._create_artwork).pack(side='left',padx=(0,8))
        ttk.Button(art,text='Recompile pitch maps',command=self._recompile_artwork).pack(side='left',padx=(0,8))
        ttk.Button(art,text='Attach artwork manifest…',command=self._attach_artwork).pack(side='left')
        uploads = ttk.Frame(self.artwork_tab)
        uploads.pack(fill='x',pady=6)
        self.slot = tk.StringVar(value='0')
        ttk.Label(uploads,text='Upload slot').pack(side='left',padx=(10,3))
        self.slot_control = ttk.Combobox(uploads,textvariable=self.slot,values=list(range(3)),width=3,state='readonly')
        self.slot_control.pack(side='left')
        self.bank = tk.StringVar(value='0')
        ttk.Label(uploads,text='Palette bank').pack(side='left',padx=(10,3))
        self.bank_control = ttk.Combobox(uploads,textvariable=self.bank,values=(0,1,2),width=3,state='readonly')
        self.bank_control.pack(side='left')
        self.hd_scale = tk.StringVar(value='1')
        ttk.Label(uploads,text='Image scale').pack(side='left',padx=(8,3))
        ttk.Combobox(uploads,textvariable=self.hd_scale,values=('1','2','4','8'),width=3,state='readonly').pack(side='left')
        ttk.Button(uploads,text='Import tile atlas…',command=self._import_artwork).pack(side='left',padx=8)
        placement_toolbar = ttk.Frame(self.artwork_tab)
        placement_toolbar.pack(fill='x',pady=4)
        self.place_image = tk.BooleanVar(value=False)
        ttk.Checkbutton(placement_toolbar,text='Place imported image',variable=self.place_image).pack(side='left')
        self.place_layer = tk.StringVar(value='1')
        ttk.Combobox(placement_toolbar,textvariable=self.place_layer,values=('0','1'),
                     width=3,state='readonly').pack(side='left',padx=5)
        self.place_x,self.place_y = tk.StringVar(value='0'),tk.StringVar(value='0')
        for label,variable in (('Native x',self.place_x),('y',self.place_y)):
            ttk.Label(placement_toolbar,text=label).pack(side='left',padx=3)
            ttk.Entry(placement_toolbar,textvariable=variable,width=6).pack(side='left')
        self.art_preview = ttk.Label(self.artwork_tab)
        self.art_preview.pack(anchor='w')
        self.art_usage = tk.StringVar()
        ttk.Label(self.artwork_tab,textvariable=self.art_usage,wraplength=620).pack(anchor='w')
        preview_toolbar = ttk.Frame(self.artwork_tab)
        preview_toolbar.pack(fill='x',pady=6)
        ttk.Button(preview_toolbar,text='Preview compiled stadium',command=self._preview_scene).pack(side='left')
        self.preview_layer = tk.StringVar(value='Combined')
        self.preview_region = tk.StringVar(value='Whole stadium')
        ttk.Combobox(preview_toolbar,textvariable=self.preview_layer,
                     values=('Combined','BG1','BG2'),state='readonly',width=10).pack(side='left',padx=6)
        ttk.Combobox(preview_toolbar,textvariable=self.preview_region,
                     values=('Whole stadium','Initial native camera','Left goal','Center','Right goal'),
                     state='readonly',width=15).pack(side='left')
        self.scene_preview = ttk.Label(self.artwork_tab)
        self.scene_preview.pack(anchor='w')
        self.scene_preview.bind('<Button-1>',self._pick_art_position)
        self.scene_status = tk.StringVar(value='Private day/fine backgrounds · sprites and animated scenery are verified in the game.')
        ttk.Label(self.artwork_tab,textvariable=self.scene_status,wraplength=620).pack(anchor='w')
        self.refresh()

    @property
    def ready_for_test(self):
        return self.draft.ready_for_test

    def refresh(self):
        self._syncing = True
        profile = self.draft.profile
        for field, variable in self.variables.items():
            value = ''
            if profile:
                if field in CAMERA_FIELDS:
                    value = self.draft.geometry.get('camera', {}).get(field,
                        TEMPLATES[profile['base_layout']][2+CAMERA_FIELDS.index(field)])
                else:
                    value = self.draft.geometry.get(field, '')
            variable.set(self.draft.raw.get(field, str(value)))
            self.controls[field].configure(state='normal' if profile else 'disabled')
        self._syncing = False
        self._validate()
        self.redraw()
        self._refresh_artwork()
        self.scene_preview.configure(image='')
        self._preview_mapping = None
        self.scene_status.set('Preview needs updating after edits. Private day/fine backgrounds; test animations in the game.')

    def _preview_scene(self):
        if not self.draft.ready_for_test or not self.draft.profile:
            self.scene_status.set('Correct the stadium diagnostics before previewing.')
            return
        rom = self.get_rom() if self.get_rom else None
        if rom is None:
            self.scene_status.set('Choose your cartridge to preview template graphics.')
            return
        try:
            compiled = None
            if self.draft.profile.get('artwork'):
                source = self._art_source()
                compiled = compile_manifest(json.loads(source.read_text(encoding='utf-8')),source.parent)
            layer = {'Combined':None,'BG1':0,'BG2':1}[self.preview_layer.get()]
            region = None
            selection = self.preview_region.get()
            if selection == 'Initial native camera':
                geometry=self.draft.geometry
                region=((geometry['length_units']+geometry['width_units'])//2+32,384,256,224)
            elif selection != 'Whole stadium':
                length = self.draft.geometry['length_units']
                center = {'Left goal':320,'Center':length//2+352,'Right goal':length+352}[selection]
                region = (max(0,center-256),224,512,640)
            image = preview_artwork_scene(rom,self.draft.profile,compiled,layer,region)
            original_size = image.size
            image.thumbnail((620,260),Image.Resampling.NEAREST)
            self._preview_mapping = (region or (0,0,*original_size),original_size,image.size,layer)
            self._scene_photo = ImageTk.PhotoImage(image)
            self.scene_preview.configure(image=self._scene_photo)
            self.scene_status.set(f'Compiled {self.preview_layer.get()} · {selection} · {original_size[0]}×{original_size[1]} native pixels. Day/fine static backgrounds.')
        except (ValueError,OSError,KeyError) as exc:
            self.scene_status.set(str(exc))

    def _pick_art_position(self,event):
        if not getattr(self,'_preview_mapping',None):
            return
        region,original,shown,layer = self._preview_mapping
        if not 0 <= event.x < shown[0] or not 0 <= event.y < shown[1]:
            return
        offset = 16 if layer is None and self.place_layer.get() == '0' else 0
        self.place_x.set(str((region[0]+int(event.x*original[0]/shown[0])+offset)//8*8))
        self.place_y.set(str((region[1]+int(event.y*original[1]/shown[1])+offset)//8*8))
        self.place_image.set(True)

    def _art_source(self):
        if not self.draft.profile or not self.draft.pack_dir:
            raise ValueError('Save the pack and create an independent profile first')
        name = self.draft.profile.get('artwork')
        if not name:
            raise ValueError('Attach an authored artwork manifest first')
        from .stadium_assets import resolve_asset
        return resolve_asset(self.draft.pack_dir,name)

    def _create_artwork(self):
        if not self.draft.profile or not self.draft.pack_dir:
            self.validation.set('Save the pack and create an independent profile first.')
            return
        if self.draft.profile.get('artwork'):
            self.validation.set('This stadium already has artwork. Import an atlas to edit it.')
            return
        rom = self.get_rom() if self.get_rom else None
        if rom is None:
            self.validation.set('Choose your cartridge to initialize template map references.')
            return
        destination = self.draft.pack_dir/('stadium-art-'+uuid.uuid4().hex[:8])
        result = create_artwork(self.draft.profile,rom,destination)
        if result.diagnostics:
            self.validation.set('\n'.join(d.message for d in result.diagnostics))
            return
        self.draft.attach_artwork((destination/'stadium.json').relative_to(self.draft.pack_dir).as_posix())
        self.on_change()
        self.refresh()

    def _attach_artwork(self):
        if not self.draft.pack_dir or not self.draft.profile:
            self.validation.set('Save the pack and create an independent profile first.')
            return
        chosen = filedialog.askopenfilename(title='Choose authored manifest inside the pack folder',
            initialdir=self.draft.pack_dir,filetypes=[('Artwork manifest','*.json')],parent=self)
        if not chosen:
            return
        try:
            name = Path(chosen).resolve().relative_to(self.draft.pack_dir.resolve()).as_posix()
            self.draft.attach_artwork(name)
        except (ValueError,OSError) as exc:
            self.validation.set(str(exc))
            return
        self.on_change()
        self.refresh()

    def _recompile_artwork(self):
        try:
            source = self._art_source()
        except ValueError as exc:
            self.validation.set(str(exc))
            return
        rom = self.get_rom() if self.get_rom else None
        if rom is None:
            return
        destination = self.draft.pack_dir/('stadium-art-'+uuid.uuid4().hex[:8])
        result = recompile_artwork(self.draft.profile,rom,source,destination)
        if result.diagnostics:
            self.validation.set('\n'.join(d.message for d in result.diagnostics))
            return
        self.draft.attach_artwork((destination/'stadium.json').relative_to(self.draft.pack_dir).as_posix())
        self.on_change()
        self.refresh()

    def _import_artwork(self):
        try:
            source = self._art_source()
        except ValueError as exc:
            self.validation.set(str(exc))
            return
        chosen = filedialog.askopenfilename(title='Import an 8px tile atlas',parent=self,
            filetypes=[('Images','*.png *.bmp *.jpg *.jpeg *.webp')])
        if not chosen:
            return
        destination = self.draft.pack_dir/('stadium-art-'+uuid.uuid4().hex[:8])
        try:
            placement = ({'layer':int(self.place_layer.get()),'x':int(self.place_x.get()),
                          'y':int(self.place_y.get())} if self.place_image.get() else None)
        except ValueError:
            self.validation.set('Enter whole native-pixel coordinates for image placement.')
            return
        result = import_image(chosen,source,destination,int(self.slot.get()),int(self.bank.get()),placement,int(self.hd_scale.get()))
        errors = [d.message for d in result.diagnostics if d.severity == 'error']
        if errors:
            self.validation.set('\n'.join(errors))
            return
        name = (destination/'stadium.json').relative_to(self.draft.pack_dir).as_posix()
        self.draft.attach_artwork(name)
        self.on_change()
        self.refresh()
        warnings = [d.message for d in result.diagnostics if d.severity == 'warning']
        if warnings:
            self.validation.set('\n'.join(warnings))

    def _refresh_artwork(self):
        profile = self.draft.profile
        if not profile:
            return
        base = profile.get('base_layout',0)
        if type(base) is not int or not 0 <= base < 8:
            return
        self.slot_control.configure(values=list(range(len(SLOTS[base]))))
        self.bank_control.configure(values=list(range(2 if base in (2,6) else 3)))
        try:
            source = self._art_source()
            compiled = compile_manifest(json.loads(source.read_text(encoding='utf-8')),source.parent)
            if compiled.diagnostics:
                raise ValueError('Correct artwork resource errors to preview it')
            self._art_photo = ImageTk.PhotoImage(compiled.preview.resize(
                (compiled.preview.width*2,compiled.preview.height*2),Image.Resampling.NEAREST))
            self.art_preview.configure(image=self._art_photo)
            usage = ', '.join(f'slot{write.slot}: {len(write.data)//32} tiles / {SLOTS[base][write.slot][1]}'
                              for write in compiled.tile_writes)
            self.art_usage.set('Decoded native upload atlas · '+usage+f' · {len(compiled.hd)} local HD tiles')
        except (ValueError,OSError,json.JSONDecodeError) as exc:
            self.art_preview.configure(image='')
            self.art_usage.set(str(exc))

    def _enable(self):
        if self.draft.profile:
            self.validation.set('This stadium already has a profile. Reset geometry preserves its artwork.')
            return
        self.draft.enable_profile(int(self.base.get()))
        self.on_change()
        self.refresh()

    def _reset(self):
        if self.draft.profile:
            self.draft.reset_geometry()
            self.on_change()
            self.refresh()

    def _typed(self, field):
        if self._syncing:
            return
        if self.draft.set_text(field, self.variables[field].get()):
            self.on_change()
        self._validate()
        self.redraw()

    def _validate(self):
        errors = self.draft.diagnostics()
        self.validation.set('\n'.join(d.message for d in errors) if errors else
                            ('Geometry values validated' if self.draft.profile else
                             'Uses the original gameplay layout. Display yards are separate.'))
        profile = self.draft.profile or {}
        self.artwork.set('Artwork: '+(profile.get('artwork') or 'template graphics with compiled pitch markings'))
        self.event_generate('<<StadiumValidation>>')

    def focus_diagnostic(self, diagnostic):
        field = diagnostic.path[-1] if diagnostic.path else None
        control = self.controls.get(field)
        if control:
            control.focus_set()
            control.selection_range(0, 'end')
            return True
        return False

    def redraw(self):
        if not self.winfo_exists():
            return
        geometry = self.draft.geometry if self.draft.profile else template_profile(int(self.base.get()))['geometry']
        length, width = geometry['length_units'], geometry['width_units']
        self._scale = min(max(1,self.canvas.winfo_width()-72)/(length+width+160), 170/(width+160))
        self._origin = (40+80*self._scale, 30+80*self._scale)
        def point(u,v):
            return world_to_canvas(u,v,self._scale,self._origin)
        self.canvas.delete('all')
        points = [point(0,0), point(length,0), point(length,width), point(0,width)]
        self.canvas.create_polygon(*[coordinate for p in points for coordinate in p],
                                   fill='#237343',outline='#e9f5ea',width=2)
        self.canvas.create_line(*point(length/2,0),*point(length/2,width),fill='#e9f5ea')
        # BG2 map space adds (160,224) to the original sheared field
        # projection. Initial camera X follows the patched ROM center table.
        camera_x,camera_y=(length+width)//2+32,384
        left=self._origin[0]+(camera_x-160)*self._scale
        top=self._origin[1]+(camera_y-224)*self._scale
        self.canvas.create_rectangle(left,top,left+256*self._scale,top+224*self._scale,
                                     outline='#64c8ff',dash=(4,3),width=2,tags='camera-window')
        circle = [point(length/2+96*math.cos(i*math.pi/24),width/2+96*math.sin(i*math.pi/24))
                  for i in range(49)]
        self.canvas.create_line(*[c for p in circle for c in p],fill='#e9f5ea')
        for u, depth in ((0,-72),(length,72)):
            goal = [point(u,width/2-48),point(u+depth,width/2-48),
                    point(u+depth,width/2+48),point(u,width/2+48)]
            self.canvas.create_line(*[c for p in goal for c in p],fill='#e9f5ea',width=2)
        self._handles = {'length_units': point(length,width/2),
                         'width_units': point(length/2,width)}
        for x,y in self._handles.values():
            self.canvas.create_oval(x-5,y-5,x+5,y+5,fill='#ffd166',outline='')

    def _press(self, event):
        if not self.draft.profile:
            return
        for field, point in self._handles.items():
            if math.hypot(event.x-point[0],event.y-point[1]) <= 14:
                self._drag_field = field
                self.draft.begin_drag()
                self.canvas.focus_set()
                break

    def _drag(self, event):
        if self._drag_field:
            u,v = canvas_to_world(event.x,event.y,self._scale,self._origin)
            self.draft.drag_dimension(self._drag_field,u if self._drag_field == 'length_units' else v)
            self.refresh()

    def _release(self, event):
        if self._drag_field:
            self.draft.end_drag()
            self._drag_field = None
            self.on_change()

    def _cancel_drag(self, event):
        if self._drag_field:
            self.draft.document.cancel_edit()
            self.draft._dragging = False
            self._drag_field = None
            self.draft.raw.clear()
            self.refresh()
