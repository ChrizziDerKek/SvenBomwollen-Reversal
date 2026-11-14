using System.IO;
using System.Text;

namespace Editor
{
    internal sealed class LevelFile
    {
        private static readonly int[] SvenTemplate =
        {
            1127481344, 1135869952, 1133248512, 1135869952, 1133248512, 0, 120, 120, 1123024896, 1123024896,
            0, 0, 0, 6, 0, 1097859072, 14, 0, 8, 0,
            0, 0, -1, 0, 30
        };

        private static readonly int[] InactiveTemplate =
        {
            0, 1137836032, 1136525312, 1137836032, 1136525312, 0, 160, 160, 0, 1123024896,
            0, 0, -1, 14, 1099525120, 1092616192, 0, 0, 0, 10,
            2, 299, 0, 1097859072, 40
        };

        public byte[] Data { get; private set; }
        public int Version { get; private set; }
        public int HeaderUnknown0 { get; private set; }
        public int HeaderUnknown1 { get; private set; }
        public string Name { get; private set; } = "";
        public int TileOffset { get; private set; }
        public int ObjectOffset { get; private set; }
        public List<TileRecord> Tiles { get; private set; } = new();
        public List<ObjectRecord> Objects { get; private set; } = new();
        public string? SourcePath { get; set; }

        private LevelFile(byte[] data, string? sourcePath)
        {
            Data = data;
            SourcePath = sourcePath;
            Rebind();
        }

        public static LevelFile Load(string path)
        {
            byte[] data = File.ReadAllBytes(path);
            LevelFile level = new(data, path);
            level.ValidateSize();
            return level;
        }

        public static LevelFile CreateBlank(string name)
        {
            if (string.IsNullOrWhiteSpace(name))
                name = "blank";

            byte[] nameBytes = Encoding.ASCII.GetBytes(name);
            int bodySize = Constants.TileCount * Constants.TileRecordSize + Constants.ReservedCount * Constants.FieldByteSize + Constants.ObjectCount * Constants.ObjectRecordSize + Constants.FieldByteSize;
            byte[] data = new byte[16 + nameBytes.Length + bodySize];
            BaseRecord.WriteInt(data, 0, 6);
            BaseRecord.WriteInt(data, 4, 0);
            BaseRecord.WriteInt(data, 8, 0);
            BaseRecord.WriteInt(data, 12, nameBytes.Length);
            Buffer.BlockCopy(nameBytes, 0, data, 16, nameBytes.Length);

            LevelFile level = new(data, null);
            BaseRecord.WriteInt(data, level.ObjectOffset, BitConverter.SingleToInt32Bits(150f));
            for (int i = 1; i < Constants.ReservedCount; i++)
                BaseRecord.WriteInt(data, level.ObjectOffset - Constants.ReservedCount * 4 + i * 4, -1);
            level.Object(0).WriteInts(SvenTemplate);
            for (int i = 1; i < Constants.ObjectCount; i++)
                level.SetInactiveObject(i);
            return level;
        }

        public void Save(string path)
        {
            File.WriteAllBytes(path, Data);
            SourcePath = path;
        }

        // Export a copy. Version selection does not change the working level or actor setup.
        public byte[] ExportBytes(int? version) => ExportBytes(Data, version);

        public static byte[] ExportBytes(byte[] source, int? version)
        {
            if (version == null) return (byte[])source.Clone();
            if (version is not (6 or 7)) throw new InvalidDataException("Only Zwo v6 and XXX v7 export layouts are supported. Sven 004 v12 requires a different grid and object format.");
            LevelFile level = new((byte[])source.Clone(), null);
            int coreSize = level.TileOffset + 34648;
            byte[] output = new byte[coreSize + (version == 7 ? 4096 : 0)];
            Buffer.BlockCopy(source, 0, output, 0, Math.Min(source.Length, output.Length));
            BaseRecord.WriteInt(output, 0, version.Value);
            if (version == 7 && source.Length == coreSize)
                for (int i = 0; i < 256; i++)
                {
                    // Default extension tuple observed in the shipped XXX levels.
                    BaseRecord.WriteInt(output, coreSize + i * 16 + 8, 400);
                    BaseRecord.WriteInt(output, coreSize + i * 16 + 12, 2);
                }
            return output;
        }

        public byte[] Snapshot()
        {
            byte[] copy = new byte[Data.Length];
            Buffer.BlockCopy(Data, 0, copy, 0, Data.Length);
            return copy;
        }

        public void Restore(byte[] snapshot)
        {
            Data = new byte[snapshot.Length];
            Buffer.BlockCopy(snapshot, 0, Data, 0, snapshot.Length);
            Rebind();
        }

        public void ClearToBlankKeepingSven()
        {
            for (int i = 0; i < Constants.TileCount * Constants.TileRecordSize; i++)
                Data[TileOffset + i] = 0;

            ObjectRecord sven = Object(0);
            int[] svenFields = sven.GetInts();
            sven.WriteInts(svenFields);
            for (int i = 1; i < Constants.ObjectCount; i++)
                SetInactiveObject(i);
        }

