# SvenBomwollen-Reversal

Reversing an old game from my childhood for fun

![](thumbnail.jpg?raw=true "Title")

#  Content

## assets
Decoder output for all Sven Zwø .pak and .dat files. Includes:
- Character spritesheets
- Items
- Menus, GUI elements and icons
- Sound effects
- Localized strings in xml format
- Game parameters like character movement speed, item usage durations, animation framerates and more
- Level elements and backgrounds
- Level layout files (not decoded yet)

## decoder
Simple C# application that can decode the .pak and .dat format that Sven Zwø uses. Not tested on XXX and 004 yet, but because the engine is effectively identical, it probably works for the other games too.\
``Usage: decoder <file.dat|file.pak> <output-folder>``

## dumper
DLL file for dumping all loaded textures while the game is running. Just inject it with your favourite injector and play some levels! Includes 3 pre-compiled versions for Sven Zwø, XXX and 004.

## leveleditor
Leveleditor that I created based on this research. There's still a lot of stuff to do though.

## levellogger
Old project which simply dumps all texture data from a running level. Just inject it with your favourite injector and play some levels! Includes 3 pre-compiled versions for Sven Zwø, XXX and 004. You can also see example logs in the logfiles folder.

## levelpacks
All available level packages for the game. They get loaded automatically when dropped into the game directory and have some sort of version validation (Sven XXX can't load a Sven Zwø pack, etc.).

## logfiles
String dumps of the game exe and engine dlls that can be useful and logged level texture data.

## patcher
DLL file for patching packfile signature checks. This has to be loaded very early, so injecting the dll won't work. Instead, use CFF Explorer to add an import to the exported Dummy function.

## screenshots
Screenshots that were used for testing, as a note or inside this readme.

## sven2
The actual game we reverse (Sven Zwø XS).

## tests
Stores patched pack files, notes and C# scripts that I used for testing the .lvl file format. I tested it with trial-and-error by modifying the first tutorial level:
- XS_tutorial_4sheep: Adds a 4th sheep\
![](screenshots/extra_sheep.png?raw=true "Title")
- XS_tutorial_field01v01: Copies level data from field01v01\
![](screenshots/meltdown.png?raw=true "Title")
- XS_tutorial_10sheep: Adds 10 sheep\
![](screenshots/more_sheep.png?raw=true "Title")
- XS_tutorial_new_bush: Adds a new bush object with a somewhat accurate collision box\
![](screenshots/new_bush.png?raw=true "Title")
- XS_tutorial_no_collision: Removes all colliders from the level\
![](screenshots/no_collision.png?raw=true "Title")
- XS_tutorial_blank: Removes all objects from the level\
![](screenshots/void.png?raw=true "Title")
- XS_tutorial_weirdness: Adds a second sven to the level and the original sven no longer plays animations\
![](screenshots/what.png?raw=true "Title")


# Research and notes


## Engine
The game ships with multiple dlls from the mudGE / WitanEngine runtime. The descriptions below are based on reverse-engineering, exports and embedded strings.
### mudGE.dll
The main engine/runtime library of the game. It provides core engine functions like initialization, update handling, bitmap objects, serialization, basic math/utility classes and device abstractions used by the game and its plugins.
### pluginpack.dll
Plugin library for loading and saving media formats used by the engine. Based on embedded symbols and strings, it contains image/resource handling code and is basically an extension for mudGE's asset system. It's probably responsible to load the levels and textures.
### wtnlib.dll
Support library which contains lower-level utility and framework code used by the other 2 dlls and the game. It includes file helpers, UI classes, string utilities and a general runtime.


## Important functions
Here are a few important/interesting functions.
### LoadSprite
```bool LoadSprite(CApplication* app, void* filePath, void* sprite, int width, int height, void* spriteMirrored) //mudGe.dll+0x1D440``` \
This function is responsible for loading any sprites into the game, including characters, level elements, GUI controls but strangely not the level backgrounds. The sprite dumper hooks this to signal that we just loaded a sprite and to get its original name.
- app: Owner object of the function
- filePath: Encoded ("virtual") filepath of the sprite
- sprite: Sprite object output
- width: Width of the sprite
- height: Height of the sprite
- spriteMirrored: Optional parameter, if specified outputs a mirrored version of the sprite object
- returns: True on success, otherwise false

### GetFile
```CString GetFile(void* filePath) //wtnlib.dll+0xA5A0``` \
This function is responsible for turning a filepath object (for example from LoadSprite parameter 2) into a human-readable string. The sprite dumper uses this one to get the original sprite names.
- filePath: Encoded ("virtual") filepath
- returns: String struct

### DecodeBmpFile
```void DecodeBmpFile(void* codec, iIOStream* stream, void* texture) //pluginpack.dll+0x14360``` \
This function gets called somewhere later by LoadSprite to decode a bmp file into an ingame texture. It uses a custom IOStream class for decoding the file. The sprite dumper hooks this to actually dump the texture as a bmp file after the LoadSprite hook got called.
- codec: Owner object of the function
- stream: Custom bytestream class used for decoding
- texture: Decoded texture output
- returns: Nothing


## Important structures
Here are a few important/interesting structures.
### CString
```cpp
struct CString
{
    char pad[4]; //0x00
    char* data; //0x04
    int size; //0x08
};
```
Simple string structure that's used almost everywhere in the game. Contains a pointer to the string and its size.
### BmpFileHeader
```cpp
struct BmpFileHeader
{
    union {
        struct {
            union {
                struct {
                    uint8_t header_0x42;
                    uint8_t header_0x4D;
                };
                uint16_t header;
            };
            uint32_t file_size;
            uint16_t reserved_1;
            uint16_t reserved_2;
            uint32_t pixel_array_offset;
        };
        char buffer[14];
    };
};
```
BMP file format header that I use for decoding the texture bitmaps (https://en.wikipedia.org/wiki/BMP_file_format).
### iIOStream
```cpp
class iIOStream
{
public:
    virtual ~iIOStream() { } //0x00
    virtual void func1() { } //0x04
    virtual void func2() { } //0x08
    virtual void func3() { } //0x0C
    virtual void func4() { } //0x10
    //returns this+0xC
    virtual int32_t get_location() { } //0x14
    virtual void func6() { } //0x18
    //sets this+0xC and does some exception handling we don't care about
    virtual void seek(int32_t value, int32_t alwaysZero) { } //0x1C
    virtual void func8() { } //0x20
    //copies this+0x10 with memcpy (doing that manually does nothing)
    virtual void read(char* buffer, int32_t size) { } //0x24
public:
    char pad_0004[8]; //0x04
    DWORD location; //0x0C setting this does absolute jack shit for some reason
    DWORD* some_array; //0x10 according to ida, this is correct but does nothing when reading it
public:
    //custom read function that doesn't modify the read location
    void custom_read(char* buffer, int32_t size) {
        int32_t pos = this->get_location();
        this->read(buffer, size);
        this->seek(pos, 0);
    }
};
```
Bytestream helper that's used for file i/o operations. I use this to read the texture data from the DecodeBmpFile hook. For some strange reason the attributes of that class didn't behave as expected so I had to use the virtual functions to read any data.


## Pack File Format
Here's a quick summary of the file format that Sven Zwø uses for basically everything. It is basically a simple archive format with minimal xor encryption. It starts with a simple header layout:
```cpp
struct ArchiveHeader
{
    char header[8]; //MUDGE4.0
    char pad_0008[48]; //Unknown data, doesn't interest me rn
    unsigned data_end; //End offset of header data
};
```
After we add a padding of 17 bytes to data_end, we get to the root of the archive. For directories and files inside the archive, a simple node structure is used which starts with a special node header:
```cpp
struct NodeHeader
{
    byte node_type; //1 for directory, 2 for file
    unsigned name_hash; //Hash of the node name
    unsigned name_length; //Length of the node name
    char name[name_length]; //Node name/path
    union {
        DirectoryNode directory_node; //Used for node_type 1
        FileNode file_node; //Used for node_type 2
    };
};
```
Depending on the node type, there is extra data after the header:
```cpp
struct DirectoryNode
{
    unsigned child_count; //Number of children (Files in the directory)
    NodeHeader children[child_count]; //Child nodes
};

struct FileNode
{
    unsigned flags; //Unknown flags
    unsigned offset; //Offset to the file's content
    unsigned size; //Size of the file
    unsigned unknown; //Unknown data, possible padding
};
```
It's also really interesting that a simple encryption was used for the data. There are 3 different xor keys:
- 0xFFAA5533: Used for decrypting the file node offset
- 0x3355AAFF: Used for decrypting the file node size
- 0x88: Used for decrypting every byte of the file node content


## Signature checks
The next logical step was to modify existing levels. Sadly this wasn't so easy, because turns out the pack files have an embedded signature that needs to be valid, otherwise the game will crash when trying to load it.
Turns out, the game has a huge function at 0x425120 that verifies all kinds of stuff when loading a pack file. An interesting function was CryptVerifySignatureA which we can actually just hook and make always return true. However, this only works when loading the dll really early, because this is one of the first things that the game does. The easiest way to apply the patch is to use tools like CFF Explorer to add an import that loads my patcher.dll file.


## Level file format
Reversing and understanding the level file format was by far the hardest part. It took me multiple days to figure everything out... The .lvl files inside the pack files are not compressed or encrypted like the packfiles themselves. They are binary files with a small header, a tile table with a fixed size and an object table. The structure looks kinda like this:
```cpp
struct LvlFile
{
    int version; //Normally 6, probably different for other levelpacks or Sven games
    int unknown0; //Not sure, might be padding as the value is always 0
    int unknown1; //Same here
    int tileset_length; //Length of the tileset name
    char tileset[tileset_length]; //Tileset name, for example download
    TileEntry tiles[25 * 15]; //Tile table, maximum 25 horizontally and 15 vertically (at least in Sven Zwø and XXX, 004 is probably different)
    int reserved[11]; //Always -1
    ObjectEntry objects[256]; //Object table
    int unknown2; //Same as unknown0 and 1
};

struct TileEntry
{
    int tile_type; //Defines if the tile is a collider, enemy, sheep, etc.
    int field1; //Not sure about the other stuff, probably metadata like sheep angryness, teleport locations, etc.
    int field2;
    int field3;
    int field4;
    int field5;
};

struct ObjectEntry
{
    Flint fields[25]; //Type-specific
    /*
    field1 = x position (float)
    field2 = y position (float)
    field3 = x position (same as field1)
    field4 = y position (same as field2)
    field6 = width (int)
    field7 = height (int)
    field12 = object type or activation state (1 = active, -1 = inactive)
    field18 = texture index in tileset
    field21 = tile link (usually used by sheep objects)
    */
};

union Flint
{
    int value_i;
    float value_f;
};
```
Figuring out the general layout was still ok and so was figuring out how the objects are defined since I was able to use the level logger dll to log the coordinates for every object. Each object has 25 int or float fields which store different data depending on the object type. See the ObjectEntry struct for more details. The player object is always the first one and has an object type of zero and because of that it's even possible to clone Sven when zeroing out an object. This happened by accident when I tried to delete certain objects. The draw order of objects is the same as the order of the objects inside the array, so later objects are drawn later. There exists a texture index that defines the texture type (for example 16 = BG_DL_puddle01.bmp or 17 = BG_DL_puddle02.bmp). The visual sprite and interaction logic / collision is separated, objects are purely visual while tiles handle the game logic. There are even special tiles that store extra information like puddle teleport locations. Sheep are a special case, they are linked to a tile with field21 which stores the index of the tile in the tile array. Example:
```
objects[22] = sheep
objects[22].field21 = 118
-> tiles[118] = sheep collision / interaction trigger (type 2)
```
The tiles were a completely different story, the coordinate system was really hard to understand, so it took a bit of luck and testing. Eventually I figured it out though. What interests us the most is a) the tile index and b) the tile type. That type is stored in field0 and through placing tiles of a certain type in the level I found out which type is what:
```
T0 = Empty
T1 = Collider, static
T2 = Sheep Interaction Trigger + Collider, disappears after the sheep is gone and moves with it
T3 = Teleport controller, stores a list of coordinates where teleporter puddles are
T4 = Dog / Wøtan Spawnpoint
T5 = Farmer / Lars Spawnpoint
T6 = Girl / Brømse Spawnpoint
T7 = Unknown, possibly Alien Spawnpoint
T8 = Unknown, possibly Item Spawnpoint
T9 = Unknown
T10 = Puddle Teleport Trigger, walkthough
T11 = Pond Teleport Trigger, blocking
```
Originally I thought that the tile table is a normal 25x15 grid which would make sense when looking at the array size. However, the gameplay area is actually only 20x15 tiles which kinda tripped me up because the whole tile mapping was obviously messed up:
![](screenshots/editor2.png?raw=true "Title")
![](screenshots/oglevel.png?raw=true "Title")
I also thought that the game uses an X and Y coordinate to define tiles but that's actually not the case at all. After a lot of tests...
![](screenshots/ntests.png?raw=true "Title")
...I found out that the game treats the tiles as a 22 wide stream although 2 elements are always unused! The only way to truely test this was to start with a level without collision, always place one collision tile at each possible location, creating a pack file for it and finally testing it ingame. I made a few useful scripts that made testing this less of a hassle, take a look at the tests folder if you're interested. By running all tests, I created a "canonical coordinate mapping" with the format [testId] (X=[editorX];Y=[editorY]): [gameX],[gameY] or n for nothing / offscreen:
```
000 (X=0;Y=0): n
001 (X=1;Y=0): n
002 (X=2;Y=0): n
003 (X=3;Y=0): n
004 (X=4;Y=0): n
005 (X=5;Y=0): n
006 (X=6;Y=0): n
007 (X=7;Y=0): n
008 (X=8;Y=0): n
009 (X=9;Y=0): n
010 (X=10;Y=0): n
011 (X=11;Y=0): n
012 (X=12;Y=0): n
013 (X=13;Y=0): n
014 (X=14;Y=0): n
015 (X=15;Y=0): n
016 (X=16;Y=0): n
017 (X=17;Y=0): n
018 (X=18;Y=0): n
019 (X=19;Y=0): n
020 (X=20;Y=0): n
021 (X=21;Y=0): n
022 (X=22;Y=0): n
023 (X=23;Y=0): 0,0
024 (X=24;Y=0): 1,0
025 (X=0;Y=1): 2,0
026 (X=1;Y=1): 3,0
027 (X=2;Y=1): 4,0
028 (X=3;Y=1): 5,0
029 (X=4;Y=1): 6,0
030 (X=5;Y=1): 7,0
031 (X=6;Y=1): 8,0
032 (X=7;Y=1): 9,0
033 (X=8;Y=1): 10,0
034 (X=9;Y=1): 11,0
035 (X=10;Y=1): 12,0
036 (X=11;Y=1): 13,0
037 (X=12;Y=1): 14,0
038 (X=13;Y=1): 15,0
039 (X=14;Y=1): 16,0
040 (X=15;Y=1): 17,0
041 (X=16;Y=1): 18,0
042 (X=17;Y=1): 19,0
043 (X=18;Y=1): n
044 (X=19;Y=1): n
045 (X=20;Y=1): 0,1
046 (X=21;Y=1): 1,1
047 (X=22;Y=1): 2,1
048 (X=23;Y=1): 3,1
049 (X=24;Y=1): 4,1
050 (X=0;Y=2): 5,1
051 (X=1;Y=2): 6,1
052 (X=2;Y=2): 7,1
053 (X=3;Y=2): 8,1
054 (X=4;Y=2): 9,1
055 (X=5;Y=2): 10,1
056 (X=6;Y=2): 11,1
057 (X=7;Y=2): 12,1
058 (X=8;Y=2): 13,1
059 (X=9;Y=2): 14,1
060 (X=10;Y=2): 15,1
061 (X=11;Y=2): 16,1
062 (X=12;Y=2): 17,1
063 (X=13;Y=2): 18,1
064 (X=14;Y=2): 19,1
065 (X=15;Y=2): n
066 (X=16;Y=2): n
067 (X=17;Y=2): 0,2
068 (X=18;Y=2): 1,2
069 (X=19;Y=2): 2,2
070 (X=20;Y=2): 3,2
071 (X=21;Y=2): 4,2
072 (X=22;Y=2): 5,2
073 (X=23;Y=2): 6,2
074 (X=24;Y=2): 7,2
075 (X=0;Y=3): 8,2
076 (X=1;Y=3): 9,2
077 (X=2;Y=3): 10,2
078 (X=3;Y=3): 11,2
079 (X=4;Y=3): 12,2
080 (X=5;Y=3): 13,2
081 (X=6;Y=3): 14,2
082 (X=7;Y=3): 15,2
083 (X=8;Y=3): 16,2
084 (X=9;Y=3): 17,2
085 (X=10;Y=3): 18,2
086 (X=11;Y=3): 19,2
087 (X=12;Y=3): n
088 (X=13;Y=3): n
089 (X=14;Y=3): 0,3
090 (X=15;Y=3): 1,3
091 (X=16;Y=3): 2,3
092 (X=17;Y=3): 3,3
093 (X=18;Y=3): 4,3
094 (X=19;Y=3): 5,3
095 (X=20;Y=3): 6,3
096 (X=21;Y=3): 7,3
097 (X=22;Y=3): 8,3
098 (X=23;Y=3): 9,3
099 (X=24;Y=3): 10,3
100 (X=0;Y=4): 11,3
101 (X=1;Y=4): 12,3
102 (X=2;Y=4): 13,3
103 (X=3;Y=4): 14,3
104 (X=4;Y=4): 15,3
105 (X=5;Y=4): 16,3
106 (X=6;Y=4): 17,3
107 (X=7;Y=4): 18,3
108 (X=8;Y=4): 19,3
109 (X=9;Y=4): n
110 (X=10;Y=4): n
111 (X=11;Y=4): 0,4
112 (X=12;Y=4): 1,4
113 (X=13;Y=4): 2,4
114 (X=14;Y=4): 3,4
115 (X=15;Y=4): 4,4
116 (X=16;Y=4): 5,4
117 (X=17;Y=4): 6,4
118 (X=18;Y=4): 7,4
119 (X=19;Y=4): 8,4
120 (X=20;Y=4): 9,4
121 (X=21;Y=4): 10,4
122 (X=22;Y=4): 11,4
123 (X=23;Y=4): 12,4
124 (X=24;Y=4): 13,4
125 (X=0;Y=5): 14,4
126 (X=1;Y=5): 15,4
127 (X=2;Y=5): 16,4
128 (X=3;Y=5): 17,4
129 (X=4;Y=5): 18,4
130 (X=5;Y=5): 19,4
131 (X=6;Y=5): n
132 (X=7;Y=5): n
133 (X=8;Y=5): 0,5
134 (X=9;Y=5): 1,5
135 (X=10;Y=5): 2,5
136 (X=11;Y=5): 3,5
137 (X=12;Y=5): 4,5
138 (X=13;Y=5): 5,5
139 (X=14;Y=5): 6,5
140 (X=15;Y=5): 7,5
141 (X=16;Y=5): 8,5
142 (X=17;Y=5): 9,5
143 (X=18;Y=5): 10,5
144 (X=19;Y=5): 11,5
145 (X=20;Y=5): 12,5
146 (X=21;Y=5): 13,5
147 (X=22;Y=5): 14,5
148 (X=23;Y=5): 15,5
149 (X=24;Y=5): 16,5
150 (X=0;Y=6): 17,5
151 (X=1;Y=6): 18,5
152 (X=2;Y=6): 19,5
153 (X=3;Y=6): n
154 (X=4;Y=6): n
155 (X=5;Y=6): 0,6
156 (X=6;Y=6): 1,6
157 (X=7;Y=6): 2,6
158 (X=8;Y=6): 3,6
159 (X=9;Y=6): 4,6
160 (X=10;Y=6): 5,6
161 (X=11;Y=6): 6,6
162 (X=12;Y=6): 7,6
163 (X=13;Y=6): 8,6
164 (X=14;Y=6): 9,6
165 (X=15;Y=6): 10,6
166 (X=16;Y=6): 11,6
167 (X=17;Y=6): 12,6
168 (X=18;Y=6): 13,6
169 (X=19;Y=6): 14,6
170 (X=20;Y=6): 15,6
171 (X=21;Y=6): 16,6
172 (X=22;Y=6): 17,6
173 (X=23;Y=6): 18,6
174 (X=24;Y=6): 19,6
175 (X=0;Y=7): n
176 (X=1;Y=7): n
177 (X=2;Y=7): 0,7
178 (X=3;Y=7): 1,7
179 (X=4;Y=7): 2,7
180 (X=5;Y=7): 3,7
181 (X=6;Y=7): 4,7
182 (X=7;Y=7): 5,7
183 (X=8;Y=7): 6,7
184 (X=9;Y=7): 7,7
185 (X=10;Y=7): 8,7
186 (X=11;Y=7): 9,7
187 (X=12;Y=7): 10,7
188 (X=13;Y=7): 11,7
189 (X=14;Y=7): 12,7
190 (X=15;Y=7): 13,7
191 (X=16;Y=7): 14,7
192 (X=17;Y=7): 15,7
193 (X=18;Y=7): 16,7
194 (X=19;Y=7): 17,7
195 (X=20;Y=7): 18,7
196 (X=21;Y=7): 19,7
197 (X=22;Y=7): n
198 (X=23;Y=7): n
199 (X=24;Y=7): 0,8
200 (X=0;Y=8): 1,8
201 (X=1;Y=8): 2,8
202 (X=2;Y=8): 3,8
203 (X=3;Y=8): 4,8
204 (X=4;Y=8): 5,8
205 (X=5;Y=8): 6,8
206 (X=6;Y=8): 7,8
207 (X=7;Y=8): 8,8
208 (X=8;Y=8): 9,8
209 (X=9;Y=8): 10,8
210 (X=10;Y=8): 11,8
211 (X=11;Y=8): 12,8
212 (X=12;Y=8): 13,8
213 (X=13;Y=8): 14,8
214 (X=14;Y=8): 15,8
215 (X=15;Y=8): 16,8
216 (X=16;Y=8): 17,8
217 (X=17;Y=8): 18,8
218 (X=18;Y=8): 19,8
219 (X=19;Y=8): n
220 (X=20;Y=8): n
221 (X=21;Y=8): 0,9
222 (X=22;Y=8): 1,9
223 (X=23;Y=8): 2,9
224 (X=24;Y=8): 3,9
225 (X=0;Y=9): 4,9
226 (X=1;Y=9): 5,9
227 (X=2;Y=9): 6,9
228 (X=3;Y=9): 7,9
229 (X=4;Y=9): 8,9
230 (X=5;Y=9): 9,9
231 (X=6;Y=9): 10,9
232 (X=7;Y=9): 11,9
233 (X=8;Y=9): 12,9
234 (X=9;Y=9): 13,9
235 (X=10;Y=9): 14,9
236 (X=11;Y=9): 15,9
237 (X=12;Y=9): 16,9
238 (X=13;Y=9): 17,9
239 (X=14;Y=9): 18,9
240 (X=15;Y=9): 19,9
241 (X=16;Y=9): n
242 (X=17;Y=9): n
243 (X=18;Y=9): 0,10
244 (X=19;Y=9): 1,10
245 (X=20;Y=9): 2,10
246 (X=21;Y=9): 3,10
247 (X=22;Y=9): 4,10
248 (X=23;Y=9): 5,10
249 (X=24;Y=9): 6,10
250 (X=0;Y=10): 7,10
251 (X=1;Y=10): 8,10
252 (X=2;Y=10): 9,10
253 (X=3;Y=10): 10,10
254 (X=4;Y=10): 11,10
255 (X=5;Y=10): 12,10
256 (X=6;Y=10): 13,10
257 (X=7;Y=10): 14,10
258 (X=8;Y=10): 15,10
259 (X=9;Y=10): 16,10
260 (X=10;Y=10): 17,10
261 (X=11;Y=10): 18,10
262 (X=12;Y=10): 19,10
263 (X=13;Y=10): n
264 (X=14;Y=10): n
265 (X=15;Y=10): 0,11
266 (X=16;Y=10): 1,11
267 (X=17;Y=10): 2,11
268 (X=18;Y=10): 3,11
269 (X=19;Y=10): 4,11
270 (X=20;Y=10): 5,11
271 (X=21;Y=10): 6,11
272 (X=22;Y=10): 7,11
273 (X=23;Y=10): 8,11
274 (X=24;Y=10): 9,11
275 (X=0;Y=11): 10,11
276 (X=1;Y=11): 11,11
277 (X=2;Y=11): 12,11
278 (X=3;Y=11): 13,11
279 (X=4;Y=11): 14,11
280 (X=5;Y=11): 15,11
281 (X=6;Y=11): 16,11
282 (X=7;Y=11): 17,11
283 (X=8;Y=11): 18,11
284 (X=9;Y=11): 19,11
285 (X=10;Y=11): n
286 (X=11;Y=11): n
287 (X=12;Y=11): 0,12
288 (X=13;Y=11): 1,12
289 (X=14;Y=11): 2,12
290 (X=15;Y=11): 3,12
291 (X=16;Y=11): 4,12
292 (X=17;Y=11): 5,12
293 (X=18;Y=11): 6,12
294 (X=19;Y=11): 7,12
295 (X=20;Y=11): 8,12
296 (X=21;Y=11): 9,12
297 (X=22;Y=11): 10,12
298 (X=23;Y=11): 11,12
299 (X=24;Y=11): 12,12
300 (X=0;Y=12): 13,12
301 (X=1;Y=12): 14,12
302 (X=2;Y=12): 15,12
303 (X=3;Y=12): 16,12
304 (X=4;Y=12): 17,12
305 (X=5;Y=12): 18,12
306 (X=6;Y=12): 19,12
307 (X=7;Y=12): n
308 (X=8;Y=12): n
309 (X=9;Y=12): 0,13
310 (X=10;Y=12): 1,13
311 (X=11;Y=12): 2,13
312 (X=12;Y=12): 3,13
313 (X=13;Y=12): 4,13
314 (X=14;Y=12): 5,13
315 (X=15;Y=12): 6,13
316 (X=16;Y=12): 7,13
317 (X=17;Y=12): 8,13
318 (X=18;Y=12): 9,13
319 (X=19;Y=12): 10,13
320 (X=20;Y=12): 11,13
321 (X=21;Y=12): 12,13
322 (X=22;Y=12): 13,13
323 (X=23;Y=12): 14,13
324 (X=24;Y=12): 15,13
325 (X=0;Y=13): 16,13
326 (X=1;Y=13): 17,13
327 (X=2;Y=13): 18,13
328 (X=3;Y=13): 19,13
329 (X=4;Y=13): n
330 (X=5;Y=13): n
331 (X=6;Y=13): 0,14
332 (X=7;Y=13): 1,14
333 (X=8;Y=13): 2,14
334 (X=9;Y=13): 3,14
335 (X=10;Y=13): 4,14
336 (X=11;Y=13): 5,14
337 (X=12;Y=13): 6,14
338 (X=13;Y=13): 7,14
339 (X=14;Y=13): 8,14
340 (X=15;Y=13): 9,14
341 (X=16;Y=13): 10,14
342 (X=17;Y=13): 11,14
343 (X=18;Y=13): 12,14
344 (X=19;Y=13): 13,14
345 (X=20;Y=13): 14,14
346 (X=21;Y=13): 15,14
347 (X=22;Y=13): 16,14
348 (X=23;Y=13): 17,14
349 (X=24;Y=13): 18,14
350 (X=0;Y=14): 19,14
351 (X=1;Y=14): n
352 (X=2;Y=14): n
353 (X=3;Y=14): n
354 (X=4;Y=14): n
355 (X=5;Y=14): n
356 (X=6;Y=14): n
357 (X=7;Y=14): n
358 (X=8;Y=14): n
359 (X=9;Y=14): n
360 (X=10;Y=14): n
361 (X=11;Y=14): n
362 (X=12;Y=14): n
363 (X=13;Y=14): n
364 (X=14;Y=14): n
365 (X=15;Y=14): n
366 (X=16;Y=14): n
367 (X=17;Y=14): n
368 (X=18;Y=14): n
369 (X=19;Y=14): n
370 (X=20;Y=14): n
371 (X=21;Y=14): n
372 (X=22;Y=14): n
373 (X=23;Y=14): n
374 (X=24;Y=14): n
```
Do you see the pattern? Starting from X=23, there are always 20 points in a single Y row, followed by 2 offscreen points. They are wrapped by 2 big chunks of offscreen points. And if we add up the offscreen points, we get exactly 75 which is the 5*15 tiles that are unused by the game! It's all coming together now!
After applying these rules, the tile layout in the editor started to make sense:
![](screenshots/oglevel.png?raw=true "Title")
![](screenshots/editor_final_1.png?raw=true "Title")
Furthermore, I added a feature that displays the textures for convenience:
![](screenshots/editor_final_2.png?raw=true "Title")
I already have a simple tool that can replace a .lvl file in a pack file but the next step is to create a tool that can add new levelpacks to a pack file.
