namespace Editor
{
    class BaseRecord
    {
        protected readonly byte[] Data;
        protected readonly int Offset;
        private readonly int NumFields;

        public BaseRecord(byte[] data, int offset, int nfields)
        {
            Data = data;
            Offset = offset;
            NumFields = nfields;
        }

        public int GetInt(int field)
        {
            Helpers.EnsureValue(field, 0, NumFields);
            return BitConverter.ToInt32(Data, Offset + field * Constants.FieldByteSize);
        }

        public void SetInt(int field, int value)
        {
            Helpers.EnsureValue(field, 0, NumFields);
            Helpers.Write(Data, Offset + field * Constants.FieldByteSize, value);
        }

        public int[] GetInts()
        {
            int[] result = new int[NumFields];
            for (int i = 0; i < NumFields; i++)
                result[i] = GetInt(i);
            return result;
        }

        public float GetFloat(int field)
        {
            Helpers.EnsureValue(field, 0, NumFields);
            return BitConverter.ToSingle(Data, Offset + field * Constants.FieldByteSize);
        }

        public void SetFloat(int field, float value)
        {
            Helpers.EnsureValue(field, 0, NumFields);
            Helpers.Write(Data, Offset + field * Constants.FieldByteSize, value);
        }

        public float[] GetFloats()
        {
            float[] result = new float[NumFields];
            for (int i = 0; i < NumFields; i++)
                result[i] = GetFloat(i);
            return result;
        }

        public void CopyFrom(BaseRecord that) => Buffer.BlockCopy(that.Data, that.Offset, Data, Offset, Constants.FieldByteSize * NumFields);
    }
}