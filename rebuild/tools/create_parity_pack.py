"""Regenerate the authored ParityLab loose v6 package; never writes assets/."""
from pathlib import Path
import argparse
import copy
import hashlib
import json
import struct
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'rebuild/testpacks/ParityLab'
ASSETS = ROOT / 'assets'

def integer(data, offset, value):
    struct.pack_into('<i', data, offset, value)

def real(data, offset, value):
    struct.pack_into('<f', data, offset, value)

def raw(x, y):
    assert 0 <= x < 20 and 0 <= y < 15
    return 23 + y * 22 + x

def templates():
    actors, scenery, sources = {}, {}, {}
    for path in sorted(ASSETS.glob('*/levels/*.lvl')):
        data = path.read_bytes()
        if len(data) < 16 or struct.unpack_from('<i', data)[0] != 6:
            continue
        n = struct.unpack_from('<I', data, 12)[0]
        if len(data) != 16 + n + 34648:
            continue
        for slot in range(256):
            record = data[16+n+9048+slot*100:16+n+9048+(slot+1)*100]
            kind = struct.unpack_from('<i', record, 44)[0]
            if kind in range(14) and kind != 1 and kind not in actors:
                actors[kind] = record
                sources[str(kind)] = f'{path.relative_to(ROOT).as_posix()} slot {slot}'
            if path.parent.parent.name == 'XS' and kind == 1:
                texture = struct.unpack_from('<i', record, 68)[0]
                scenery.setdefault(texture, record)
    assert all(k in actors for k in [0,2,3,4,5,6,7,8,9,10,11,12,13])
    return actors, scenery, sources

