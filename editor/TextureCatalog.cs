using System.IO;
using System.Xml.Linq;

namespace Editor
{
    internal static class TextureCatalog
    {
        public sealed class Texture
        {
            public string File { get; set; }
            public string Name => Path.GetFileName(File);
            public int Width { get; set; }
            public int Height { get; set; }

            public Texture(string file, int width, int height)
            {
                File = Normalize(file);
                Width = width;
                Height = height;
            }
        }

        public sealed class Tileset
        {
            public string Name { get; set; }
            public string Background { get; set; }
            public Dictionary<int, Texture> Textures { get; } = new();

            public Tileset(string name, string background)
            {
                Name = name;
                Background = Normalize(background);
            }
        }

        public static Dictionary<string, Tileset> Tilesets { get; } = new(StringComparer.OrdinalIgnoreCase);
        public static Dictionary<string, string> LevelTilesets { get; } = new(StringComparer.OrdinalIgnoreCase);
        public static string CurrentTilesetName { get; private set; } = "Download";

        public static Tileset CurrentTileset
        {
            get
            {
                if (Tilesets.TryGetValue(CurrentTilesetName, out Tileset? tileset))
                    return tileset;
                return Tilesets.Values.First();
            }
        }

        public static IReadOnlyDictionary<int, Texture> Current => CurrentTileset.Textures;

        static TextureCatalog()
        {
            RegisterBuiltIns();
            UseTileset("Download");
        }

        public static void UseTileset(string name)
        {
            if (Tilesets.ContainsKey(name))
                CurrentTilesetName = name;
        }

        public static IEnumerable<string> TilesetNames() => Tilesets.Keys.OrderBy(x => x, StringComparer.OrdinalIgnoreCase);

        public static string TextureName(int index)
        {
            return Current.TryGetValue(index, out Texture? texture) ? texture.Name : string.Format("texture#{0}", index);
        }

        public static string TextureFile(int index)
        {
            return Current.TryGetValue(index, out Texture? texture) ? texture.File : string.Format("texture#{0}", index);
        }

        public static Texture GetTexture(int index)
        {
            if (Current.TryGetValue(index, out Texture? texture))
                return texture;
            return new Texture(string.Format("texture#{0}", index), 40, 40);
        }

        public static string CurrentBackgroundFile() => CurrentTileset.Background;

        public static void UpdateTextureSize(int index, int width, int height)
        {
            if (Current.TryGetValue(index, out Texture? texture))
            {
                texture.Width = width;
                texture.Height = height;
            }
        }

        public static int LoadTilesetsFromXml(string xmlPath)
        {
            XDocument document = XDocument.Load(xmlPath);
            int count = 0;
            foreach (XElement tilesetElement in document.Descendants("tileset"))
            {
                string? name = (string?)tilesetElement.Attribute("name");
                if (string.IsNullOrWhiteSpace(name))
                    continue;

                Tilesets.TryGetValue(name, out Tileset? existingTileset);

                string background = (string?)tilesetElement.Attribute("background") ?? "";
                if (string.IsNullOrWhiteSpace(background) && existingTileset != null)
                    background = existingTileset.Background;

                Tileset tileset = new(name, background);
                int index = 0;
                foreach (XElement tileElement in tilesetElement.Elements("tile"))
                {
                    string? file = (string?)tileElement.Attribute("file");
                    if (string.IsNullOrWhiteSpace(file))
                        continue;

                    int width = 80;
                    int height = 80;

                    if (existingTileset != null && existingTileset.Textures.TryGetValue(index, out Texture? existingTexture))
                    {
                        width = existingTexture.Width;
                        height = existingTexture.Height;
                    }

                    if (int.TryParse((string?)tileElement.Attribute("width"), out int xmlWidth) && xmlWidth > 0)
                        width = xmlWidth;
                    if (int.TryParse((string?)tileElement.Attribute("height"), out int xmlHeight) && xmlHeight > 0)
                        height = xmlHeight;

                    tileset.Textures[index++] = new Texture(file, width, height);
                }
                if (tileset.Textures.Count == 0)
                    continue;
                Tilesets[tileset.Name] = tileset;
                count++;
            }

            foreach (XElement levelElement in document.Descendants("level"))
            {
                string? map = (string?)levelElement.Attribute("map");
                string? tileset = (string?)levelElement.Attribute("tileset");
                if (string.IsNullOrWhiteSpace(map) || string.IsNullOrWhiteSpace(tileset))
                    continue;
                string normalized = Normalize(map);
                LevelTilesets[normalized] = tileset;
                LevelTilesets[Path.GetFileName(normalized)] = tileset;
            }

            return count;
        }

