#pragma once

#include <cstdint>
#include <intsafe.h>

enum class Order : uint8_t
{
	Normal = 2,
	Post = 3,
	Transparent = 5,
};

enum class ObjectType
{
	Entity = -1,
	Empty = 0,
	Instanced,
	Grass,
};

struct ObjectId
{
	ObjectId() = default;
	ObjectId(UINT);
	ObjectId(UINT idx, Order order, UINT groupId);
	ObjectId(UINT idx, ObjectType, UINT groupId);

	Order getOrder() const;
	UINT getLocalIdx() const;
	uint16_t getGroupId() const;
	UINT getGroupMask() const;
	ObjectType getObjectType() const;

	UINT value{};

	operator bool() const
	{
		return value != 0;
	}
	bool operator==(const ObjectId& rhs) const
	{
		return value == rhs.value;
	}
};
