#pragma once
#include "EnemyCharacter.h"
#include "AnimalCharacter.h"

class Sheep final : public AnimalCharacter
{
public:
	Sheep(LevelScene& scene, size_t index)
	    : AnimalCharacter(scene, index, ObjectPhase::Sheep)
	{}
	void Update(f32 seconds) override;
};

class Chicken final : public AnimalCharacter
{
public:
	using AnimalCharacter::AnimalCharacter;
};

class Goat final : public AnimalCharacter
{
public:
	using AnimalCharacter::AnimalCharacter;
};

class Pig final : public AnimalCharacter
{
public:
	using AnimalCharacter::AnimalCharacter;
};

class Penguin final : public AnimalCharacter
{
public:
	using AnimalCharacter::AnimalCharacter;
};

class Turtle final : public AnimalCharacter
{
public:
	using AnimalCharacter::AnimalCharacter;
};

class Rabbit final : public AnimalCharacter
{
public:
	using AnimalCharacter::AnimalCharacter;
};

class Alien final : public EnemyCharacter
{
public:
	using EnemyCharacter::EnemyCharacter;
};

class Dog final : public EnemyCharacter
{
public:
	using EnemyCharacter::EnemyCharacter;
};

class Farmer final : public EnemyCharacter
{
public:
	using EnemyCharacter::EnemyCharacter;
};

class Girl final : public EnemyCharacter
{
public:
	using EnemyCharacter::EnemyCharacter;
};

class Mermaid final : public BaseCharacter
{
public:
	Mermaid(LevelScene& scene, size_t index);
	void Initialize() override;
	void Update(f32 seconds) override;
	bool Reveal();
	bool BeginInteraction(size_t playerNumber, i32 facing);
	// Called once per completed player interaction; true means the repeat was accepted.
	bool FinishInteraction(bool repeat);
	void CancelInteraction(size_t playerNumber);
	MermaidState GetMermaidState() const;

private:
	void SetState(MermaidState state);
	bool Advance(cstr sequence, f32 rate, f32 seconds);
	void ClearCell();
	void Draw();
	i32 LinkedPlayer = -1;
	i32 CompletingPlayer = 0;
	f32 Cycles = 0;
};