        public TileRecord Tile(int rawX, int rawY)
        {
            if (rawX < 0 || rawX >= Constants.TileWidth || rawY < 0 || rawY >= Constants.TileHeight)
                throw new ArgumentOutOfRangeException();
            return Tiles[rawY * Constants.TileWidth + rawX];
        }

        public TileRecord TileByRawIndex(int rawIndex)
        {
            if (rawIndex < 0 || rawIndex >= Constants.TileCount)
                throw new ArgumentOutOfRangeException(nameof(rawIndex));
            return Tiles[rawIndex];
        }

        public ObjectRecord Object(int index)
        {
            if (index < 0 || index >= Constants.ObjectCount)
                throw new ArgumentOutOfRangeException(nameof(index));
            return Objects[index];
        }

        public int FindInactiveObject()
        {
            for (int i = 1; i < Constants.ObjectCount; i++)
                if (Object(i).GetInt(12) == -1)
                    return i;
            return -1;
        }

        public void SetInactiveObject(int index)
        {
            Object(index).WriteInts(InactiveTemplate);
        }

        public IEnumerable<int> ObjectReferencesFromTiles(int objectIndex)
        {
            for (int i = 0; i < Constants.TileCount; i++)
            {
                TileRecord tile = TileByRawIndex(i);
                if ((tile.GetInt(0) is 2 or 3 or 4 or 5 or 6 or 7 or 8 or 9 && tile.GetInt(2) == objectIndex) ||
                    (tile.GetInt(0) == 0 && tile.GetInt(3) is >= 128 and <= 131 && tile.GetInt(4) == objectIndex))
                    yield return i;
            }
        }

        public void RemoveTileReferencesToObject(int objectIndex)
        {
            foreach (int raw in ObjectReferencesFromTiles(objectIndex).ToList())
            {
                TileRecord tile = TileByRawIndex(raw);
                if (tile.GetInt(0) == 0) { tile.SetInt(3, 0); tile.SetInt(4, 0); }
                else { tile.SetInt(0, 0); tile.SetInt(1, 0); tile.SetInt(2, 0); }
            }
        }

        public float TimeLimit { get => BitConverter.ToSingle(Data, ObjectOffset); set => BaseRecord.WriteFloat(Data, ObjectOffset, value); }
        public int TeleporterCount => BitConverter.ToInt32(Data, TileOffset + Constants.TileCount * 24);
        public int TeleporterDestination(int index) => BitConverter.ToInt32(Data, TileOffset + Constants.TileCount * 24 + 4 + index * 4);
        public void SetTeleporters(IReadOnlyList<int> destinations)
        {
            if (destinations.Count > 16) throw new InvalidOperationException("At most 16 teleporters are supported.");
            int offset = TileOffset + Constants.TileCount * 24;
            BaseRecord.WriteInt(Data, offset, destinations.Count);
            for (int i = 0; i < 16; i++) BaseRecord.WriteInt(Data, offset + 4 + i * 4, i < destinations.Count ? destinations[i] : -1);
        }

        private void Rebind()
        {
            if (Data.Length < 16)
                throw new InvalidDataException("File too small.");
            Version = BitConverter.ToInt32(Data, 0);
            if (Version is not (6 or 7))
                throw new InvalidDataException($"Level version {Version} is not supported for editing. Zwo v6 and XXX v7 use the supported grid; 004 v12 has a larger grid and different object records.");
            HeaderUnknown0 = BitConverter.ToInt32(Data, 4);
            HeaderUnknown1 = BitConverter.ToInt32(Data, 8);
            int nameLength = BitConverter.ToInt32(Data, 12);
            if (nameLength < 0 || nameLength > 4096)
                throw new InvalidDataException($"Invalid level name length: {nameLength}");
            TileOffset = 16 + nameLength;
            if (TileOffset > Data.Length) throw new InvalidDataException("Truncated level name.");
            ValidateSize();
            Name = Encoding.ASCII.GetString(Data, 16, nameLength);
            ObjectOffset = TileOffset + Constants.TileCount * Constants.TileRecordSize + Constants.ReservedCount * Constants.FieldByteSize;
            Tiles = Enumerable.Range(0, Constants.TileCount).Select(i => new TileRecord(Data, TileOffset + i * Constants.TileRecordSize)).ToList();
            Objects = Enumerable.Range(0, Constants.ObjectCount).Select(i => new ObjectRecord(Data, ObjectOffset + i * Constants.ObjectRecordSize)).ToList();
        }

        private void ValidateSize()
        {
            int v6Body = Constants.TileCount * Constants.TileRecordSize + Constants.ReservedCount * Constants.FieldByteSize + Constants.ObjectCount * Constants.ObjectRecordSize + Constants.FieldByteSize;
            int v6Size = TileOffset + v6Body;
            if (Data.Length == v6Size)
                return;
            if (Version == 7 && Data.Length == v6Size + 4096)
                return;
            throw new InvalidDataException($"Invalid level size. Expected {v6Size} bytes for v6, got {Data.Length}.");
        }
    }
}
