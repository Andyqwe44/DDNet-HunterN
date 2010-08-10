#ifndef GAME_COLLISION_H
#define GAME_COLLISION_H

#include <base/vmath.h>

class CCollision
{
	class CTile *m_pTiles;
	int m_Width;
	int m_Height;
	class CLayers *m_pLayers;

public:
	enum
	{
		COLFLAG_SOLID=1,
		COLFLAG_DEATH=2,
		COLFLAG_NOHOOK=4,
		COLFLAG_NOLASER=8,
		COLFLAG_THROUGH=16
	};
	CCollision();
	void Init(class CLayers *pLayers);
	bool CheckPoint(float x, float y) { return IsSolid(round(x), round(y)); }
	bool CheckPoint(vec2 p) { return CheckPoint(p.x, p.y); }
	void SetCollisionAt(float x, float y, int flag);
	int GetCollisionAt(float x, float y) { return GetTile(round(x), round(y)); }
	int GetWidth() { return m_Width; };
	int GetHeight() { return m_Height; };
	int IntersectLine(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision);
	int IntersectNoLaser(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision);
	int IntersectAir(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision);
	void MovePoint(vec2 *pInoutPos, vec2 *pInoutVel, float Elasticity, int *Bpounces);
	void MoveBox(vec2 *pInoutPos, vec2 *pInoutVel, vec2 Size, float Elasticity);
	bool TestBox(vec2 Pos, vec2 Size);
	int GetTile(int x, int y);
	//DDRace
	int GetIndex(vec2 PrevPos, vec2 Pos);
	vec2 GetPos(int Index);
	int GetCollisionDDRace(int Index);
	int IsTeleport(int x, int y);
	int IsCheckpoint(int Index);
	bool IsSpeedup(int x, int y);
	void GetSpeedup(int x, int y, vec2 *Dir, int *Force);
	
	int IsSolid(int x, int y);
	int IsBegin(int Index);
	int IsNoLaser(int x, int y);
	int IsEnd(int Index);
	int IsBoost(int Index);
	int IsFreeze(int Index);
	int IsUnfreeze(int Index);
	int IsCp(int x, int y);

	vec2 BoostAccelerator(int Index);
	vec2 CpSpeed(int index);
	
	class CTeleTile *m_pTele;
	class CSpeedupTile *m_pSpeedup;
	class CFrontTile *m_pFront;
	class CLayers *Layers() { return m_pLayers; }
};

#endif
