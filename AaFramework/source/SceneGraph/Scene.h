#pragma once

#include "ComponentStorage.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <typeindex>

class Scene
{
public:

	Scene() = default;

	~Scene() = default;


	// --------------------------------------------------------
	// Entity Lifetime
	// --------------------------------------------------------

	Entity CreateEntity()
	{
		Entity entity;

		if (!freeList.empty())
		{
			entity.index = freeList.back();
			freeList.pop_back();

			entity.generation =	entitySlots[entity.index].generation;
		}
		else
		{
			entity.index =
				static_cast<uint32_t>(entitySlots.size());

			entity.generation = 1;

			entitySlots.push_back({});
		}

		entitySlots[entity.index].alive = true;

		return entity;
	}

	void DestroyEntity(Entity entity)
	{
		if (!IsAlive(entity))
			return;

		// remove from all component storages

		for (auto& [type, storage] : storages)
		{
			auto& bus = buses[type];
			storage->Remove(entity, bus.get());
		}

		entitySlots[entity.index].alive = false;
		entitySlots[entity.index].generation++;

		freeList.push_back(entity.index);
	}

	bool IsAlive(Entity entity) const
	{
		if (entity.index >= entitySlots.size())
			return false;

		const EntitySlot& slot =
			entitySlots[entity.index];

		return
			slot.alive &&
			slot.generation == entity.generation;
	}

	// --------------------------------------------------------
	// Component events registration
	// --------------------------------------------------------

	template<typename T>
	ComponentEventBus<T>& Events()
	{
		auto type = std::type_index(typeid(T));

		if (!buses.contains(type))
		{
			buses[type] = std::make_unique<BusHolder<T>>();
		}

		return static_cast<BusHolder<T>*>(buses[type].get())->bus;
	}

	// --------------------------------------------------------
	// Components
	// --------------------------------------------------------

	template<typename T>
	T& AddComponent(Entity entity, const T& value = {})
	{
		auto& storage = GetStorage<T>();
		auto& bus = Events<T>();

		return storage.Add(entity, value, bus);
	}

	template<typename T>
	void RemoveComponent(Entity entity)
	{
		auto& storage = GetStorage<T>();
		auto& bus = Events<T>();

		storage.Remove(entity, bus);
	}

    template<typename T>
    T* GetComponent(Entity e)
    {
        return GetStorage<T>().TryGet(e);
    }

	template<typename T>
	ComponentStorage<T>& GetStorage()
	{
		const std::type_index type =
			std::type_index(typeid(T));

		auto it = storages.find(type);

		if (it != storages.end())
		{
			return *static_cast<ComponentStorage<T>*>(
				it->second.get());
		}

		auto storage =
			std::make_unique<ComponentStorage<T>>();

		ComponentStorage<T>* ptr = storage.get();

		storages[type] = std::move(storage);

		return *ptr;
	}

private:

	struct EntitySlot
	{
		Generation generation = 1;
		bool alive = false;
	};

	std::vector<EntitySlot> entitySlots;

	std::vector<EntityIndex> freeList;

	std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>> storages;

	std::unordered_map<std::type_index,	std::unique_ptr<IBusHolder>> buses;
};
