using System.Buffers.Binary;
using System.Text;

class Program
{
    const uint OffsetKey = 0xFFAA5533;
    const uint SizeKey = 0x3355AAFF;
    const byte XorKey = 0x88;
    const int TreePrefixSize = 17;
    const int MinLength = 64;
    const string Header = "MUDGE4.0";
    const int HeaderLength = 8;
    const int EndOffset = 56;

    static void Main(string[] args)
    {
        if (args.Length < 2)
        {
            Console.WriteLine("Usage: decoder <file.dat|file.pak> <output-folder>");
            return;
        }
        try
        {
            ExtractArchive(args[0], args[1]);
        }
        catch (Exception ex)
        {
            Console.WriteLine("Error: " + ex.Message);
        }
    }

    static void ExtractArchive(string archive, string output)
    {
        byte[] raw = File.ReadAllBytes(archive);
        if (raw.Length < MinLength || !raw.AsSpan(0, HeaderLength).SequenceEqual(Encoding.ASCII.GetBytes(Header)))
            throw new("Not a " + Header + " archive");
        uint dataend = ReadInteger(raw, EndOffset); //End offset of payload data is at offset 56
        int root = checked((int)dataend + TreePrefixSize); //17 bytes of padding are added
        if (root < 0 || root >= raw.Length)
            throw new("Archive root out of bounds");
        List<ArchiveEntry> entries = [];
        ParseNode(raw, root, entries, "");
        string rootpath = Path.GetFullPath(output);
        Directory.CreateDirectory(rootpath);
        foreach (ArchiveEntry entry in entries)
        {
            int start = checked((int)entry.Offset);
            int size = checked((int)entry.Size);
            int end = checked(start + size);
            if (start < 0 || end > raw.Length)
                throw new("Entry " + entry.Path + " invalid");
            byte[] decoded = new byte[size];
            for (int i = 0; i < size; i++)
                decoded[i] = (byte)(raw[start + i] ^ XorKey);
            string path = Path.GetFullPath(Path.Combine(rootpath, NormalizePath(entry.Path)));
            if (!path.StartsWith(rootpath + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase) && !string.Equals(path, rootpath, StringComparison.OrdinalIgnoreCase))
                throw new("Unsafe output path in entry " + entry.Path);
            Directory.CreateDirectory(Path.GetDirectoryName(path) ?? "");
            File.WriteAllBytes(path, decoded);
        }
        Console.WriteLine("Extracted " + entries.Count + " files from " + Path.GetFileName(archive) + " into " + output);
    }

    static int ParseNode(byte[] table, int position, List<ArchiveEntry> output, string parent)
    {
        /*
        NODE HEADER
        - node type (1 byte)
        - name hash (4 bytes)
        - name length (4 bytes)
        - name (n bytes)
        */
        if (position >= table.Length)
            throw new("Out of bounds position when reading node type");
        byte type = table[position++];
        if (position + 4 > table.Length)
            throw new("Out of bounds position when reading name hash");
        byte[] namehash = [.. table.AsSpan(position, 4)];
        position += 4;
        uint length = ReadInteger(table, position);
        position += 4;
        if (length > int.MaxValue || position + (int)length > table.Length)
            throw new("Out of bounds position when reading node name");
        string name = Encoding.Latin1.GetString(table, position, (int)length);
        position += (int)length;
        string fullpath = string.IsNullOrEmpty(parent) ? name : parent + "/" + name;
        /*
        NODE TYPE 1 (DIRECTORY)
        - NODE HEADER
        - number of children (4 bytes)
        - children (n bytes)
        */
        if (type == 1)
        {
            uint nchildren = ReadInteger(table, position);
            position += 4;
            for (uint i = 0; i < nchildren; i++)
                position = ParseNode(table, position, output, fullpath);
            return position;
        }
        /*
        NODE TYPE 2 (FILE)
		- NODE HEADER
        - flags (4 bytes)
        - encrypted offset (4 bytes, xored with OffsetKey)
        - encrypted size (4 bytes, xored with SizeKey)
        - unknown data (4 bytes)
        */
        if (type == 2)
        {
            uint flags = ReadInteger(table, position);
            position += 4;
            uint offset = ReadInteger(table, position);
            position += 4;
            uint size = ReadInteger(table, position);
            position += 4;
            uint unk = ReadInteger(table, position);
            position += 4;
            offset ^= OffsetKey;
            size ^= SizeKey;
            output.Add(new(fullpath, namehash, flags, offset, size, unk));
            return position;
        }
        throw new("Unknown node type " + type);
    }

    static uint ReadInteger(byte[] buffer, int offset)
    {
        if (offset < 0 || offset + 4 > buffer.Length)
            throw new("Failed to read integer");
        return BinaryPrimitives.ReadUInt32LittleEndian(buffer.AsSpan(offset, 4));
    }

    static string NormalizePath(string archivePath) => Path.Combine(archivePath.Replace('/', Path.DirectorySeparatorChar).Replace('\\', Path.DirectorySeparatorChar).Split(Path.DirectorySeparatorChar, StringSplitOptions.RemoveEmptyEntries));
    
    record ArchiveEntry(string Path, byte[] NameHash, uint Flags, uint Offset, uint Size, uint Unk);
}