#pragma once
#include "Types.h"
#include "BaseObject.h"
#include <memory>
#include <vector>

class PlayerCharacter;

// Owns objects, including their state. Slots are stable until explicitly reused.
class ObjectManager
{
public:
	explicit ObjectManager(LevelScene& scene);
	~ObjectManager();
	ObjectManager(const ObjectManager&) = delete;
	ObjectManager& operator=(const ObjectManager&) = delete;
	PlayerCharacter& GetPlayer(size_t number = 0);
	const PlayerCharacter& GetPlayer(size_t number = 0) const;
	size_t Size() const
	{
		return Objects.size();
	}
	void Reserve(size_t count)
	{
		Objects.reserve(count);
	}
	void Add(const WorldObject& state);
	void Replace(size_t index, const WorldObject& state);
	BaseObject& Get(size_t index)
	{
		return *Objects.at(index);
	}
	const BaseObject& Get(size_t index) const
	{
		return *Objects.at(index);
	}
	WorldObject& operator[](size_t index)
	{
		return Get(index).GetState();
	}
	const WorldObject& operator[](size_t index) const
	{
		return Get(index).GetState();
	}
	void UpdatePhase(ObjectPhase phase, f32 seconds);
	std::vector<WorldObject> CopyStates() const;
	std::vector<size_t> Find(ObjectKind kind, bool includeFreed = false) const;
	bool PlayerInRange(const WorldObject& actor, f32 distance) const;
	bool CanCatchPlayer(const WorldObject& actor) const;
	bool CanOccupy(i32 tile, size_t except) const;
	struct CombatState
	{
		i32 Enemy = -1;
		f32 Remaining = 0;
	};
	std::array<CombatState, 2> Combats;

	// Required C++ range-for protocol. Public game operations use PascalCase.
	class Iterator
	{
	public:
		Iterator(ObjectManager& owner, size_t index)
		    : Owner(owner), Index(index)
		{}
		WorldObject& operator*() const
		{
			return Owner[Index];
		}
		Iterator& operator++()
		{
			++Index;
			return *this;
		}
		bool operator!=(const Iterator& other) const
		{
			return Index != other.Index;
		}

	private:
		ObjectManager& Owner;
		size_t Index;
	};
	Iterator begin()
	{
		return Iterator(*this, 0);
	}
	Iterator end()
	{
		return Iterator(*this, Size());
	}

private:
	std::unique_ptr<BaseObject> Create(const WorldObject& state, size_t index);
	LevelScene& Scene;
	std::vector<std::unique_ptr<BaseObject>> Objects;
	std::array<std::unique_ptr<PlayerCharacter>, 2> PendingPlayers;
	std::array<PlayerCharacter*, 2> Players{};
	size_t AdoptedPlayers = 0;
};
