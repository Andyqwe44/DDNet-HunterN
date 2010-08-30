#include "teams.h"


CGameTeams::CGameTeams(CGameContext *pGameContext) : m_pGameContext(pGameContext) {
	for(int i = 0; i < MAX_CLIENTS; ++i) {
		m_TeamState[i] = EMPTY;
		m_TeeFinished[i] = false;
	}
}

void CGameTeams::OnCharacterStart(int id) {
	int Tick = Server()->Tick();
	if(m_Core.Team(id) == 0) {
		CCharacter* Char = Character(id);
		Char->m_RaceState = RACE_STARTED;
		Char->m_StartTime = Tick;
		Char->m_RefreshTime = Tick;
	} else {
		if(m_TeamState[m_Core.Team(id)] <= CLOSED) {
			ChangeTeamState(m_Core.Team(id), STARTED);
			
			for(int i = 0; i < MAX_CLIENTS; ++i) {
				if(m_Core.SameTeam(i, id)) {
					CCharacter* Char = Character(i);

					Char->m_RaceState = RACE_STARTED;
					Char->m_StartTime = Tick;
					Char->m_RefreshTime = Tick;
				}
			}
		}
	}
}

void CGameTeams::OnCharacterFinish(int id) {
	if(m_Core.Team(id) == 0) {
		Character(id)->OnFinish();
	} else {
		m_TeeFinished[id] = true;
		if(TeamFinished(m_Core.Team(id))) {
			ChangeTeamState(m_Core.Team(id), FINISHED);//TODO: Make it better
			for(int i = 0; i < MAX_CLIENTS; ++i) {
				if(m_Core.SameTeam(i, id)) {
					CCharacter * Char = Character(i);
					if(Char != 0) {
						Char->OnFinish();
						m_TeeFinished[i] = false;
					} //else {
					//	m_Core.Team(id) = 0; //i saw zomby =)
					//}
				}
			}
			
		}
	}
}

bool CGameTeams::SetCharacterTeam(int id, int Team) {
	//TODO: Send error message 
	if(id < 0 || id >= MAX_CLIENTS || Team < 0 || Team >= MAX_CLIENTS) {
		return false;
	}
	if(m_TeamState[Team] >= CLOSED) {
		return false;
	}
	if(m_Core.Team(id) != 0 && m_TeamState[m_Core.Team(id)] != EMPTY) {
		bool NoOneInOldTeam = true;
		for(int i = 0; i < MAX_CLIENTS; ++i) {
			if(m_Core.SameTeam(i, id)) {
				NoOneInOldTeam = false;//all good exists someone in old team
				break;
			} 
		}
		if(NoOneInOldTeam) {
			m_TeamState[m_Core.Team(id)] = EMPTY;
		}
	}
	m_Core.Team(id, Team);
	if(m_TeamState[Team] == EMPTY) {
		ChangeTeamState(Team, OPEN);
	}
	return true;
}

void CGameTeams::ChangeTeamState(int Team, int State) {
	m_TeamState[Team] = State;
}



bool CGameTeams::TeamFinished(int Team) {
	for(int i = 0; i < MAX_CLIENTS; ++i) {
		if(m_Core.Team(i) == Team && !m_TeeFinished[i]) {
			return false;
		}
	}
	return true;
}