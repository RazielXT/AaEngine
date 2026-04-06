#pragma once

#include <cstdint>
#include <intsafe.h>

enum class Order
{
	Normal = 5,
	Post = 6,
	Transparent = 10,
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
	ObjectId(UINT idx, Order order);
	ObjectId(UINT idx, ObjectType, UINT groupId);

	Order getOrder() const;
	UINT getLocalIdx() const;
	UINT getGroupId() const;
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
