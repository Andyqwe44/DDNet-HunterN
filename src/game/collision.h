#ifndef GAME_COLLISION_H
#define GAME_COLLISION_H

#include <base/vmath.h>

class CCollision
{
	class CTile *m_pTiles;
	int m_Width;
	int m_Height;
	class CLayers *m_pLayers;
	class CTeleTile *m_pTele;
	class CSpeedupTile *m_pSpeedup;
	class CTile *m_pFront;
	class CTeleTile *m_pSwitch;

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
	int GetFCollisionAt(float x, float y) { return GetFTile(round(x), round(y)); }
	int GetWidth() { return m_Width; };
	int GetHeight() { return m_Height; };
	int IntersectLine(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision, bool AllowThrough);
	int IntersectNoLaser(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision);
	int IntersectNoLaserNW(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision);
	int IntersectAir(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision);
	void MovePoint(vec2 *pInoutPos, vec2 *pInoutVel, float Elasticity, int *Bpounces);
	void MoveBox(vec2 *pInoutPos, vec2 *pInoutVel, vec2 Size, float Elasticity);
	bool TestBox(vec2 Pos, vec2 Size);
	int GetTile(int x, int y);
	int GetFTile(int x, int y);
	int Entity(int x, int y, bool Front);
	//DDRace
	int GetMapIndex(vec2 PrevPos, vec2 Pos);
	vec2 GetPos(int Index);
	int GetCollisionDDRace(int Index);
	int GetFCollisionDDRace(int Index);
	int IsTeleport(int x, int y);
	//int IsCheckpoint(int Index);
	bool IsSpeedup(int x, int y);
	void GetSpeedup(int x, int y, vec2 *Dir, int *Force);
	
	int IsSolid(int x, int y);
	int IsThrough(int x, int y);
	int IsNoLaser(int x, int y);
	int IsFNoLaser(int x, int y);
	
	int IsCheckpoint(int Index);
	
	int IsCp(int x, int y);

	vec2 CpSpeed(int index);
	
	class CTeleTile *TeleLayer() { return m_pTele; }
	//class CSpeedupTile *SpeedupLayer() { return m_pSpeedup; }
	//class CTile *FrontLayer() { m_pFront; }
	class CTeleTile *SwitchLayer() { return m_pSwitch; }
	class CLayers *Layers() { return m_pLayers; }
};

#endif
