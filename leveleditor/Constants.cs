namespace Editor
{
    static class Constants
    {
        public const int FieldByteSize = 4;
        public const int NumObjectFields = 25;
        public const int NumTileFields = 6;
        public const int TileWidth = 25;
        public const int TileHeight = 15;
        public const int TileCount = TileWidth * TileHeight;
        public const int ObjectRecordSize = NumObjectFields * FieldByteSize;
        public const int TileRecordSize = NumTileFields * FieldByteSize;
        public const int ObjectCount = 256;
        public const int ReservedCount = 11;
        public const int HiddenTileWidth = 5;
        public const int VisibleTileWidth = TileWidth - HiddenTileWidth;
        public const int CollisionStride = 22;
        public const int CollisionStartIndex = 23;
        public const float CellWidth = 40;
        public const float CellHeight = 40;
        public const float VisibleOriginX = 0;
        public const float VisibleOriginY = 0;
        public const float HiddenOriginX = 840;
        public const float HiddenOriginY = 0;
        public const double Width = 800;
        public const double Height = 600;
        public const int ChromaR = 235;
        public const int ChromaG = 40;
        public const int ChromaB = 235;
    }
}