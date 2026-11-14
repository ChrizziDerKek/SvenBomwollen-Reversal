namespace Editor
{
    internal static class Constants
    {
        public const int FieldByteSize = 4;
        public const int NumObjectFields = 25;
        public const int NumTileFields = 6;
        public const int TileWidth = 25;
        public const int TileHeight = 15;
        public const int TileCount = 374;
        public const int ObjectCount = 256;
        public const int ReservedCount = 17;
        public const int ObjectRecordSize = NumObjectFields * FieldByteSize;
        public const int TileRecordSize = NumTileFields * FieldByteSize;
        public const int VisibleTileWidth = 20;
        public const int CollisionStride = 22;
        public const int CollisionStartIndex = 23;
        public const float CellWidth = 40;
        public const float CellHeight = 40;
        public const double LevelWidth = 800;
        public const double LevelHeight = 600;
        public const byte ChromaR = 235;
        public const byte ChromaG = 40;
        public const byte ChromaB = 235;
    }
}
