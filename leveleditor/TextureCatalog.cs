namespace Editor
{
    static class TextureCatalog
    {
        public record Texture(string Name, int Width, int Height);

        //Example, TODO: Make this dynamic
        public static Dictionary<int, Texture> Download = new()
        {
            [0] = new("BG_DL_bench01.bmp", 120, 80),
            [1] = new("BG_DL_boat01.bmp", 160, 120),
            [2] = new("BG_DL_bridge01.bmp", 160, 120),
            [3] = new("BG_DL_bridge_02.bmp", 160, 120),
            [5] = new("BG_DL_bush01.bmp", 200, 120),
            [6] = new("BG_DL_bush02.bmp", 160, 80),
            [7] = new("BG_DL_bush03.bmp", 80, 120),
            [8] = new("BG_DL_field01.bmp", 120, 80),
            [9] = new("BG_DL_fireplace01.bmp", 120, 80),
            [10] = new("BG_DL_grass01.bmp", 40, 40),
            [11] = new("BG_DL_grass02.bmp", 40, 40),
            [12] = new("BG_DL_grass03.bmp", 40, 80),
            [13] = new("BG_DL_grass04.bmp", 40, 80),
            [14] = new("BG_DL_grass05water.bmp", 40, 40),
            [15] = new("BG_DL_grass06water.bmp", 40, 80),
            [16] = new("BG_DL_puddle01.bmp", 80, 40),
            [17] = new("BG_DL_puddle02.bmp", 80, 40),
            [18] = new("BG_DL_rake01.bmp", 120, 80),
            [19] = new("BG_DL_river01.bmp", 400, 280),
            [20] = new("BG_DL_river02.bmp", 400, 280),
            [21] = new("BG_DL_river03.bmp", 400, 280),
            [22] = new("BG_DL_river04.bmp", 400, 280),
            [23] = new("BG_DL_tomatoes01.bmp", 120, 80),
            [24] = new("BG_DL_tomatoes02.bmp", 120, 80),
            [25] = new("BG_DL_tree01.bmp", 240, 280),
            [26] = new("BG_DL_bridge01_top.bmp", 160, 120),
            [27] = new("BG_riverdelta_bridge_09.bmp", 160, 120),
            [30] = new("BG_riverdelta_bridge_17.bmp", 160, 120),
        };

        public static string TextureName(int index) => Download.TryGetValue(index, out Texture? t) ? t.Name : string.Format("texture#{0}", index);
    }
}