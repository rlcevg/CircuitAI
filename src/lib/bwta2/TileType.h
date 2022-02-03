#pragma once
#pragma pack(1)

#include <cstdint>

/** Represents information shared for all tiles in one tile group. */
class TileType
{
public:
	uint16_t index;
	uint8_t buildability; /**< 8th bit should sign not buildable. */
	uint8_t groundHeight; /**< Ground Height(4 lower bits) - Deprecated? Some values are incorrect. */
	uint16_t leftEdge;
	uint16_t topEdge;
	uint16_t rightEdge;
	uint16_t bottomEdge;
	uint16_t _1;
	uint16_t _2; /**<  Unknown - Edge piece has rows above it. (Recently noticed; not fully understood.)
				 o 1 = Basic edge piece.
				 o 2 = Right edge piece.
				 o 3 = Left edge piece.*/
	uint16_t _3;
	uint16_t _4;
	uint16_t miniTile[16]; /** MegaTile References (VF4/VX4) */
};
#pragma pack()
