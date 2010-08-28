#ifndef GAME_SERVER_TEAMS_H
#define GAME_SERVER_TEAMS_H

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>

class CTeams {
	int m_Team[MAX_CLIENTS];
	int m_TeamState[MAX_CLIENTS];
	bool m_TeeFinished[MAX_CLIENTS];
	
	class CGameContext * m_pGameContext;
	
public:
	enum {
		EMPTY, 
		OPEN,
		CLOSED,
		STARTED,
		FINISHED
	};
	
	CTeams(CGameContext *pGameContext);
	
	//helper methods
	CCharacter* Character(int id) { return GameServer()->GetPlayerChar(id); }	

	class CGameContext *GameServer() { return m_pGameContext; }
	class IServer *Server() { return m_pGameContext->Server(); }
	
	void OnCharacterStart(int id);
	void OnCharacterFinish(int id);
	
	bool SetCharacterTeam(int id, int Team);
	
	void ChangeTeamState(int Team, int State);
	
	bool TeamFinished(int Team);
	
	bool SameTeam(int Cid1, int Cid2);
	
	int GetTeam(int Cid) {
		return m_Team[Cid];
	}
};

#endif