#pragma once
#include "Types.h"
#include "Engine/Structures.h"
#include "Scene/ShagAnimation.h"
#include "BaseCharacter.h"

class EnemyCharacter : public BaseCharacter
{
	using Object = WorldObject;

public:
	EnemyCharacter(LevelScene& scene, size_t index);
	void Initialize() override;
	void Update(f32 seconds) override;
	EnemyCharacter(const EnemyCharacter&) = delete;
	EnemyCharacter& operator=(const EnemyCharacter&) = delete;

protected:
	void EnemyIdle(Object& actor, bool retry = false);
	void EnemyNotice(const Object& actor, bool item);
	i32 FindNoticePlayer(bool walking) const;
	bool ChooseEnemyStep();
	void DrawEnemy(Object& actor);
	void EnemyArrived();
	void UpdateEnemy(f32 seconds);
	void BeginBattle();
	void FinishBattle(Object& actor);
};
