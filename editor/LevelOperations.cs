namespace Editor
{
    internal static class LevelOperations
    {
        private static readonly int[] DogTemplate =
        {
            0, 1143603200, 1126170624, 1143603200, 1126170624, 0, 120, 120, 1123024896, 1123024896,
            0, 0, 4, 0, 0, 1101004800, 4, 0, 0, 10,
            0, 151, 0, 1086324685, 40
        };

        private static readonly int[] FarmerTemplate =
        {
            0, 1101004800, 1127481344, 1101004800, 1127481344, 0, 160, 160, 0, 1123024896,
            0, 0, 5, 13, 1094713312, 1092616192, 0, 0, 0, 10,
            0, 179, 0, -1213227008, 40
        };

        private static readonly int[] GirlTemplate =
        {
            0, 1128792064, 1101004800, 1128792064, 1101004800, 0, 120, 120, 1123024896, 1123024896,
            0, 0, 6, 5, 1083179008, 1101004800, 0, 0, 0, 0,
            5, 73, 0, 1083179008, 40
        };

        public static int SpawnStatic(LevelFile level, int textureId, int cx, int cy)
        {
            int slot = level.FindInactiveObject();
            if (slot < 0)
                throw new InvalidOperationException("No inactive object slot found.");
            ObjectRecord obj = level.Object(slot);
            TextureCatalog.Texture tex = TextureCatalog.GetTexture(textureId);
            float x = cx * Constants.CellWidth;
            float y = cy * Constants.CellHeight;
            level.SetInactiveObject(slot);
            obj.SetFloat(1, x);
            obj.SetFloat(2, y);
            obj.SetFloat(3, x);
            obj.SetFloat(4, y);
            obj.SetInt(6, tex.Width);
            obj.SetInt(7, tex.Height);
            obj.SetInt(12, 1);
            obj.SetInt(18, textureId);
            return slot;
        }

        public static int SpawnCharacter(LevelFile level, CharacterKind character, int cx, int cy, int animalFacing = 0, int sheepAnger = 3)
        {
            if (character == CharacterKind.SvenStart)
            {
                MoveSven(level, cx, cy);
                return 0;
            }

            int slot = level.FindInactiveObject();
            if (slot < 0)
                throw new InvalidOperationException("No inactive object slot found.");

            int kind = (int)character;
            ObjectRecord obj = level.Object(slot);
            EnsureActorDestination(level, slot, cx, cy);
            if (AnimalFactory.IsAnimal(kind)) AnimalFactory.Initialize(obj, kind, animalFacing, sheepAnger);
            else
            {
                int[] template = kind switch
                {
                    4 => DogTemplate,
                    5 => FarmerTemplate,
                    6 => GirlTemplate,
                    _ => ActorTemplates.Extra.TryGetValue(kind, out var extra) ? extra : throw new InvalidOperationException($"Unsupported character kind {kind}"),
                };
                obj.WriteInts(template);
            }
            SetCharacterPosition(obj, kind, cx, cy);
            WriteLinkedActorTile(level, slot, kind, cx, cy);
            return slot;
        }

        public static void MoveSven(LevelFile level, int cx, int cy)
        {
            ObjectRecord obj = level.Object(0);
            float x = cx * Constants.CellWidth - 40;
            float y = cy * Constants.CellHeight - 40;
            obj.SetFloat(1, x);
            obj.SetFloat(2, y);
            obj.SetFloat(3, x);
            obj.SetFloat(4, y);
        }

        public static void MoveObject(LevelFile level, int index, float x, float y, bool finalizeActorTile)
        {
            ObjectRecord obj = level.Object(index);
            int kind = obj.GetInt(12);
            if (finalizeActorTile && Helpers.IsActorKind(kind))
            {
                var cell = CharacterCellFromPosition(kind, x, y);
                EnsureActorDestination(level, index, cell.CX, cell.CY);
            }
            obj.SetFloat(1, x);
            obj.SetFloat(2, y);
            obj.SetFloat(3, x);
            obj.SetFloat(4, y);

            if (finalizeActorTile && Helpers.IsActorKind(kind))
            {
                (int cx, int cy) = CharacterCellFromPosition(kind, x, y);
                SetCharacterPosition(obj, kind, cx, cy);
                WriteLinkedActorTile(level, index, kind, cx, cy);
            }
        }

        public static void DeleteObject(LevelFile level, int index)
        {
            if (index <= 0)
                return;
            level.RemoveTileReferencesToObject(index);
            level.SetInactiveObject(index);
        }

        public static void PaintTile(LevelFile level, int cx, int cy, int type)
        {
            if (!Helpers.GridCellToRaw(cx, cy, out int rawX, out int rawY))
                return;
            TileRecord tile = level.Tile(rawX, rawY);
            if (type is not (0 or 1 or 10 or 11 or 140 or 141 or 142))
                throw new InvalidOperationException("Use a character tool for linked actors and Rebuild T3 for controllers.");
            if (tile.GetInts().Take(5).Any(v => v != 0) && tile.GetInt(0) is not (1 or 10 or 11))
                throw new InvalidOperationException("This cell contains actor, controller, or special data. Move/delete its object or use the raw inspector.");
            if (type is >= 140 and <= 142)
            {
                int raw = rawY * Constants.TileWidth + rawX;
                if (Enumerable.Range(0, Constants.TileCount).Count(i => i != raw && level.TileByRawIndex(i).GetInt(5) == type) >= 16)
                    throw new InvalidOperationException("The game supports at most 16 spawn markers per group.");
                tile.SetInt(5, type);
                return;
            }
            tile.SetInt(0, type);
            if (type == 0) for (int i = 1; i < Constants.NumTileFields; i++) tile.SetInt(i, 0);
        }

        public static void RebuildT3(LevelFile level)
        {
            var puddles = Enumerable.Range(0, Constants.TileCount).Where(i => level.TileByRawIndex(i).GetInt(0) == 10).ToList();
            level.SetTeleporters(puddles);
        }

        private static void EnsureActorDestination(LevelFile level, int objectIndex, int cx, int cy)
        {
            if (!Helpers.GridCellToRaw(cx, cy, out int rx, out int ry))
                throw new InvalidOperationException("Invalid actor cell.");
            TileRecord target = level.Tile(rx, ry);
            if (level.ObjectReferencesFromTiles(objectIndex).Contains(ry * Constants.TileWidth + rx)) return;
            if (target.GetInts().Any(v => v != 0))
                throw new InvalidOperationException("Actor destination is occupied. Clear it or choose another cell.");
        }

        public static int ReorderStatic(LevelFile level, int index, bool later)
        {
            if (level.Object(index).GetInt(12) != 1) return index;
            ObjectRecord obj = level.Object(index);
            double current = DrawOrder.Depth(obj);
            var depths = Enumerable.Range(0, Constants.ObjectCount)
                .Where(i => i != index && level.Object(i).GetInt(12) != -1 && level.Object(i).GetInt(5) == 0)
                .Select(i => DrawOrder.Depth(level.Object(i)));
            double target = later ? depths.Where(d => d >= current).DefaultIfEmpty(current).Min() + 1
                                  : depths.Where(d => d <= current).DefaultIfEmpty(current).Max() - 1;
            double offset = obj.GetFloat(2) + obj.GetInt(7) - target;
            obj.SetInt(24, checked((int)(later ? Math.Floor(offset) : Math.Ceiling(offset))));
            return index;
        }

        private static void WriteLinkedActorTile(LevelFile level, int objectIndex, int kind, int cx, int cy)
        {
            if (!Helpers.GridCellToRaw(cx, cy, out int rawX, out int rawY))
                throw new InvalidOperationException("Invalid character cell.");

            level.RemoveTileReferencesToObject(objectIndex);
            int rawIndex = rawY * Constants.TileWidth + rawX;
            TileRecord tile = level.Tile(rawX, rawY);
            tile.SetInt(0, kind switch { 11 => 7, 12 => 8, 13 => 9, >= 7 and <= 10 => 0, _ => kind });
            tile.SetInt(1, 0);
            tile.SetInt(2, kind is >= 7 and <= 10 ? 0 : objectIndex);
            tile.SetInt(3, kind switch { 7 => 130, 8 => 131, 9 => 129, 10 => 128, _ => 0 });
            tile.SetInt(4, kind is >= 7 and <= 10 ? objectIndex : 0);
            level.Object(objectIndex).SetInt(21, rawIndex);
        }

        private static void SetCharacterPosition(ObjectRecord obj, int kind, int cx, int cy)
        {
            (float x, float y) = CharacterPosition(kind, cx, cy);
            obj.SetFloat(1, x);
            obj.SetFloat(2, y);
            obj.SetFloat(3, x);
            obj.SetFloat(4, y);
        }

        private static (float X, float Y) CharacterPosition(int kind, int cx, int cy)
        {
            return kind switch
            {
                5 => (cx * Constants.CellWidth - 60, cy * Constants.CellHeight - 100),
                6 => (cx * Constants.CellWidth - 40, cy * Constants.CellHeight - 60),
                >= 7 and <= 10 => (cx * Constants.CellWidth - 20, cy * Constants.CellHeight - 20),
                _ => (cx * Constants.CellWidth - 40, cy * Constants.CellHeight - 40),
            };
        }

        private static (int CX, int CY) CharacterCellFromPosition(int kind, float x, float y)
        {
            (float ox, float oy) = kind switch
            {
                5 => (60, 100),
                6 => (40, 60),
                >= 7 and <= 10 => (20, 20),
                _ => (40, 40),
            };
            int cx = (int)Math.Round((x + ox) / Constants.CellWidth);
            int cy = (int)Math.Round((y + oy) / Constants.CellHeight);
            cx = Math.Clamp(cx, 0, Constants.VisibleTileWidth - 1);
            cy = Math.Clamp(cy, 0, Constants.TileHeight - 1);
            return (cx, cy);
        }
    }
}
