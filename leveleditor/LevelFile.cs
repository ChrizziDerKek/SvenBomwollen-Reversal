using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Editor
{
    class LevelFile
    {
        public record TileRef(int X, int Y, TileRecord Tile);
        public byte[] Data { get; }
        public int Version { get; }
        public string Name { get; }
        public int TileOffset { get; }
        public int ObjectOffset { get; }
        public List<TileRecord> Tiles { get; }
        public List<ObjectRecord> Objects { get; }

        public LevelFile(byte[] data, int version, string name, int tileOffset, int objectOffset)
        {
            Data = data;
            Version = version;
            Name = name;
            TileOffset = tileOffset;
            ObjectOffset = objectOffset;
            Tiles = [.. Enumerable.Range(0, Constants.TileCount).Select(x => new TileRecord(data, TileOffset + x * Constants.TileRecordSize))];
            Objects = [.. Enumerable.Range(0, Constants.ObjectCount).Select(x => new ObjectRecord(data, ObjectOffset + x * Constants.ObjectRecordSize))];
        }

        public static LevelFile Load(string path)
        {
            byte[] data = File.ReadAllBytes(path);
            if (data.Length < 16)
                throw new InvalidDataException("File too small");
            int version = BitConverter.ToInt32(data, 0);
            int length = BitConverter.ToInt32(data, 12);
            if (length < 0 || length > 1024)
                throw new InvalidDataException(string.Format("Invalid level name length: {0}", length));
            int tileOffset = 16 + length;
            int expectedBody = Constants.TileCount * Constants.TileRecordSize + Constants.ReservedCount * Constants.FieldByteSize + Constants.ObjectCount * Constants.ObjectRecordSize + Constants.FieldByteSize;
            if (tileOffset + expectedBody != data.Length)
                throw new InvalidDataException(string.Format("Invalid level size. Expected {0}, got {1}", tileOffset + expectedBody, data.Length));
            string name = Encoding.ASCII.GetString(data, 16, length);
            int objectOffset = tileOffset + Constants.TileCount * Constants.TileRecordSize + Constants.ReservedCount * Constants.FieldByteSize;
            return new(data, version, name, tileOffset, objectOffset);
        }

        public void Save(string path) => File.WriteAllBytes(path, Data);

        public TileRecord Tile(int x, int y)
        {
            Helpers.EnsureValue(x, 0, Constants.TileWidth);
            Helpers.EnsureValue(y, 0, Constants.TileHeight);
            return Tiles[y * Constants.TileWidth + x];
        }

        public ObjectRecord Object(int index)
        {
            Helpers.EnsureValue(index, 0, Constants.ObjectCount);
            return Objects[index];
        }

        public IEnumerable<TileRef> TileReferencesToObject(int index)
        {
            for (int y = 0; y < Constants.TileHeight; y++)
            {
                for (int x = 0; x < Constants.TileWidth; x++)
                {
                    TileRecord tile = Tile(x, y);
                    if (tile.GetInt(2) == index && tile.GetInt(4) == index)
                        yield return new(x, y, tile);
                }
            }
        }

        public int FindInactiveObject(int? except = null)
        {
            for (int i = 1; i < Constants.ObjectCount; i++)
            {
                if (except.HasValue && except.Value == i)
                    continue;
                if (Object(i).GetInt(12) == -1)
                    return i;
            }
            return -1;
        }

        public int FindPreviousStatic(int index)
        {
            for (int i = index - 1; i >= 1; i--)
                if (Object(i).GetInt(12) == 1)
                    return i;
            return -1;
        }

        public int FindNextStatic(int index)
        {
            for (int i = index + 1; i < Constants.ObjectCount; i++)
                if (Object(i).GetInt(12) == 1)
                    return i;
            return -1;
        }

        public void SwapObjects(int a, int b)
        {
            byte[] temp = new byte[Constants.ObjectRecordSize];
            Buffer.BlockCopy(Data, ObjectOffset + a * temp.Length, temp, 0, temp.Length);
            Buffer.BlockCopy(Data, ObjectOffset + b * temp.Length, Data, ObjectOffset + a * temp.Length, temp.Length);
            Buffer.BlockCopy(temp, 0, Data, ObjectOffset + b * temp.Length, temp.Length);
        }
    }
}