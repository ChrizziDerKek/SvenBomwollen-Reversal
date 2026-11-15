"""Author five distinct WeekendAdventure scenes from original artwork.
Only writes rebuild/examplepacks/WeekendAdventure. Run with python -B.
"""
from pathlib import Path
from collections import deque
from functools import lru_cache
import argparse,copy,hashlib,json,math,struct,xml.etree.ElementTree as ET
import create_parity_pack as codec
ROOT=codec.ROOT
OUT=ROOT/'rebuild/examplepacks/WeekendAdventure'
THEMES={'Farm':('01','Farm'),'Barn':('01','Barn'),'River':('02','Delta'),'Forest':('XXL','Forest'),'Lab':('02','Ufo')}

def digest(b): return hashlib.sha256(b).hexdigest()
def field(r,o): return struct.unpack_from('<i',r,o)[0]

@lru_cache(None)
def bitmap(path):
    b=(codec.ASSETS/path).read_bytes();off=field(b,10);w,h=struct.unpack_from('<ii',b,18);bits=struct.unpack_from('<H',b,28)[0]
    assert b[:2]==b'BM' and field(b,30)==0 and bits in (8,24,32)
    stride=((w*bits+31)//32)*4
    palette=[tuple(reversed(b[i:i+3])) for i in range(54,off,4)] if bits==8 else None
    def rgb(x,y):
        if not(0<=x<w and 0<=y<abs(h)):return (255,0,255)
        pos=off+(abs(h)-1-y if h>0 else y)*stride+x*(bits//8)
        return palette[b[pos]] if palette else tuple(reversed(b[pos:pos+3]))
    return w,abs(h),rgb

def keyed(rgb):return (rgb[0]&248,rgb[1]&252,rgb[2]&248)==(248,0,248)

class Catalog:
    def __init__(self):
        self.themes={};self.samples={};self.references=[]
        for name,(pkg,ts) in THEMES.items():
            path=next((codec.ASSETS/pkg).glob('*.xml'));root=ET.fromstring(path.read_bytes().decode('utf8',errors='replace'))
            tiles=copy.deepcopy(next(t for t in root.findall('.//tileset') if t.get('name')==ts));tiles.set('name',name)
            ET.SubElement(tiles,'tile',file='Sven2/gfx/icons.bmp',frames='5');self.themes[name]=tiles;self.samples[name]={}
            for level in root.findall('.//level'):
                if level.get('tileset')!=ts:continue
                map=level.get('map');data=(codec.ASSETS/map).read_bytes();base=16+struct.unpack_from('<I',data,12)[0]
                for slot in range(256):
                    r=data[base+9048+slot*100:base+9048+(slot+1)*100]
                    if field(r,44)==1 and field(r,16)==0:
                        self.samples[name].setdefault(field(r,68),[]).append((map,slot,r,data,base))
    def tile(self,theme,texture):return self.themes[theme].findall('tile')[texture]
    def prototype(self,theme,texture):
        candidates=self.samples[theme].get(texture,[])
        # Prefer unobscured in-world placements with feet-level grid collision.
        scored=[]
        for map,slot,r,data,base in candidates:
            x,y=struct.unpack_from('<2f',r);w,h=struct.unpack_from('<2i',r,20)
            phase=(int(x)%40,int(y)%40);ox,oy=math.floor(x/40),math.floor(y/40)
            rows=math.ceil((phase[1]+h)/40);cols=math.ceil((phase[0]+w)/40)
            mask=[]
            for dy in range(max(0,rows-2),rows):
                for dx in range(cols):
                    cx,cy=ox+dx,oy+dy
                    if 0<=cx<20 and 0<=cy<15 and field(data,base+codec.raw(cx,cy)*24)==1:mask.append((dx,dy))
            if mask and 0<=x and 0<=y and x+w<=800 and y+h<=600 and field(r,92)<400:
                scored.append((len(mask),map,slot,r,phase,mask))
        if scored:
            _,map,slot,r,phase,mask=min(scored,key=lambda a:(a[0],a[1],a[2]))
        elif candidates:
            map,slot,r,_,_=candidates[0];x,y=struct.unpack_from('<2f',r);phase=(int(x)%40,int(y)%40);mask=[]
        else:
            r=bytearray(100);w,h,_=bitmap(self.tile(theme,texture).get('file'));codec.integer(r,20,w);codec.integer(r,24,h);codec.integer(r,44,1);codec.integer(r,68,texture);codec.integer(r,92,20)
            map,slot,phase,mask='constructor',None,(0,0),[]
        return bytearray(r),phase,mask,{'map':map,'slot':slot,'texture':texture}

class Scene(codec.Level):
    def __init__(self,catalog,actors,theme,title):
        super().__init__(actors,{})
        self.catalog,self.theme,self.title=catalog,theme,title
        self.placements=[];self.scenery=[];self.start=None;self.bridges=[];self.features=[];self.land_surfaces=[]
        codec.real(self.data,self.base+9044,540)
    def prop(self,texture,x,y,mask=None,depth=None,pixels=False,verify_reference=False):
        r,phase,native,source=self.catalog.prototype(self.theme,texture)
        px,py=(x,y) if pixels else (40*x+phase[0],40*y+phase[1])
        if verify_reference:
            assert (px%40,py%40)==phase,'A sampled footprint requires the original pixel phase'
            _,_,original,data,base=next(v for v in self.catalog.samples[self.theme][texture] if v[0]==source['map'] and v[1]==source['slot'])
            sx,sy=struct.unpack_from('<2f',original);ox,oy=math.floor(sx/40),math.floor(sy/40)
            for dx,dy in mask:
                assert field(data,base+codec.raw(ox+dx,oy+dy)*24)==1,(source,'footprint differs from original',dx,dy)
        for off,val in [(0,px),(4,py),(8,px),(12,py)]:codec.real(r,off,val)
        for off,val in [(16,0),(44,1),(48,0),(68,texture),(72,0)]:codec.integer(r,off,val)
        if depth is not None:codec.integer(r,92,depth)
        slot=self.count;begin=self.base+9048+slot*100;assert slot<256
        self.data[begin:begin+100]=r;self.count+=1
        footprint=native if mask is None else mask
        ox,oy=math.floor(px/40),math.floor(py/40);solid=[]
        for dx,dy in footprint:
            cx,cy=ox+dx,oy+dy
            if 0<=cx<20 and 0<=cy<15 and self.cells[cy][cx]!='~':self.tile(cx,cy,1,'#');solid.append((cx,cy))
        self.scenery.append({'texture':texture,'asset':self.catalog.tile(self.theme,texture).get('file'),'origin':[px,py],'size':list(struct.unpack_from('<2i',r,20)),'collision_cells':solid,'reference':source,'footprint':'native lower-band sample' if mask is None else 'explicit authored support/structure'})
        if verify_reference:self.scenery[-1]['footprint']='selected original body cells, verified at identical pixel phase'
        return px,py
    def carve_land(self,texture,px,py,stone=False):
        self.prop(texture,px,py,mask=[],depth=1000,pixels=True)
        w,h,rgb=bitmap(self.catalog.tile(self.theme,texture).get('file'))
        self.land_surfaces.append((px,py,rgb,stone))
        for cy in range(15):
            for cx in range(20):
                def supported(dx,dy):
                    for ox,oy,pixel,is_stone in self.land_surfaces:
                        c=pixel(cx*40+20+dx-ox,cy*40+20+dy-oy)
                        if not keyed(c) and (is_stone or (c[1]>=c[0]*.95 and c[1]>70 and c[2]<140)):return True
                    return False
                # Test the assembled terrain, including samples across a seam.
                if supported(0,0) and sum(supported(dx,dy) for dx,dy in [(0,0),(-9,0),(9,0),(0,-9),(0,9)])>=4:self.tile(cx,cy,0,'.')
    def bridge(self,px,py,cells,texture=8):
        self.prop(texture,px,py,mask=[],depth=500,pixels=True)
        w,h,pixel=bitmap(self.catalog.tile(self.theme,texture).get('file'))
        for cx,cy in cells:
            sx,sy=cx*40+20-px,cy*40+20-py
            # Small transparent gaps between planks are decorative; the long
            # transparent tail of some bridge bitmaps is not a walkway.
            visible=0<=sx<w and 0<=sy<h and any(not keyed(pixel(sx+dx,sy+dy)) for dx,dy in [(0,0),(-7,0),(7,0),(0,-7),(0,7)])
            assert self.cells[cy][cx]!='~' or visible,(self.title,'bridge over transparent pixel',cx,cy)
            self.tile(cx,cy,0,'.')
        self.bridges.append({'origin':[px,py],'texture':texture,'cells':cells})
    def connected(self):
        if self.start is None:return {(x,y) for y in range(15) for x in range(20) if self.cells[y][x] not in '#~T'}
        seen={self.start};q=deque(seen)
        while q:
            x,y=q.popleft()
            for xx,yy in [(x-1,y),(x+1,y),(x,y-1),(x,y+1)]:
                if 0<=xx<20 and 0<=yy<15 and (xx,yy) not in seen and self.cells[yy][xx] not in '#~T':seen.add((xx,yy));q.append((xx,yy))
        return seen
    def place(self,kind,x,y,facing=None):
        reachable=self.connected()
        choices=[(abs(cx-x)+abs(cy-y),cy,cx) for cy in range(1,14) for cx in range(1,19) if self.cells[cy][cx]=='.' and (cx,cy) in reachable]
        assert choices,('no actor space',self.title,kind)
        _,y,x=min(choices);slot=self.count
        super().actor(kind,x,y)
        if kind==0:self.start=(x,y)
        r=self.base+9048+slot*100
        codec.integer(self.data,r+76,(x+y+kind)%4 if kind in (2,11,12,13) else (2 if facing is None else facing))
        if 7<=kind<=10:codec.integer(self.data,r+68,1)
        self.placements.append({'kind':kind,'cell':[x,y]})
    def puddle(self,x,y,texture):
        reachable=self.connected();assert (x,y) in reachable and self.cells[y][x]=='.',(self.title,'puddle',x,y)
        w,h,_=bitmap(self.catalog.tile(self.theme,texture).get('file'))
        self.prop(texture,x*40+20-w//2,y*40+20-h//2,mask=[],depth=500,pixels=True)
        self.tile(x,y,10,'T');self.destinations.append(codec.raw(x,y))
    def pickup(self,code,x,y):
        reachable=self.connected();assert (x,y) in reachable and self.cells[y][x]=='.',(self.title,'pickup',x,y,[''.join(r) for r in self.cells])
        r=bytearray(100);texture=len(self.catalog.themes[self.theme].findall('tile'))-1
        for off,value in [(20,80),(24,80),(44,1),(48,code-132),(68,texture),(80,codec.raw(x,y)),(92,40)]:codec.integer(r,off,value)
        for off,value in [(0,x*40-20),(4,y*40-20),(8,x*40-20),(12,y*40-20)]:codec.real(r,off,value)
        b=self.base+9048+self.count*100;self.data[b:b+100]=r
        codec.integer(self.data,self.base+codec.raw(x,y)*24+12,code);codec.integer(self.data,self.base+codec.raw(x,y)*24+16,self.count)
        self.count+=1;self.cells[y][x]='!'
    def check(self):
        payload=self.finish();assert self.count<=256 and len(self.destinations)<=16
        assert sum(p['kind']==0 for p in self.placements)==1
        assert any(p['kind'] in (2,11,12,13) for p in self.placements)
        assert len(set(self.destinations))==len(self.destinations)
        for t in self.destinations:assert field(payload,self.base+t*24)==10
        # Bush canopies must not become solid sprite-sized rectangles again.
        bushes={'Farm':{3,4,5},'River':{25,26},'Forest':{0,1}}.get(self.theme,set())
        for prop in self.scenery:
            if prop['texture'] not in bushes:continue
            px,py=prop['origin'];w,h=prop['size']
            canvas=math.ceil((px%40+w)/40)*math.ceil((py%40+h)/40)
            assert len(prop['collision_cells'])<canvas,(self.title,'solid bush canopy',prop)
        textures=self.catalog.themes[self.theme].findall('tile')
        for slot in range(self.count):
            r=payload[self.base+9048+slot*100:self.base+9048+(slot+1)*100]
            if field(r,44)==1:
                texture,frame=field(r,68),field(r,48)
                assert 0<=texture<len(textures) and 0<=frame<int(textures[texture].get('frames','1'))
        if self.theme=='River':assert len(self.destinations)>=2,'Water needs reachable rescue puddles'
        blocked={tuple(p['cell']) for p in self.placements if p['kind'] in (2,3,4,5,6,11,12,13)}
        seen={self.start};q=deque(seen)
        while q:
            x,y=q.popleft()
            for p in [(x-1,y),(x+1,y),(x,y-1),(x,y+1)]:
                xx,yy=p
                if 0<=xx<20 and 0<=yy<15 and p not in seen and p not in blocked and self.cells[yy][xx] not in '#~T':seen.add(p);q.append(p)
        for p in self.placements:
            if p['kind']==0:continue
            x,y=p['cell'];assert any(v in seen for v in [(x-1,y),(x+1,y),(x,y-1),(x,y+1)]),(self.title,'unreachable',p,[''.join(r) for r in self.cells])
            if 7<=p['kind']<=10:assert (x,y) in seen
        for y,row in enumerate(self.cells):
            for x,c in enumerate(row):
                if c=='!':assert (x,y) in seen
                if c=='T':assert any(p in seen for p in [(x-1,y),(x+1,y),(x,y-1),(x,y+1)])
        assert len(seen)>55,(self.title,'too little reachable area',len(seen))
        base=self.base
        assert len(payload)==base+34648 and field(payload,0)==6 and field(payload,8)==0
        for slot in range(self.count):
            r=payload[base+9048+slot*100:base+9048+(slot+1)*100];kind=field(r,44)
            if kind in (0,2,3,4,5,6,7,8,9,10,11,12,13):
                t=field(r,80);w,h=struct.unpack_from('<2i',r,20);x,y=struct.unpack_from('<2f',r)
                assert (x+w/2,y+h/2)==((t%22-1)*40+20,(t//22-1)*40+20)
        return payload,len(seen)


def build():
    actors,_,sources=codec.templates();cat=Catalog();scenes=[]
    # A farmyard with buildings framing a road, tractor loop and animal pens.
    l=Scene(cat,actors,'Farm','01 - Market Day Detour')
    l.prop(0,-64,-141,mask=[(x,y) for y in (1,2,3) for x in range(1,9)]+[(x,4) for x in range(1,8)]+[(x,5) for x in range(1,4)]+[(x,6) for x in range(1,3)],depth=500,pixels=True,verify_reference=True)
    # Preserve the original cottage pixel phase and its stepped ground footprint
    # (Bauernhof05v01 slot 24), including the gap below the front steps.
    l.prop(14,503,-131,mask=[(x,y) for y in (4,5) for x in range(8)]+[(x,6) for x in range(3,8)],depth=80,pixels=True,verify_reference=True)
    # Tractor footprint from Bauernhof04v01 slot 10, translated one cell east
    # and south. The old approximation missed the cab and front wheel.
    l.prop(19,315,172,mask=[(x,y) for y,xs in [(1,range(3,5)),(2,range(1,6)),(3,range(1,7)),(4,range(3,6))] for x in xs],depth=70,pixels=True,verify_reference=True)
    for x,y in [(1,6),(4,9),(13,8),(16,10)]:l.prop(12,x,y,mask=[(i,1) for i in range(3)],depth=20)
    for x,y in [(2,10),(16,3),(12,12),(1,4)]:l.prop(4,x,y)
    for x,y,t in [(0,5,20),(18,4,21),(17,10,20),(0,11,21)]:l.prop(t,x,y)
    for x,y,t in [(2,4,6),(3,4,7),(15,3,6),(15,11,7),(3,11,6),(2,8,7),(12,10,6)]:l.prop(t,x,y)
    l.prop(15,0,12,mask=[(x,2) for x in range(4)],depth=30)
    l.place(0,4,5)
    for kind,x,y in [(2,3,4),(11,6,3),(2,15,4),(12,16,8),(13,5,12),(2,13,12),(2,11,3),(11,18,12)]:l.place(kind,x,y)
    l.place(5,17,6,6);l.place(7,14,6);l.pickup(136,6,8)
    l.puddle(4,7,17);l.puddle(17,12,18)
    scenes.append(('01-garden-maze',l,'Tour the pens around the tractor and buildings. The central yard and southern road form bypasses; leave the beer for Lars.'))
    # Barn rails produce three connected pens with several open ends.
    l=Scene(cat,actors,'Barn','02 - Bales, Bones and Back Doors')
    for x,y in [(3,3),(10,3),(5,8),(12,8),(2,12),(13,12)]:l.prop(9,x,y,mask=[(i,0) for i in range(5)],depth=10)
    for x,y in [(3,4),(15,4),(9,9)]:l.prop(10,x,y,mask=[(0,0),(0,1),(1,1),(1,2)],depth=80)
    for x,y,t in [(0,0,5),(3,0,6),(16,0,5),(18,11,6),(0,10,6),(17,12,5)]:l.prop(t,x,y,mask=[(0,1),(1,1)]+([(2,1),(0,2),(1,2)] if t==5 else []),depth=30)
    for x,y in [(6,5),(15,10)]:l.prop(7,x,y,mask=[(1,0),(2,0),(2,1),(3,1)],depth=40)
    for x,y in [(10,1),(1,7),(17,6)]:l.prop(15,x,y,mask=[(1,2)],depth=10)
    # Ceiling stanchions and hanging ropes require matching architecture;
    # leave those out of this open pen layout rather than floating in midair.
    l.place(0,2,6)
    for kind,x,y in [(2,5,2),(2,12,2),(12,17,3),(13,6,7),(2,12,6),(11,3,11),(2,11,11),(13,17,11)]:l.place(kind,x,y)
    l.place(4,17,6,6);l.place(9,14,6);l.puddle(5,6,2);l.puddle(16,13,3)
    scenes.append(('02-bone-boulevard',l,'Use the back doors between rail pens. The dog guards the east pen, but a bone and two puddles give you escape routes.'))
    # New river topology is composed from individual island pieces, never an old grid.
    l=Scene(cat,actors,'River','03 - Three Islands and a Picnic')
    for y in range(15):
        for x in range(20):l.tile(x,y,11,'~')
    # Island textures are shoreline halves, not standalone rectangular islands.
    # Overlap their grass edges so no straight cut is exposed to the water.
    for x in [-115,275,665]:
        l.carve_land(31,x,100);l.carve_land(29,x+135,100)
    l.carve_land(34,285,440)
    l.bridge(135,190,[(x,5) for x in range(3,9)])
    l.bridge(505,190,[(x,5) for x in range(13,18)])
    l.bridge(485,310,[(x,8) for x in range(12,17)])
    l.bridge(345,350,[(9,y) for y in range(9,12)],texture=14)
    for x,y,t in [(1,3,25),(8,3,26),(17,6,25),(9,12,24)]:l.prop(t,x,y,mask=[] if t==24 else None)
    l.place(0,2,8)
    for kind,x,y in [(2,2,3),(2,10,3),(12,10,7),(2,18,4),(11,18,8),(13,8,12),(2,12,6)]:l.place(kind,x,y)
    for x,y in [(2,6),(10,5),(18,7)]:
        if l.cells[y][x]=='.' and (x,y) in l.connected():l.puddle(x,y,35)
    l.place(3,17,2,4);l.place(10,10,9)
    scenes.append(('03-puddle-islands',l,'Cross wooden bridges between three new islands. The southern picnic island is a side trip; puddles rescue a swim and offer optional shortcuts.'))
    # Dense forest framing with two groves and an open S-shaped central trail.
    l=Scene(cat,actors,'Forest','04 - The Sleepy Forester Trail')
    for i,x in enumerate(range(-1,20,1)):
        l.prop([6,9,7,10][i%4],x,-2,depth=500)
        l.prop([9,6,10,7][i%4],x,12+(i%2),depth=500)
    for i,y in enumerate(range(1,12,2)):
        for x in [-1,18]:l.prop([6,9,7][i%3],x,y,depth=500)
    for x,y in [(4,2),(5,3),(4,5),(5,6),(13,5),(14,6),(13,8),(14,9)]:l.prop(6 if (x+y)%2 else 9,x,y)
    for x,y in [(3,7),(11,2),(8,9),(15,4),(12,11)]:l.prop(4,x,y)
    l.prop(5,7,5);l.prop(1,15,10);l.prop(0,2,3)
    for x,y,t in [(2,9,2),(9,3,3),(7,10,2),(16,8,3),(9,12,12),(3,2,11)]:l.prop(t,x,y,mask=[])
    l.place(0,2,7)
    for kind,x,y in [(2,3,3),(2,8,2),(12,15,3),(2,10,6),(11,4,10),(2,9,11),(13,16,8),(2,16,7)]:l.place(kind,x,y)
    l.place(5,13,3,4);l.place(7,11,4);l.place(6,11,9,6);l.place(8,12,10)
    l.pickup(135,4,7);l.pickup(136,11,8);l.puddle(3,9,13);l.puddle(12,5,14)
    scenes.append(('04-sunday-distractions',l,'Follow the S-shaped clearing between dense groves. Stumps split the trail, Lars and Bromse patrol the eastern forks, and smell/flowers change your animal strategy.'))
    # A machinery-filled laboratory with outer lanes and three crate banks.
    l=Scene(cat,actors,'Lab','05 - The Rose Protocol')
    for x,y in [(0,-1),(1,-2),(17,-2),(18,-1),(0,9),(1,10),(18,9),(17,11)]:l.prop(2,x,y,mask=[(0,3),(1,3)],depth=0)
    for x,y in [(3,2),(5,2),(3,3),(5,3),(12,2),(14,2),(16,2),(14,3),(12,4),(2,10),(4,10),(6,10),(4,11),(6,11),(12,9),(14,9),(16,9),(16,10),(14,11)]:l.prop(0,x,y,mask=[(0,1),(1,1)],depth=10)
    for x,y,t in [(8,1,3),(9,2,4),(10,10,6),(7,8,5)]:l.prop(t,x,y,mask=[(0,2),(1,2)],depth=10)
    for x,y in [(2,5),(8,4),(15,5),(10,12)]:l.prop(7,x,y,mask=[(0,2)],depth=0)
    l.prop(8,337,224,mask=[(1,5),(2,5),(3,5)],depth=30,pixels=True,verify_reference=True)
    l.place(0,2,8)
    for kind,x,y in [(2,2,2),(2,9,1),(12,17,3),(2,7,5),(11,13,7),(13,17,12),(2,8,12),(2,17,7)]:l.place(kind,x,y)
    l.place(3,16,5,6);l.place(3,12,11,0);l.place(10,5,7)
    l.pickup(132,9,8);l.pickup(136,3,6);l.puddle(3,7,9);l.puddle(17,8,9)
    scenes.append(('05-alien-garden',l,'Steal the rose protocol through crate banks, glowing consoles and a central brain machine. Two aliens guard the laboratories; an artifact and rocket open a fast final circuit.'))

    root=ET.Element('sven2',version='1.0');package=ET.SubElement(root,'package',name='WeekendAdventure')
    episode=ET.SubElement(ET.SubElement(package,'levelsets'),'levelset',name='Weekend Adventure')
    tileRoot=ET.SubElement(package,'tilesets')
    for t in cat.themes.values():tileRoot.append(copy.deepcopy(t))
    manifest={'format':6,'revision':3,'episode':'Weekend Adventure','design':'Five newly composed scenes, original scenery/backgrounds; no original level grid copied','collision_policy':'Native placement phases and lower-band footprints for small props; explicitly authored structural supports. Sprite rectangles are never automatically made solid.','levels':[],'template_sources':sources}
    files={}
    for slug,l,instructions in scenes:
        data,reachable=l.check();name=l.theme.encode();data=data[:12]+struct.pack('<I',len(name))+name+data[l.base:]
        files['levels/'+slug+'.lvl']=data
        music={'Farm':'01/sfx/atmo_grass.ogg','Barn':'01/sfx/atmo_barn.ogg','River':'02/sfx/atmo_river.ogg','Forest':'XXL/sfx/Atmo_forrest.ogg','Lab':'02/sfx/atmo_UFO.ogg'}[l.theme]
        assert (codec.ASSETS/music).is_file()
        ET.SubElement(episode,'level',name=f'{len(manifest["levels"])+1:02}',tileset=l.theme,music=music,map='WeekendAdventure/levels/'+slug+'.lvl')
        manifest['levels'].append({'id':slug,'name':l.title,'theme':l.theme,'path':'WeekendAdventure/levels/'+slug+'.lvl','sha256':digest(data),'objects':l.count,'animals':sum(p['kind'] in (2,11,12,13) for p in l.placements),'initial_reachable_cells':reachable,'instructions':instructions,'layout':[''.join(r) for r in l.cells],'actors':l.placements,'scenery':l.scenery,'bridges':l.bridges})
    originals=[]
    for p in codec.ASSETS.glob('*/levels/*.lvl'):
        d=p.read_bytes()
        if field(d,0)==6:originals.append(d[16+field(d,12):][:8976])
    grids=[d[16+field(d,12):][:8976] for d in files.values()]
    assert len(set(grids))==5 and all(g not in originals for g in grids)
    ET.indent(root);files['WeekendAdventure.xml']=ET.tostring(root,encoding='utf-8',xml_declaration=True)+b'\n'
    manifest['files_sha256']={n:digest(d) for n,d in files.items()}
    manifest['source_resource_sha256']={t.get('file'):digest((codec.ASSETS/t.get('file')).read_bytes()) for ts in cat.themes.values() for t in ts.findall('tile')}
    files['manifest.json']=(json.dumps(manifest,indent=2)+'\n').encode()
    return files

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true');args=p.parse_args()
    files=build();assert files==build(),'Generation differs between runs'
    for name,data in files.items():
        path=OUT/name
        if args.check:assert path.read_bytes()==data,f'Stale generated file: {path}'
        else:path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
    print('WeekendAdventure v3: five themed maps; footprints, links, reachability, novelty and determinism passed.')
if __name__=='__main__':main()
