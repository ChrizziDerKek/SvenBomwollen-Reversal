namespace Editor;

internal static class DrawOrder
{
    // sub_40D6F0: Y + height - integer depth offset; the game uses pair swaps.
    public static double Depth(ObjectRecord obj) => obj.GetFloat(2) + obj.GetInt(7) - (double)obj.GetInt(24);
    public static List<int> Indices(LevelFile level)
    {
        var list = Enumerable.Range(0, Constants.ObjectCount)
            .Where(i => level.Object(i).GetInt(12) != -1 && level.Object(i).GetInt(5) == 0).ToList();
        for (int i = 0; i < list.Count - 1; i++)
            for (int j = i + 1; j < list.Count; j++)
                if (Depth(level.Object(list[i])) > Depth(level.Object(list[j])))
                    (list[i], list[j]) = (list[j], list[i]);
        list.AddRange(Enumerable.Range(0, Constants.ObjectCount).Where(i => level.Object(i).GetInt(5) == 1));
        return list;
    }
}
