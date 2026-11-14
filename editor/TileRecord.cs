namespace Editor
{
    internal sealed class TileRecord : BaseRecord
    {
        public TileRecord(byte[] data, int offset)
            : base(data, offset, Constants.NumTileFields)
        { }
    }
}
