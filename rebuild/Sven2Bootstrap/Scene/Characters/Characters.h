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
