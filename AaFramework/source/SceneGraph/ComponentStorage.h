#pragma once

#include <vector>
#include <functional>
#include <span>

// ============================================================
// Entity
// ============================================================

using EntityIndex = uint32_t;
using Generation = uint32_t;

struct Entity
{
	uint32_t index = UINT32_MAX;
	Generation generation = 0;

	bool operator==(const Entity&) const = default;
};

static constexpr uint32_t INVALID_INDEX = UINT32_MAX;

template<typename T>
struct ComponentEventBus
{
	std::function<void(Entity, T&)> onAdded;
	std::function<void(Entity, T&)> onRemoved;
	std::function<void(Entity, T&)> onChanged;

	void EmitAdded(Entity e, T& c)
	{
		if (onAdded)
			onAdded(e, c);
	}

	void EmitRemoved(Entity e, T& c)
	{
		if (onRemoved)
			onRemoved(e, c);
	}

	void EmitChanged(Entity e, T& c)
	{
		if (onChanged)
			onChanged(e, c);
	}
};

struct IBusHolder
{
	virtual ~IBusHolder() = default;
};

template<typename T>
struct BusHolder : IBusHolder
{
	ComponentEventBus<T> bus;
};

class IComponentStorage
{
public:

	virtual ~IComponentStorage() = default;

	virtual void Remove(Entity entity, IBusHolder* bus) = 0;
};

template<typename T>
class ComponentStorage : public IComponentStorage
{
public:

	static constexpr uint32_t INVALID_INDEX = UINT32_MAX;

public:

	template<typename... Args>
	T& Emplace(Entity entity, ComponentEventBus<T>& bus, Args&&... args)
	{
		EnsureSparseSize(entity.index);

		// Optional overwrite behavior
		if (Has(entity))
		{
			T& existing = components[sparse[entity.index]];
			existing = T(std::forward<Args>(args)...);
			return existing;
		}

		const uint32_t denseIndex = static_cast<uint32_t>(components.size());

		sparse[entity.index] = denseIndex;

		entities.push_back(entity);

		components.emplace_back(std::forward<Args>(args)...);

		bus.EmitAdded(entity, components.back());

		return components.back();
	}

	T& Add(Entity entity, const T& value, ComponentEventBus<T>& bus)
	{
		return Emplace(entity, bus, value);
	}

	void Remove(Entity entity, ComponentEventBus<T>& bus)
	{
		if (!Has(entity))
			return;

		const uint32_t index = sparse[entity.index];
		const uint32_t lastIndex = static_cast<uint32_t>(components.size() - 1);

		bus.EmitRemoved(entity, components[index]);

		// Swap-remove if not removing last
		if (index != lastIndex)
		{
			components[index] = std::move(components[lastIndex]);
			entities[index] = entities[lastIndex];

			// Repair sparse mapping for moved entity
			sparse[entities[index].index] = index;
		}

		components.pop_back();
		entities.pop_back();

		sparse[entity.index] = INVALID_INDEX;
	}

	void Remove(Entity entity, IBusHolder* bus) override
	{
		Remove(entity, static_cast<BusHolder<T>*>(bus)->bus);
	}

	bool Has(Entity entity) const
	{
		if (entity.index >= sparse.size())
			return false;

		const uint32_t denseIndex = sparse[entity.index];

		if (denseIndex == INVALID_INDEX)
			return false;

		if (denseIndex >= components.size())
			return false;

		return entities[denseIndex] == entity;
	}

	T& Get(Entity entity)
	{
		return components[sparse[entity.index]];
	}

	const T& Get(Entity entity) const
	{
		return components[sparse[entity.index]];
	}

	std::span<T> Components()
	{
		return components;
	}

	std::span<const T> Components() const
	{
		return components;
	}

	std::span<Entity> Entities()
	{
		return entities;
	}

	std::span<const Entity> Entities() const
	{
		return entities;
	}

	size_t Size() const
	{
		return components.size();
	}

	bool Empty() const
	{
		return components.empty();
	}

	void Clear()
	{
		components.clear();
		entities.clear();
		sparse.clear();
	}

private:

	void EnsureSparseSize(uint32_t index)
	{
		if (index < sparse.size())
			return;

		sparse.resize(index + 1, INVALID_INDEX);
	}

	std::vector<T> components;
	std::vector<Entity> entities;

	// entity.index -> dense index
	std::vector<uint32_t> sparse;
};
