#ifdef REGISTER_WEAPON

REGISTER_WEAPON(WEAPON_TYPE_PISTOL, CPistol)

#else

#ifndef GAME_SERVER_WEAPONS_H
#define GAME_SERVER_WEAPONS_H

#include "weapons/pistol.h"

enum
{
	WEAPON_TYPE_NONE = 0,
#define REGISTER_WEAPON(TYPE, CLASS) \
	TYPE,
#include <game/server/weapons.h>
#undef REGISTER_WEAPON
};

#endif // GAME_SERVER_WEAPONS_H

#endif // REGISTER_WEAPON