class Level:
    def __init__(self, actors, scenery):
        self.actors, self.scenery = actors, scenery
        name = b'Download'
        self.base = 16 + len(name)
        self.data = bytearray(self.base + 34648)
        # Header +8 hides the UFO and its shadow, including artifact requests.
        # Keep them visible so the lab exercises the complete presentation.
        struct.pack_into('<4i', self.data, 0, 6, 0, 0, len(name))
        self.data[16:self.base] = name
        self.count = 0
        self.destinations = []
        self.cells = [['.' for _ in range(20)] for _ in range(15)]
        for t in range(374):
            integer(self.data, self.base+t*24, int(t%22 in (0,21) or t//22 in (0,16)))
            integer(self.data, self.base+t*24+16, -1)
        for i in range(16):
            integer(self.data, self.base+8980+i*4, -1)
        real(self.data, self.base+9044, 600)
        for slot in range(256):
            integer(self.data, self.base+9048+slot*100+44, -1)

    def tile(self, x, y, kind, symbol):
        integer(self.data, self.base+raw(x,y)*24, kind)
        self.cells[y][x] = symbol

    def object(self, template, x, y, linked=False):
        record = bytearray(template)
        for offset, value in [(0,x*40-40),(4,y*40-40),(8,x*40-40),(12,y*40-40)]:
            real(record, offset, value)
        if linked:
            integer(record, 80, raw(x,y))
        start = self.base+9048+self.count*100
        self.data[start:start+100] = record
        self.count += 1
        return self.count-1

    def actor(self, kind, x, y):
        record = bytearray(self.actors[kind])
        integer(record, 16, 0)
        integer(record, 72, 0) # ordinary standing state
        integer(record, 76, 0)
        if kind in (2,11,12,13):
            record = bytearray(100)
            for off, value in [(20,120),(24,120),(44,kind),(60,4),(68,-1),(84,6),(92,40)]:
                integer(record, off, value)
            real(record,52,900 if kind==2 else 0)
            real(record,56,20 if kind==2 else 15)
            real(record,88,25)
        slot = self.object(record,x,y,True)
        if kind == 5:
            # Farmer's original 160x160 canvas centers at origin+(80,80).
            start = self.base+9048+slot*100
            for off,value in [(0,x*40-60),(4,y*40-60),(8,x*40-60),(12,y*40-60)]:
                real(self.data,start+off,value)
        if 7 <= kind <= 10:
            # Original fixed items are 80x80, not 120x120 actors. Keep their
            # centers on the linked cell (XS tutorial01v04 slots 37/38/47).
            start = self.base+9048+slot*100
            for off,value in [(0,x*40-20),(4,y*40-20),(8,x*40-20),(12,y*40-20)]:
                real(self.data,start+off,value)
            integer(self.data,self.base+raw(x,y)*24+12,{7:130,8:131,9:129,10:128}[kind])
            integer(self.data,self.base+raw(x,y)*24+16,slot)
            self.cells[y][x] = str(kind%10)
        elif kind:
            self.tile(x,y,kind if kind<=6 else kind-4, {2:'s',3:'A',4:'D',5:'F',6:'B',11:'c',12:'g',13:'p'}[kind])
            integer(self.data,self.base+raw(x,y)*24+8,slot)
        else:
            self.cells[y][x] = '@'

    def bush(self, x, y):
        self.object(self.scenery[5],x,y)
        for dx in (-1,0,1):
            for dy in (-1,0,1):
                self.tile(x+dx,y+dy,1,'#')

    def teleport(self,x,y,water=False):
        self.tile(x,y,11 if water else 10,'W' if water else 'T')
        slot = self.object(self.scenery[16],x,y)
        # Center the 80x40 marker on the trigger, independently of actor origin.
        start = self.base+9048+slot*100
        for off,value in [(0,x*40-20),(4,y*40),(8,x*40-20),(12,y*40)]:
            real(self.data,start+off,value)
        if not water:
            self.destinations.append(raw(x,y))

    def effect(self, code, x, y):
        # Original-valid scenery record; pickup semantics come from the grid.
        record = bytearray(100)
        for off, value in [(20,80),(24,80),(44,1),(48,code-132),(68,31),(92,40)]:
            integer(record,off,value)
        slot=self.object(record,x,y,True)
        # Cell center is x*40+20; an 80px icon starts 40px before it.
        start=self.base+9048+slot*100
        for off,value in [(0,x*40-20),(4,y*40-20),(8,x*40-20),(12,y*40-20)]:
            real(self.data,start+off,value)
        integer(self.data,self.base+raw(x,y)*24+12,code)
        integer(self.data,self.base+raw(x,y)*24+16,slot)
        self.cells[y][x]='!'

    def finish(self):
        integer(self.data,self.base+8976,len(self.destinations))
        for i,t in enumerate(self.destinations):
            integer(self.data,self.base+8980+i*4,t)
        assert len(self.data)==self.base+34648 and self.count<=256
        return bytes(self.data)

def validate(payload, icon_texture=31):
    version, _, _, name_length=struct.unpack_from('<4i',payload)
    base=16+name_length
    assert version==6 and len(payload)==base+34648
    assert struct.unpack_from('<i',payload,8)[0]==0, 'Lab must show UFO body and shadow'
    records=[payload[base+9048+i*100:base+9048+(i+1)*100] for i in range(256)]
    kinds=[struct.unpack_from('<i',r,44)[0] for r in records]
    assert kinds.count(0)==1 and any(k in (2,11,12,13) for k in kinds)
    assert all(-1<=k<=13 for k in kinds)
    for kind,record in zip(kinds,records):
        if kind==0 or 2<=kind<=13:
            t=struct.unpack_from('<i',record,80)[0]
            assert 0<=t<374
            width,height=struct.unpack_from('<2i',record,20)
            expected_size=160 if kind==5 else 80 if 7<=kind<=10 else 120
            assert (width,height)==(expected_size,expected_size)
            x,y=(t%22-1)*40+20-width/2,(t//22-1)*40+20-height/2
            assert struct.unpack_from('<4f',record)==(x,y,x,y), 'Actor origin does not match its cell and dimensions'
            if kind==5:
                assert struct.unpack_from('<i',record,92)[0]==40, 'Farmer depth adjustment must remain original'
    for t in range(374):
        terrain, _, actor, code, item, _=struct.unpack_from('<6i',payload,base+t*24)
        if terrain in (2,3,4,5,6,7,8,9):
            assert 0<=actor<256 and struct.unpack_from('<i',records[actor],80)[0]==t
        if code:
            assert 128<=code<=136 and 0<=item<256
            assert struct.unpack_from('<i',records[item],80)[0]==t
            if code<=131:
                record=records[item]
                assert kinds[item]=={128:10,129:9,130:7,131:8}[code]
                assert terrain==0 and struct.unpack_from('<i',record,16)[0]==0
                assert struct.unpack_from('<2i',record,20)==(80,80)
                x,y=(t%22-1)*40-20,(t//22-1)*40-20
                assert struct.unpack_from('<4f',record)==(x,y,x,y), 'Fixed item is offset from its linked cell'
            if code>=132:
                assert kinds[item]==1
                assert struct.unpack_from('<i',records[item],68)[0]==icon_texture
                assert struct.unpack_from('<i',records[item],48)[0]==code-132
                assert struct.unpack_from('<2i',records[item],20)==(80,80)
        if terrain==10:
            n=struct.unpack_from('<i',payload,base+8976)[0]
            assert t in struct.unpack_from('<16i',payload,base+8980)[:n]
        if terrain in (10,11):
            # Lab marker contract: visible center must identify the trigger.
            x,y=(t%22-1)*40-20,(t//22-1)*40
            assert any(k==1 and struct.unpack_from('<i',r,68)[0]==16
                       and struct.unpack_from('<2i',r,20)==(80,40)
                       and struct.unpack_from('<4f',r)==(x,y,x,y)
                       for k,r in zip(kinds,records)), 'Teleport marker is offset from its trigger'
    n=struct.unpack_from('<i',payload,base+8976)[0]
    destinations=struct.unpack_from('<16i',payload,base+8980)[:n]
    assert 0<=n<=16 and len(set(destinations))==n
    assert all(0<=t<374 and struct.unpack_from('<i',payload,base+t*24)[0]==10 for t in destinations)

def generate():
    actors, scenery, sources = templates()
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/'levels').mkdir(exist_ok=True)
    root = ET.Element('sven2',version='1.0')
    package = ET.SubElement(root,'package',name='ParityLab')
    original = ET.parse(ASSETS/'XS/XS.xml').getroot().find('package')
    package.append(copy.deepcopy(original.find('dependencies')))
    sets = ET.SubElement(package,'levelsets')
    manifest = {'format':6,'controls':{'move':'Arrows or WASD','interact':'Space, facing the target; release and repeat','restart':'Escape, leave, select the episode again'},'template_sources':sources,'scenarios':[]}
    def save(slug,title,level,instructions,lab=False):
        payload=level.finish()
        if lab:
            payload=payload[:12]+struct.pack('<i',3)+b'Lab'+payload[level.base:]
        validate(payload)
        (OUT/'levels'/f'{slug}.lvl').write_bytes(payload)
        episode=ET.SubElement(sets,'levelset',name=title)
        ET.SubElement(episode,'level',name='01',tileset='Lab' if lab else 'Download',music='XS/sfx/atmo_grass.ogg',map=f'ParityLab/levels/{slug}.lvl')
        manifest['scenarios'].append({'id':slug,'episode':title,'instructions':instructions,'expected_path':instructions,'sha256':hashlib.sha256(payload).hexdigest(),'map':[''.join(row) for row in level.cells]})
    for kind,label in [(3,'Alien'),(4,'Dog'),(5,'Farmer'),(6,'Broemse')]:
        level=Level(actors,scenery)
        level.actor(0,7,7)
        level.actor(kind,12,7)
        level.actor(2,3,12)
        level.bush(9,7)
        save(f'corner-{kind}',f'Corner {label}',level,'Approach the central bush from the west; lure the enemy around its north and south corners. Repeat after leaving and restarting. Keep the southwest sheep unresolved.')
    for kind,label in [(3,'Alien'),(4,'Dog'),(5,'Farmer')]:
        level=Level(actors,scenery)
        level.actor(0,8,7)
        level.actor(kind,11,7)
        level.actor(2,3,12)
        save(f'attack-{kind}',f'Attack {label}',level,'Walk east into detection range, then stop. Record notice, pursuit, contact, attack animation, life delta and recovery. For Farmer also compare while repeatedly interacting with the southwest sheep. Restart for each attempt.')
    level=Level(actors,scenery)
    level.actor(0,4,7)
    for kind,x in [(2,7),(11,10),(12,13),(13,16)]:
        level.actor(kind,x,7)
    save('animals','Animals',level,'Approach each animal and repeatedly press Space facing it. Compare facing, meters, score/time changes, release and completion. Sheep temperament is randomized at load.')
    level=Level(actors,scenery)
    level.actor(0,3,7)
    level.actor(2,3,12)
    for kind,x in [(7,6),(8,9),(9,12),(10,15)]:
        level.actor(kind,x,4)
    level.teleport(6,9)
    level.teleport(15,9)
    level.teleport(10,12,True)
    save('items-teleports','Items and teleports',level,'Visit beer, teddy, bone and artifact along row 4. Sven does not collect beer/teddy/bone: they are enemy distractions, covered by Dog Bone, Farmer Beer and Girl Teddy. Walk onto the artifact for requested UFO collection; record the resulting sequence. Enter either puddle on row 9, then the water trigger at (10,12). Compare entry/exit, sound and cooldown. The puddle image at the water trigger is only a location marker.')
    level=Level(actors,scenery)
    level.actor(0,9,7)
    level.actor(2,3,12)
    for x,y in [(7,6),(9,6),(11,6),(7,8),(9,8),(11,8)]:
        integer(level.data,level.base+raw(x,y)*24+20,141)
        level.cells[y][x]='?'
    save('gift-life','Gift and life',level,'Wait near the six player-effect spawners. Collect gifts when available; record score/lives before and after, including randomized gift outcomes. Other effects also spawn. Keep the sheep unresolved. This does not force a gift or score-threshold life award.')
    level=Level(actors,scenery)
    level.actor(0,8,7)
    level.actor(2,8,6)
    real(level.data,level.base+9044,800)
    save('lives-score','Lives',level,'Face north and repeatedly press Space to resolve the single nearby sheep. Finish promptly: the 800-second timer bonus should carry the campaign score past 750. Observe any threshold life/HUD award, then continue to the alien arena.')
    level=Level(actors,scenery)
    level.actor(0,8,7)
    level.actor(3,11,7)
    level.actor(2,3,12)
    save('lives-arena','Lives arena',level,'Entered through Lives with carried nonzero score. Approach the alien and compare actual life loss and HUD animation. Wait more than five seconds between attacks. This tests campaign score even though this level score starts at zero.')
    # Join these two authored maps into one campaign, preserving every other
    # scenario as an independently selectable single-level episode.
    episodes=list(sets)
    lives=episodes[-2]
    arena=episodes[-1]
    second=copy.deepcopy(arena.find('level'))
    second.set('name','02')
    lives.append(second)
    sets.remove(arena)
    # New single-level episodes follow the unchanged original twelve maps.
    for slug,title,enemy,item in [
        ('dog-bone','Dog Bone',4,9),('farmer-beer','Farmer Beer',5,7),
        ('girl-teddy','Girl Teddy',6,8)]:
        level=Level(actors,scenery)
        level.actor(0,4,9); level.actor(2,3,12)
        level.actor(enemy,12,7); level.actor(item,10,7)
        save(slug,title,level,'Stay southwest and leave the item at (10,7) for the enemy at (12,7). Observe approach, item disappearance, consumption and recovery. Dog: digging then idle recovery. Farmer: drinking, lying down, sleeping and waking. Girl: teddy playing and return. If idle, approach to provoke movement, then retreat; do not collect the item. Restart for an independent attempt.')
    level=Level(actors,scenery)
    level.actor(0,4,9); level.actor(2,3,12)
    level.actor(6,10,7); level.actor(4,12,7)
    save('girl-dog','Girl Dog',level,'Approach from southwest to activate Girl/Broemse, then retreat. Lure her alongside the dog; observe her dog interaction, dog stun/rest, and both recoveries. Relative facing and AI movement matter; repeat from the menu if they separate. Keep the sheep unresolved.')
    level=Level(actors,scenery)
    level.actor(0,8,7); level.actor(6,11,7); level.actor(2,3,12)
    save('girl-sven','Girl Sven attack',level,'Walk east toward Girl/Broemse, stop in range, and let her reach Sven. Also compare while interacting with the southwest sheep. Observe pursuit, battle/contact animation, logical life count, HUD icons and recovery; restart each independent attempt.')
    level=Level(actors,scenery)
    level.actor(0,7,7); level.actor(10,9,7); level.actor(2,13,7)
    save('artifact-sven','Artifact Sven',level,'Walk east onto the artifact at (9,7). Observe pickup, transformation/effect, movement, then face the sheep at (13,7) and press Space repeatedly. Record interaction, score and return to ordinary Sven; restart before resolving the only sheep if more observation is needed.')
    for kind,label in [(3,'Alien'),(4,'Dog'),(5,'Farmer'),(6,'Girl')]:
        level=Level(actors,scenery)
        level.actor(0,4,9); level.actor(2,3,12)
        level.actor(kind,12,7); level.actor(10,10,7)
        save(f'artifact-enemy-{kind}',f'Artifact {label}',level,'Leave the artifact at (10,7) for the enemy at (12,7); approach to activate then retreat southwest. Compare whether this enemy seeks/enters the artifact, consumes it and triggers UFO removal. The requested UFO, its shadow and beam should all be visible. This is a comparison path, not a guarantee that every enemy searches for artifacts; record non-consumption too.')
    for code,label in [(132,'Firework'),(133,'Gift'),(134,'Impotent'),(135,'Stink'),(136,'Rose')]:
        level=Level(actors,scenery)
        level.actor(0,7,7); level.effect(code,9,7)
        level.actor(2,13,7); level.actor(11,13,10)
        save(f'pickup-{code}',f'Pickup {label}',level,f'Walk east onto the visible {label} icon at (9,7), grid code {code}. Observe icon removal, sound and effect. Compare movement, then face the sheep at (13,7) and chicken at (13,10), pressing and releasing Space repeatedly. Observe meters, score/time and effect recovery. Gift outcomes remain random: record the outcome and restart to sample again. The icon is linked scenery, not a native spawned item animation.',lab=True)
    level=Level(actors,scenery)
    level.actor(0,8,7); level.actor(2,8,6)
    save('frigida','Frigida Sole Sheep',level,'Exactly one sheep at (8,6) forces the runtime random special-sheep choice to this sheep. Face north, press and release Space repeatedly; observe kettle/Frigida interaction stages, sound, progress and completion. Move away between attempts to compare interruption and resume; restart for a fresh sequence.')
    level=Level(actors,scenery)
    level.actor(0,8,7); level.actor(2,8,6); level.actor(2,3,12)
    for kind,x in [(11,11),(12,14),(13,17)]: level.actor(kind,x,7)
    for slot in (1,2): integer(level.data,level.base+9048+slot*100+84,10)
    save('angry-animals','Angry rear kick and animals',level,'Two sheep start with anger meter 10; runtime selects one as Frigida, leaving the other eligible for the angry kick. Try the north sheep, then the southwest sheep if necessary. Approach from its rear, face it and press Space; if it whistles/turns, reposition and repeat. Compare rear kick, timer penalty, recovery and repeat behavior. Then interact with chicken (11,7), goat (14,7), pig (17,7); compare facing, meters, sound and completion. Restart if anger decays before contact.')
    level=Level(actors,scenery)
    level.actor(0,3,7); level.actor(4,13,7); level.actor(2,3,12)
    dog=level.base+9048+100
    integer(level.data,dog+72,10) # original sleeping state
    real(level.data,dog+88,15) # authored initial sleep pulse, not a wake guarantee
    save('dog-sleep','Dog Sleep',level,'Sven starts far west at (3,7), sleeping dog at (13,7), unresolved sheep at (3,12). First stay away and observe sleep/Zzz; restart, then approach east to compare proximity waking, standing recovery and pursuit. The fixture starts the dog in original state 10 with pulse 15; record actual wake conditions and elapsed time rather than assuming an exact sleep duration. Keep the sheep unresolved.')
    level=Level(actors,scenery)
    level.actor(0,2,7); level.actor(2,13,5); level.actor(2,13,9)
    for slot in (1,2): integer(level.data,level.base+9048+slot*100+84,10)
    # Exercise the ordinary anger trigger, not a serialized demon state.
    for slot in (1,2):
        start=level.base+9048+slot*100
        assert struct.unpack_from('<i',level.data,start+72)[0]==0
        assert level.data[start+65]==0
        assert struct.unpack_from('<i',level.data,start+84)[0]==10
    assert struct.unpack_from('<i',level.data,8)[0]==0
    save('demon-sheep','Demon Sheep',level,'Open arena: Sven starts distant at (2,7), two ordinary sheep at (13,5)/(13,9), both initially at max anger 10. Stay away without interacting and watch long enough: impatience rises toward 12 after 41 pulses of 26/60 seconds, then a 1-in-5 chance triggers transformation. Repeated attempts can take more than a minute; record elapsed time and repeat if needed. Approach during transformation to observe acquisition, pursuit, contact/attack, recovery and collision in open cells. Do not resolve both sheep. One sheep randomly receives Frigida flag 1 at load; only interaction converts that flag to 2, which prevents the demon trigger. Avoid early interaction so the normal trigger remains available. No demon state is prebaked; UFO graphics remain visible and timeout collection can still occur.')
    tilesets=copy.deepcopy(original.find('tilesets'))
    lab=copy.deepcopy(tilesets.find('tileset')); lab.set('name','Lab')
    ET.SubElement(lab,'tile',file='Sven2/gfx/icons.bmp',frames='5')
    tilesets.append(lab); package.append(tilesets)
    ET.indent(root)
    ET.ElementTree(root).write(OUT/'ParityLab.xml',encoding='utf-8',xml_declaration=True)
    (OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    print(f'Generated {len(manifest["scenarios"])} v6 levels in {OUT}')

if __name__=='__main__':
    argparse.ArgumentParser(description=__doc__).parse_args()
    generate()
