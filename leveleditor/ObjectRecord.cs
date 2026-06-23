namespace Editor
{
    class ObjectRecord : BaseRecord
    {
        public ObjectRecord(byte[] data, int offset)
            : base(data, offset, Constants.NumObjectFields)
        { }
    }
}