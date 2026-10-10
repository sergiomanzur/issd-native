"""Generate original example stadium artwork without reading a cartridge."""
import json,struct
from pathlib import Path
from .stadium_assets import compile_manifest


def write_example(destination,logical,length,accent):
    destination=Path(destination);destination.mkdir(parents=True,exist_ok=False)
    glyphs={};pixels=[];blocks={};definitions=bytearray();world=bytearray()
    def color(x,y):
        u,v=x-y+64,y+4-224
        inside=0<=u<=length and 0<=v<=576
        if not inside:return 4 if ((x//16+y//16)%2) else 2
        line= min(u,length-u,v,576-v)<=2 or abs(u-length/2)<=1
        for end in (0,length):
            depth=u if end==0 else length-u
            line|=(abs(depth-384)<=1 and abs(v-288)<=192) or (0<=depth<=384 and abs(abs(v-288)-192)<=1)
        return 3 if line else (1 if u//128%2 else 2)
    for index in range(4096):
        x=((index%704)//64)*256+(index&7)*32
        y=(index//704)*256+((index&63)>>3)*32
        words=[]
        for dy in range(0,32,8):
            for dx in range(0,32,8):
                glyph=bytes(color(x+dx+px,y+dy+py) for py in range(8) for px in range(8))
                if glyph not in glyphs:glyphs[glyph]=len(glyphs);pixels.append(glyph)
                words.append(0x0800+512+glyphs[glyph])
        block=struct.pack('<16H',*words)
        if block not in blocks:blocks[block]=len(blocks);definitions.extend(block)
        world.append(blocks[block])
    assert len(glyphs)<=95 and len(blocks)<=256,(len(glyphs),len(blocks))
    tiledata=bytearray()
    for glyph in pixels:
        planes=bytearray(32)
        for y in range(8):
            for x in range(8):
                for bit in range(4):planes[(bit//2)*16+y*2+bit%2]|=((glyph[y*8+x]>>bit)&1)<<(7-x)
        tiledata.extend(planes)
    definitions.extend(bytes(8192-len(definitions)))
    # BG1 stays transparent through palette color0; it references another
    # owned tile with all-zero pixels, independently of the borrowed template.
    transparent=len(glyphs);assert transparent<95;tiledata.extend(bytes(32))
    files={'bg2-defs.bin':definitions,'bg2-map.bin':world,
           'bg1-defs.bin':struct.pack('<H',0x0800+512+transparent)*4096,
           'bg1-map.bin':bytes(4096),'characters.bin':tiledata}
    rgb=[(0,0,0),(24,130,58),(18,96,44),(245,248,233),accent]
    palette=bytearray(96)
    for index,(r,g,b) in enumerate(rgb):struct.pack_into('<H',palette,index*2,(r>>3)|((g>>3)<<5)|((b>>3)<<10))
    files['palette.bin']=palette
    for name,data in files.items():(destination/name).write_bytes(data)
    manifest={'version':1,'base_layout':0,'allocation_profile':'original-scenery-v1',
        'geometry':{'length_units':length,'width_units':576},
        'tiles':[{'slot':0,'offset_tiles':0,'tile_count':len(tiledata)//32,'file':'characters.bin'}],
        'palette':'palette.bin'}
    # Native layer0=BG1 and layer1=BG2.
    manifest['layers']=[{'layer':0,'pages_x':11,'pages_y':5,'metatiles':'bg1-defs.bin','world_map':'bg1-map.bin'},
                        {'layer':1,'pages_x':11,'pages_y':5,'metatiles':'bg2-defs.bin','world_map':'bg2-map.bin'}]
    assert not compile_manifest(manifest,destination).diagnostics
    (destination/'stadium.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return {'stadium_id':logical,'name':'MEADOW' if logical==8 else 'HARBOR',
            'stadium_profile':{'version':1,'base_layout':0,'geometry':{'length_units':length,'width_units':576},
                              'artwork':f'{destination.name}/stadium.json'}}


def write_pack(destination):
    destination=Path(destination);destination.mkdir(parents=True,exist_ok=False)
    entries=[write_example(destination/'meadow',8,1728,(30,72,150)),
             write_example(destination/'harbor',31,1664,(170,70,35))]
    pack={'name':'Independent stadium examples','stadium_count':32,'teams':[],'stadiums':entries}
    (destination/'mod.json').write_text(json.dumps(pack,indent=2)+'\n')
    return pack
