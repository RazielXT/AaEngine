#include "ObjectId.h"

/*
 * Object ID Bit Layout (32-bit unsigned integer)
 *
 * There are two types of entity IDs: Normal and Custom.
 *
 * Scene Entity:
 * 31    30       24        23             0
 * +----+---------+------------------------+
 * |  1 | Order Group  | Local Index       |
 * +----+---------+------------------------+
 *
 *  - Bit 31      : Custom Entity Flag (1 for Normal Entity)
 *  - Bits 30-24  : Order Group (7 bits, 0x7F) - max size 128
 *  - Bits 23-0   : Local Index (24 bits, 0xFFFFFF)
 *
 *
 * Group Entity:
 * 31       30       27         26          16               15             0
 * +--------+---------+---------+----------------------------+---------------+
 * |   0    | Group Type | Group Index    | Entity Index    |
 * +--------+---------+---------+----------------------------+---------------+
 *
 *  - Bit 31      : Custom Entity Flag (0 for Custom Entity)
 *  - Bits 30-27  : Group Type (4 bits, 0xF) - max size 8
 *  - Bits 26-16  : Group Index (11 bits, 0x7FF) - max size 4096
 *  - Bits 15-0   : Entity Index (16 bits, 0xFFFF) - max size 65536
 */

constexpr UINT SceneEntityFlag = 0x80000000;

ObjectId::ObjectId(UINT idx, Order order)
{
	value = SceneEntityFlag | idx | (UINT(order) << 24);
}

ObjectId::ObjectId(UINT idx, ObjectType type, UINT groupId)
{
	value = (UINT(type) << 27) | (groupId << 16) | idx;
}

ObjectId::ObjectId(UINT v) : value(v)
{
}

Order ObjectId::getOrder() const
{
	if (value & SceneEntityFlag)
		return Order((value >> 24) & 0x7F);

	return Order::Normal;
}

UINT ObjectId::getLocalIdx() const
{
	if (value & SceneEntityFlag)
		return value & 0xFFFFFF;

	return value & 0xFFFF;
}

UINT ObjectId::getGroupId() const
{
	return (value >> 16) & 0x7FF;
}

ObjectType ObjectId::getObjectType() const
{
	if (value & SceneEntityFlag)
		return ObjectType::Entity;

	return ObjectType(value >> 27);
}
