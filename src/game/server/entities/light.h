/* copyright (c) 2007 magnus auvinen, see licence.txt for more info */

#ifndef GAME_SERVER_CEntity_CLight_H
#define GAME_SERVER_CEntity_CLight_H

#include <game/server/entity.h>

class CLight : public CEntity
{
	float m_Rotation;
	vec2 m_To;
	vec2 m_Core;

	int m_EvalTick;
	
	int m_Tick;

	bool HitCharacter();
	void Move();
	void Step();
public:
	int m_CurveLength;
	int m_LengthL;
	int m_Speed;
	int m_Length;
	int m_RotationSpeed;

	CLight(CGameWorld *pGameWorld, vec2 Pos, float Rotation, int Length);
	
	virtual void Reset();
	virtual void Tick();
	virtual void Snap(int SnappingClient);
};

#endif
