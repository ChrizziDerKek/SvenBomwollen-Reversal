namespace Editor
{
    internal class BaseRecord
    {
        protected readonly byte[] Data;
        protected readonly int Offset;
        private readonly int NumFields;

        protected BaseRecord(byte[] data, int offset, int numFields)
        {
            Data = data;
            Offset = offset;
            NumFields = numFields;
        }

        public int GetInt(int field)
        {
            EnsureField(field);
            return BitConverter.ToInt32(Data, Offset + field * Constants.FieldByteSize);
        }

        public void SetInt(int field, int value)
        {
            EnsureField(field);
            WriteInt(Data, Offset + field * Constants.FieldByteSize, value);
        }

        public float GetFloat(int field)
        {
            EnsureField(field);
            return BitConverter.ToSingle(Data, Offset + field * Constants.FieldByteSize);
        }

        public void SetFloat(int field, float value)
        {
            EnsureField(field);
            WriteFloat(Data, Offset + field * Constants.FieldByteSize, value);
        }

        public int[] GetInts()
        {
            int[] result = new int[NumFields];
            for (int i = 0; i < NumFields; i++)
                result[i] = GetInt(i);
            return result;
        }

        public void CopyFrom(BaseRecord other)
        {
            Buffer.BlockCopy(other.Data, other.Offset, Data, Offset, NumFields * Constants.FieldByteSize);
        }

        public void WriteInts(IReadOnlyList<int> values)
        {
            for (int i = 0; i < NumFields && i < values.Count; i++)
                SetInt(i, values[i]);
        }

        private void EnsureField(int field)
        {
            if (field < 0 || field >= NumFields)
                throw new ArgumentOutOfRangeException(nameof(field));
        }

        public static void WriteInt(byte[] data, int offset, int value)
        {
            byte[] bytes = BitConverter.GetBytes(value);
            Buffer.BlockCopy(bytes, 0, data, offset, bytes.Length);
        }

        public static void WriteFloat(byte[] data, int offset, float value)
        {
            byte[] bytes = BitConverter.GetBytes(value);
            Buffer.BlockCopy(bytes, 0, data, offset, bytes.Length);
        }
    }
}
