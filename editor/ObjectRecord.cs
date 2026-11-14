namespace Editor;

// Retain the handoff's field numbering: f1 is X, f12 is kind.
// f0 belongs to the previous record (or the level timer), not this object.
internal sealed class ObjectRecord : BaseRecord
{
    public ObjectRecord(byte[] data, int offset) : base(data, offset, 26) { }
    public new void WriteInts(IReadOnlyList<int> values)
    {
        for (int i = 1; i <= 25; i++) SetInt(i, i < values.Count ? values[i] : 0);
    }
    public void CopyFrom(ObjectRecord other)
    {
        for (int i = 1; i <= 25; i++) SetInt(i, other.GetInt(i));
    }
}
