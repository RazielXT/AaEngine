#include "ObjectId.h"

/*
 * Object ID Bit Layout (32-bit unsigned integer)
 *
 * There are two types of entity IDs: Scene and Custom.
 *
 * Scene Entity:
 *  - Bit 31      : Custom Entity Flag (1)
 *  - Bits 30-28  : Order (3 bits, 0x7) - 0-7
 *  - Bits 27-16  : Group Index (12 bits, 0xFFF) - max size 4096
 *  - Bits 15-0   : Entity Index (16 bits, 0xFFFF) - max size 65536
 *
 *
 * Custom Entity:
 *  - Bit 31      : Custom Entity Flag (0 for Custom Entity)
 *  - Bits 30-28  : Group Type (3 bits, 0x7) - 1-7 (reserved 0)
 *  - Bits 27-16  : Group Index (12 bits, 0xFFF) - max size 4096
 *  - Bits 15-0   : Entity Index (16 bits, 0xFFFF) - max size 65536
 */

constexpr UINT SceneEntityFlag = 0x80000000;

ObjectId::ObjectId(UINT idx, Order order, UINT groupId)
{
	value = SceneEntityFlag | (UINT(order) << 28) | (groupId << 16) | idx;
}

ObjectId::ObjectId(UINT idx, ObjectType type, UINT groupId)
{
	value = (UINT(type) << 28) | (groupId << 16) | idx;
}

ObjectId::ObjectId(UINT v) : value(v)
{
}

Order ObjectId::getOrder() const
{
	if (value & SceneEntityFlag)
		return Order((value >> 28) & 0x7);

	return Order::Normal;
}

ObjectType ObjectId::getObjectType() const
{
	if (value & SceneEntityFlag)
		return ObjectType::Entity;

	return ObjectType(value >> 28);
}

uint16_t ObjectId::getGroupId() const
{
	return uint16_t(value >> 16) & 0xFFF;
}

UINT ObjectId::getGroupMask() const
{
	return value & 0xFFF0000;
}

UINT ObjectId::getLocalIdx() const
{
	return value & 0xFFFF;
}