        public static bool TryGetTilesetForLevelFile(string levelPath, out string tileset)
        {
            string normalized = Normalize(levelPath);
            string fileName = Path.GetFileName(normalized);
            if (LevelTilesets.TryGetValue(fileName, out tileset!))
                return true;
            foreach (KeyValuePair<string, string> item in LevelTilesets)
            {
                if (normalized.EndsWith(item.Key, StringComparison.OrdinalIgnoreCase))
                {
                    tileset = item.Value;
                    return true;
                }
            }
            tileset = "";
            return false;
        }

        public static string Normalize(string path) => path.Replace('\\', '/');

        private static void RegisterBuiltIns()
        {
            Tileset download = new("Download", "XS/gfx_download/BG_DL.jpg");
            Add(download, 0, "XS/gfx_download/BG_DL_bench01.bmp", 120, 80);
            Add(download, 1, "XS/gfx_download/BG_DL_boat01.bmp", 160, 120);
            Add(download, 2, "XS/gfx_download/BG_DL_bridge01.bmp", 160, 120);
            Add(download, 3, "XS/gfx_download/BG_DL_bridge_02.bmp", 160, 120);
            Add(download, 4, "XS/gfx_download/BG_DL_bridge_03.bmp", 160, 80);
            Add(download, 5, "XS/gfx_download/BG_DL_bush01.bmp", 200, 120);
            Add(download, 6, "XS/gfx_download/BG_DL_bush02.bmp", 160, 80);
            Add(download, 7, "XS/gfx_download/BG_DL_bush03.bmp", 80, 120);
            Add(download, 8, "XS/gfx_download/BG_DL_field01.bmp", 120, 80);
            Add(download, 9, "XS/gfx_download/BG_DL_fireplace01.bmp", 120, 80);
            Add(download, 10, "XS/gfx_download/BG_DL_grass01.bmp", 40, 40);
            Add(download, 11, "XS/gfx_download/BG_DL_grass02.bmp", 40, 40);
            Add(download, 12, "XS/gfx_download/BG_DL_grass03.bmp", 40, 80);
            Add(download, 13, "XS/gfx_download/BG_DL_grass04.bmp", 40, 80);
            Add(download, 14, "XS/gfx_download/BG_DL_grass05water.bmp", 40, 40);
            Add(download, 15, "XS/gfx_download/BG_DL_grass06water.bmp", 40, 80);
            Add(download, 16, "XS/gfx_download/BG_DL_puddle01.bmp", 80, 40);
            Add(download, 17, "XS/gfx_download/BG_DL_puddle02.bmp", 80, 40);
            Add(download, 18, "XS/gfx_download/BG_DL_rake01.bmp", 120, 80);
            Add(download, 19, "XS/gfx_download/BG_DL_river01.bmp", 400, 280);
            Add(download, 20, "XS/gfx_download/BG_DL_river02.bmp", 400, 280);
            Add(download, 21, "XS/gfx_download/BG_DL_river03.bmp", 400, 280);
            Add(download, 22, "XS/gfx_download/BG_DL_river04.bmp", 400, 280);
            Add(download, 23, "XS/gfx_download/BG_DL_tomatoes01.bmp", 120, 80);
            Add(download, 24, "XS/gfx_download/BG_DL_tomatoes02.bmp", 120, 80);
            Add(download, 25, "XS/gfx_download/BG_DL_tree01.bmp", 240, 280);
            Add(download, 26, "XS/gfx_download/BG_DL_bridge01_top.bmp", 160, 120);
            Add(download, 27, "XS/gfx_download/BG_riverdelta_bridge_09.bmp", 160, 120);
            Add(download, 28, "XS/gfx_download/BG_riverdelta_bridge_12.bmp", 80, 80);
            Add(download, 29, "XS/gfx_download/BG_riverdelta_bridge_14.bmp", 80, 80);
            Add(download, 30, "XS/gfx_download/BG_riverdelta_bridge_17.bmp", 160, 120);
            Tilesets[download.Name] = download;

            RegisterSimpleTileset("Barn", "XL/gfx_barn/BG_barn.jpg", new[]
            {
                "XL/gfx_barn/BG_barn_left.bmp", "XL/gfx_barn/BG_barn_right.bmp", "XL/gfx_barn/BG_barn_puddle_01.bmp", "XL/gfx_barn/BG_barn_puddle_02.bmp",
                "XL/gfx_barn/BG_barn_stanchion.bmp", "XL/gfx_barn/BG_barn_straw_01.bmp", "XL/gfx_barn/BG_barn_straw_02.bmp", "XL/gfx_barn/BG_barn_troth.bmp",
                "XL/gfx_barn/BG_barn_wall_01.bmp", "XL/gfx_barn/BG_barn_wall_02.bmp", "XL/gfx_barn/BG_barn_wall_03.bmp", "XL/gfx_barn/BG_barn_wall_04.bmp",
                "XL/gfx_barn/BG_barn_wall_05.bmp", "XL/gfx_barn/BG_barn_wall_06.bmp", "XL/gfx_barn/BG_barn_wall_07.bmp", "XL/gfx_barn/BG_barn_barrel.bmp",
                "XL/gfx_barn/BG_barn_ropehanger.bmp"
            });

            RegisterSimpleTileset("Farm", "XL/gfx_farm/BG_farm.jpg", new[]
            {
                "XL/gfx_farm/BG_Farm_barn.bmp", "XL/gfx_farm/BG_Farm_barndoor.bmp", "XL/gfx_farm/BG_Farm_birdhouse.bmp", "XL/gfx_farm/BG_Farm_bush_01.bmp",
                "XL/gfx_farm/BG_Farm_bush_02.bmp", "XL/gfx_farm/BG_Farm_bush_03.bmp", "XL/gfx_farm/BG_Farm_ceramic_01.bmp", "XL/gfx_farm/BG_Farm_ceramic_02.bmp",
                "XL/gfx_farm/BG_Farm_entrancestone.bmp", "XL/gfx_farm/BG_Farm_fence_01.bmp", "XL/gfx_farm/BG_Farm_fence_02.bmp", "XL/gfx_farm/BG_Farm_fence_03.bmp",
                "XL/gfx_farm/BG_Farm_fence_04.bmp", "XL/gfx_farm/BG_Farm_fence_05.bmp", "XL/gfx_farm/BG_Farm_house.bmp", "XL/gfx_farm/BG_Farm_manurepile.bmp",
                "XL/gfx_farm/BG_Farm_millstonedisks.bmp", "XL/gfx_farm/BG_Farm_puddle01.bmp", "XL/gfx_farm/BG_Farm_puddle02.bmp", "XL/gfx_farm/BG_Farm_tractor.bmp",
                "XL/gfx_farm/BG_Farm_tree_01.bmp", "XL/gfx_farm/BG_Farm_tree_02.bmp"
            });

            RegisterSimpleTileset("Grass", "XL/gfx_grass/BG_grass.jpg", new[]
            {
                "XL/gfx_grass/BG_grass_barrel_01.bmp", "XL/gfx_grass/BG_grass_barrel_02.bmp", "XL/gfx_grass/BG_grass_barrel_03.bmp", "XL/gfx_grass/BG_grass_bumper_01.bmp",
                "XL/gfx_grass/BG_grass_bush_01.bmp", "XL/gfx_grass/BG_grass_bush_02.bmp", "XL/gfx_grass/BG_grass_carwreck_01.bmp", "XL/gfx_grass/BG_grass_carwreck_02.bmp",
                "XL/gfx_grass/BG_grass_freezer.bmp", "XL/gfx_grass/BG_grass_motorbike_01.bmp", "XL/gfx_grass/BG_grass_puddle_01.bmp", "XL/gfx_grass/BG_grass_puddle_02.bmp",
                "XL/gfx_grass/BG_grass_repairshop.bmp", "XL/gfx_grass/BG_grass_stone_01.bmp", "XL/gfx_grass/BG_grass_stone_02.bmp", "XL/gfx_grass/BG_grass_stone_03.bmp",
                "XL/gfx_grass/BG_grass_stone_04.bmp", "XL/gfx_grass/BG_grass_stone_05.bmp", "XL/gfx_grass/BG_grass_tires_01.bmp", "XL/gfx_grass/BG_grass_tires_02.bmp",
                "XL/gfx_grass/BG_grass_tires_03.bmp", "XL/gfx_grass/BG_grass_tirewing_01.bmp", "XL/gfx_grass/BG_grass_washingmachine.bmp", "XL/gfx_grass/BG_grass_workhorse.bmp"
            });
        }

        private static void RegisterSimpleTileset(string name, string background, IEnumerable<string> files)
        {
            Tileset tileset = new(name, background);
            int i = 0;
            foreach (string file in files)
                Add(tileset, i++, file, 80, 80);
            Tilesets[tileset.Name] = tileset;
        }

        private static void Add(Tileset tileset, int index, string file, int width, int height)
        {
            tileset.Textures[index] = new Texture(file, width, height);
        }
    }
}
