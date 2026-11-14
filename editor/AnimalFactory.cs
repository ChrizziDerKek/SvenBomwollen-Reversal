namespace Editor;

// Named defaults recovered from sub_40BA60 (sheep and animal cases).
// Facing uses the four frames of the standing sprite; no level fixture is read.
internal static class AnimalFactory
{
    public static bool IsAnimal(int kind) => kind is 2 or 11 or 12 or 13;

    public static void Initialize(ObjectRecord obj, int kind, int facing = 0, int sheepAnger = 3)
    {
        if (!IsAnimal(kind)) throw new ArgumentOutOfRangeException(nameof(kind));
        Validate(facing, sheepAnger);
        obj.WriteInts(new int[26]);
        obj.SetInt(6, 120); obj.SetInt(7, 120);
        obj.SetInt(12, kind);
        obj.SetInt(13, facing); // standing sprite frame
        obj.SetFloat(14, kind == 2 ? 900f : 0f); // idle delay, game uses 450..1449 for sheep
        obj.SetFloat(15, kind == 2 ? 20f : 15f); // animation rate
        obj.SetInt(16, 4); // standing sheet frame count
        obj.SetInt(18, -1); // no target actor
        obj.SetInt(20, facing); // logical direction
        obj.SetInt(22, kind == 2 ? sheepAnger * 2 : 6);
        obj.SetFloat(23, 25f); // indicator animation timer
        obj.SetInt(24, 40); // depth offset
    }

    public static void SetSettings(ObjectRecord obj, int facing, int sheepAnger)
    {
        if (!IsAnimal(obj.GetInt(12))) throw new InvalidOperationException("Select a sheep, chicken, goat, or pig.");
        Validate(facing, sheepAnger);
        obj.SetInt(20, facing);
        if (obj.GetInt(19) is 0 or 1) obj.SetInt(13, facing);
        if (obj.GetInt(12) == 2) obj.SetInt(22, sheepAnger * 2);
        // Preserve packed special-state bytes in f17, timers and linked tile.
    }

    private static void Validate(int facing, int anger)
    {
        if (facing is < 0 or > 3) throw new ArgumentOutOfRangeException(nameof(facing));
        if (anger is < 0 or > 5) throw new ArgumentOutOfRangeException(nameof(anger));
    }
}
