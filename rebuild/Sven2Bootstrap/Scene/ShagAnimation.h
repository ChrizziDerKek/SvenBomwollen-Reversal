#pragma once
#include "Types.h"
#include "Engine/MudGE.h"

struct ShagAnimation
{
	const Sprite* Sprite = nullptr;
	std::vector<i32> Frames;
	str Sound;
	i32 Facing = 0;
	bool Mirror = false;

	inline static std::vector<ShagAnimation> Load(MudGE& engine)
	{
		std::vector<ShagAnimation> animations;
		animations.push_back({engine.LoadSprite("Sven2/gfx/OF/shag02.bmp", 5, 1), {0, 0, 1, 2, 2, 2, 3, 4}, "sven2/sfx/rammel2.ogg", 0, false}); // 0x440edc
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 3, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/OF/shag03.bmp", 5, 1), {0, 0, 1, 2, 2, 2, 3, 4}, "sven2/sfx/rammel10.ogg", 3, false}); // 0x440f3c
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 0, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/OF/shag04.bmp", 7, 1), {0, 0, 1, 2, 3, 4, 5, 6}, "sven2/sfx/rammel4.ogg", 2, false}); // 0x440f9c
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 1, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/OF/shag05.bmp", 5, 1), {0, 0, 1, 2, 2, 2, 3, 4}, "sven2/sfx/rammel5.ogg", 2, false}); // 0x440ffc
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 1, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/OF/shag06.bmp", 5, 1), {0, 0, 1, 2, 2, 2, 3, 4}, "sven2/sfx/rammel12.ogg", 0, false}); // 0x44105c
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 3, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/OF/shag07.bmp", 7, 1), {0, 0, 1, 2, 3, 4, 4, 5, 6}, "sven2/sfx/rammel2.ogg", 0, false}); // 0x4410bc
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 3, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/OF/shag08.bmp", 5, 1), {0, 0, 1, 2, 2, 2, 3, 4}, "sven2/sfx/rammel8.ogg", 2, false}); // 0x441120
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 1, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/OF/shag09.bmp", 5, 1), {0, 0, 1, 2, 2, 2, 3, 4}, "sven2/sfx/rammel9.ogg", 1, false}); // 0x441180
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 2, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/OF/shag10.bmp", 7, 1), {0, 0, 1, 2, 3, 3, 4, 5, 6}, "sven2/sfx/shag_whip.ogg", 1, false}); // 0x4411e0
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 2, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/OF/shag11.bmp", 7, 1), {0, 0, 1, 2, 3, 4, 5, 6}, "sven2/sfx/rammel11.ogg", 1, false}); // 0x441244
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 2, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/OF/shag12.bmp", 8, 1), {0, 0, 1, 2, 3, 3, 4, 5, 6, 7}, "sven2/sfx/rammel12.ogg", 0, false}); // 0x4412a4
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 3, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/OF/shag13.bmp", 6, 1), {0, 0, 1, 2, 3, 4, 4, 4, 4, 5}, "sven2/sfx/rammel13.ogg", 2, false}); // 0x44130c
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 1, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/OF/shag14.bmp", 5, 1), {0, 0, 0, 1, 2, 2, 2, 2, 3, 4}, "sven2/sfx/rammel14.ogg", 3, false}); // 0x441374
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 0, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/NF/NF01.bmp", 7, 1), {0, 1, 2, 3, 3, 4, 4, 5, 6, 3}, "sven2/sfx/nf01.ogg", 2, false}); // 0x4406d8
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 1, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/NF/NF02.bmp", 7, 1), {0, 0, 0, 1, 2, 3, 3, 4, 5, 6}, "sven2/sfx/nf02.ogg", 0, false}); // 0x440740
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 3, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/NF/NF03.bmp", 8, 1), {0, 0, 1, 2, 3, 4, 5, 5, 6, 7}, "sven2/sfx/nf03.ogg", 0, false}); // 0x4407a8
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 3, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/NF/NF04.bmp", 5, 1), {0, 0, 0, 0, 1, 2, 3, 3, 4}, "sven2/sfx/nf04.ogg", 2, false}); // 0x440810
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 1, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/NF/NF05.bmp", 7, 1), {0, 0, 1, 2, 3, 4, 4, 4, 5, 6}, "sven2/sfx/nf05.ogg", 3, false}); // 0x440874
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 0, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/NF/NF06.bmp", 6, 1), {0, 0, 0, 0, 1, 2, 2, 3, 4, 5}, "sven2/sfx/nf06.ogg", 3, false}); // 0x4408dc
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 0, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/NF/NF07.bmp", 5, 1), {0, 0, 0, 1, 2, 3, 3, 3, 4}, "sven2/sfx/nf07.ogg", 3, false}); // 0x440944
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 0, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/NF/NF08.bmp", 8, 1), {0, 0, 1, 2, 3, 4, 5, 5, 6, 7}, "sven2/sfx/nf08.ogg", 1, false}); // 0x4409a8
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 2, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/NF/NF09.bmp", 5, 1), {0, 0, 0, 0, 1, 2, 3, 3, 3, 4}, "sven2/sfx/nf09.ogg", 2, false}); // 0x440a10
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 1, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/NF/NF10.bmp", 5, 1), {0, 0, 0, 0, 1, 2, 3, 3, 3, 4}, "sven2/sfx/nf10.ogg", 2, false}); // 0x440a78
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 1, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/NF/NF11.bmp", 8, 1), {0, 0, 1, 2, 3, 4, 5, 5, 6, 7}, "sven2/sfx/nf11.ogg", 3, false}); // 0x440ae0
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 0, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/NF/NF12.bmp", 8, 1), {0, 0, 1, 2, 3, 4, 5, 6, 6, 7}, "sven2/sfx/nf12.ogg", 1, false}); // 0x440b48
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 2, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/NF/NF13.bmp", 5, 1), {0, 0, 0, 0, 1, 2, 3, 3, 3, 4}, "sven2/sfx/nf13.ogg", 1, false}); // 0x440bb0
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 2, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/NF/NF14.bmp", 5, 1), {0, 1, 2, 2, 3, 4, 3, 2, 2, 1}, "sven2/sfx/nf14.ogg", 1, false}); // 0x440c18
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 2, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/NF/NF15.bmp", 5, 1), {0, 0, 0, 1, 2, 2, 2, 3}, "sven2/sfx/nf15.ogg", 2, false}); // 0x440c80
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 1, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/NF/NF16.bmp", 8, 1), {0, 0, 1, 2, 3, 4, 5, 5, 6, 7}, "sven2/sfx/nf16.ogg", 2, false}); // 0x440ce0
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 1, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/NF/NF17.bmp", 5, 1), {0, 0, 0, 0, 1, 2, 3, 3, 3, 4}, "sven2/sfx/nf17.ogg", 0, false}); // 0x440d48
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 3, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/NF/NF18.bmp", 5, 1), {0, 0, 0, 0, 1, 2, 3, 3, 3, 4}, "sven2/sfx/nf18.ogg", 0, false}); // 0x440db0
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 3, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/NF/NF19.bmp", 7, 1), {0, 0, 1, 2, 3, 4, 4, 5, 6}, "sven2/sfx/nf19.ogg", 3, false}); // 0x440e18
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 0, true});
		animations.push_back({engine.LoadSprite("Sven2/gfx/NF/NF20.bmp", 8, 1), {0, 1, 2, 3, 4, 5, 6, 7}, "sven2/sfx/nf20.ogg", 3, false}); // 0x440e7c
		animations.push_back({animations.back().Sprite, animations.back().Frames, animations.back().Sound, 0, true});
		const size_t variants = static_cast<size_t>(std::clamp(engine.GetData().NumericParam("ShagAnims", 33), 1.0, 33.0)) * 2;
		animations.resize(variants);
		return animations;
	}
};