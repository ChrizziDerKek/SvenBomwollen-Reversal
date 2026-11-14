using System.Windows.Media;

namespace Editor
{
    internal enum EditorToolKind
    {
        Select,
        PlaceStatic,
        PlaceCharacter,
        PaintTile,
        MoveSven
    }

    internal enum CharacterKind
    {
        SvenStart = 0,
        Sheep = 2,
        Alien = 3,
        Beer = 7,
        Teddy = 8,
        Bone = 9,
        Artifact = 10,
        Chicken = 11,
        Goat = 12,
        Pig = 13,
        DogWotan = 4,
        FarmerLars = 5,
        GirlBromse = 6
    }

    internal sealed class EditorToolState
    {
        public EditorToolKind ActiveTool { get; set; } = EditorToolKind.Select;
        public int SelectedStaticTextureId { get; set; } = 0;
        public CharacterKind SelectedCharacter { get; set; } = CharacterKind.Sheep;
        public int SelectedTileType { get; set; } = 1;
        public int? SelectedObjectIndex { get; set; }
        public bool SnapStaticsToGrid { get; set; } = true;
    }

    internal sealed record StaticPaletteItem(int TextureId, string Name, int Width, int Height, ImageSource? PreviewImage)
    {
        public string Header => $"[{TextureId:00}] {Name}";
        public string SizeText => $"{Width} x {Height}";
        public bool HasPreview => PreviewImage != null;
    }

    internal sealed record CharacterPaletteItem(CharacterKind Kind, string Name, string Description)
    {
        public int Id => (int)Kind;
    }

    internal sealed record TilePaletteItem(int Type, string Name, string Fields)
    {
        public string Header => $"T{Type:00} {Name}";
    }

    internal sealed record ObjectListItem(int Index, string Label)
    {
        public override string ToString() => Label;
    }
}
