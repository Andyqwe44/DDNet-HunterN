#include <new>
#include <stdio.h>
#include <string.h>
#include <base/math.h>
#include <engine/shared/config.h>
#include <engine/server/server.h>
#include <engine/map.h>
#include <engine/console.h>
#include "gamecontext.h"
#include <game/version.h>
#include <game/collision.h>
#include <game/gamecore.h>
#include "gamemodes/DDRace.h"

#include "score.h"
#include "score/file_score.h"
#include "score/sql_score.h"

enum
{
	RESET,
	NO_RESET
};

void CGameContext::Construct(int Resetting)
{
	m_Resetting = 0;
	m_pServer = 0;

	for(int i = 0; i < MAX_CLIENTS; i++)
		m_apPlayers[i] = 0;

	m_pController = 0;
	m_VoteCloseTime = 0;
	m_pVoteOptionFirst = 0;
	m_pVoteOptionLast = 0;

	if(Resetting==NO_RESET)
		m_pVoteOptionHeap = new CHeap();
	m_pScore = 0;
}

CGameContext::CGameContext(int Resetting)
{
	Construct(Resetting);
}

CGameContext::CGameContext()
{
	Construct(NO_RESET);
}

CGameContext::~CGameContext()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		delete m_apPlayers[i];
	if(!m_Resetting)
		delete m_pVoteOptionHeap;
}

void CGameContext::Clear()
{
	CHeap *pVoteOptionHeap = m_pVoteOptionHeap;
	CVoteOption *pVoteOptionFirst = m_pVoteOptionFirst;
	CVoteOption *pVoteOptionLast = m_pVoteOptionLast;
	CTuningParams Tuning = m_Tuning;

	//bool cheats = m_Cheats;
	m_Resetting = true;
	this->~CGameContext();
	mem_zero(this, sizeof(*this));
	new (this) CGameContext(RESET);
	//this->m_Cheats = cheats;

	m_pVoteOptionHeap = pVoteOptionHeap;
	m_pVoteOptionFirst = pVoteOptionFirst;
	m_pVoteOptionLast = pVoteOptionLast;
	m_Tuning = Tuning;
}


class CCharacter *CGameContext::GetPlayerChar(int ClientId)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS || !m_apPlayers[ClientId])
		return 0;
	return m_apPlayers[ClientId]->GetCharacter();
}

void CGameContext::CreateDamageInd(vec2 p, float Angle, int Amount, int Mask)
{
	float a = 3 * 3.14159f / 2 + Angle;
	//float a = get_angle(dir);
	float s = a-pi/3;
	float e = a+pi/3;
	for(int i = 0; i < Amount; i++)
	{
		float f = mix(s, e, float(i+1)/float(Amount+2));
		NETEVENT_DAMAGEIND *ev = (NETEVENT_DAMAGEIND *)m_Events.Create(NETEVENTTYPE_DAMAGEIND, sizeof(NETEVENT_DAMAGEIND), Mask);
		if(ev)
		{
			ev->m_X = (int)p.x;
			ev->m_Y = (int)p.y;
			ev->m_Angle = (int)(f*256.0f);
		}
	}
}

void CGameContext::CreateHammerHit(vec2 p, int Mask)
{
	// create the event
	NETEVENT_HAMMERHIT *ev = (NETEVENT_HAMMERHIT *)m_Events.Create(NETEVENTTYPE_HAMMERHIT, sizeof(NETEVENT_HAMMERHIT), Mask);
	if(ev)
	{
		ev->m_X = (int)p.x;
		ev->m_Y = (int)p.y;
	}
}


void CGameContext::CreateExplosion(vec2 p, int Owner, int Weapon, bool NoDamage, int Mask)
{
	// create the event
	NETEVENT_EXPLOSION *ev = (NETEVENT_EXPLOSION *)m_Events.Create(NETEVENTTYPE_EXPLOSION, sizeof(NETEVENT_EXPLOSION), Mask);
	if(ev)
	{
		ev->m_X = (int)p.x;
		ev->m_Y = (int)p.y;
	}

	/*if (!NoDamage)
	{*/
		// deal damage
		CCharacter *apEnts[64];
		float Radius = 135.0f;
		float InnerRadius = 48.0f;
		int Num = m_World.FindEntities(p, Radius, (CEntity**)apEnts, 64, NETOBJTYPE_CHARACTER);
		for(int i = 0; i < Num; i++)
		{
			vec2 Diff = apEnts[i]->m_Pos - p;
			vec2 ForceDir(0,1);
			float l = length(Diff);
			if(l)
				ForceDir = normalize(Diff);
			l = 1-clamp((l-InnerRadius)/(Radius-InnerRadius), 0.0f, 1.0f);
			float Dmg = 6 * l;
			if((int)Dmg)
				if((g_Config.m_SvHit||NoDamage) || Owner == apEnts[i]->m_pPlayer->GetCID())
				{
					if(Owner != -1 && apEnts[i]->m_Alive && apEnts[i]->Team() != GetPlayerChar(Owner)->Team()) continue;
					apEnts[i]->TakeDamage(ForceDir*Dmg*2, (int)Dmg, Owner, Weapon);
					if(!g_Config.m_SvHit||NoDamage) break;
				}

		}
	//}
}

/*
void create_smoke(vec2 p)
{
	// create the event
	EV_EXPLOSION *ev = (EV_EXPLOSION *)events.create(EVENT_SMOKE, sizeof(EV_EXPLOSION));
	if(ev)
	{
		ev->x = (int)p.x;
		ev->y = (int)p.y;
	}
}*/

void CGameContext::CreatePlayerSpawn(vec2 p, int Mask)
{
	// create the event
	NETEVENT_SPAWN *ev = (NETEVENT_SPAWN *)m_Events.Create(NETEVENTTYPE_SPAWN, sizeof(NETEVENT_SPAWN), Mask);
	if(ev)
	{
		ev->m_X = (int)p.x;
		ev->m_Y = (int)p.y;
	}
}

void CGameContext::CreateDeath(vec2 p, int ClientId, int Mask)
{
	// create the event
	NETEVENT_DEATH *ev = (NETEVENT_DEATH *)m_Events.Create(NETEVENTTYPE_DEATH, sizeof(NETEVENT_DEATH), Mask);
	if(ev)
	{
		ev->m_X = (int)p.x;
		ev->m_Y = (int)p.y;
		ev->m_ClientId = ClientId;
	}
}

void CGameContext::CreateSound(vec2 Pos, int Sound, int Mask)
{
	if (Sound < 0)
		return;

	// create a sound
	NETEVENT_SOUNDWORLD *ev = (NETEVENT_SOUNDWORLD *)m_Events.Create(NETEVENTTYPE_SOUNDWORLD, sizeof(NETEVENT_SOUNDWORLD), Mask);
	if(ev)
	{
		ev->m_X = (int)Pos.x;
		ev->m_Y = (int)Pos.y;
		ev->m_SoundId = Sound;
	}
}

void CGameContext::CreateSoundGlobal(int Sound, int Target)
{
	if (Sound < 0)
		return;

	CNetMsg_Sv_SoundGlobal Msg;
	Msg.m_Soundid = Sound;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, Target);
}


void CGameContext::SendChatTarget(int To, const char *pText)
{
	CNetMsg_Sv_Chat Msg;
	Msg.m_Team = 0;
	Msg.m_Cid = -1;
	Msg.m_pMessage = pText;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, To);
}


void CGameContext::SendChat(int ChatterClientId, int Team, const char *pText)
{
	char aBuf[256];
	if(ChatterClientId >= 0 && ChatterClientId < MAX_CLIENTS)
		str_format(aBuf, sizeof(aBuf), "%d:%d:%s: %s", ChatterClientId, Team, Server()->ClientName(ChatterClientId), pText);
	else
		str_format(aBuf, sizeof(aBuf), "*** %s", pText);
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "chat", aBuf);

	if(Team == CHAT_ALL)
	{
		CNetMsg_Sv_Chat Msg;
		Msg.m_Team = 0;
		Msg.m_Cid = ChatterClientId;
		Msg.m_pMessage = pText;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
	}
	else
	{
		CNetMsg_Sv_Chat Msg;
		Msg.m_Team = 1;
		Msg.m_Cid = ChatterClientId;
		Msg.m_pMessage = pText;

		// pack one for the recording only
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NOSEND, -1);

		// send to the clients
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_apPlayers[i] && m_apPlayers[i]->GetTeam() == Team)
				Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NORECORD, i);
		}
	}
}

void CGameContext::SendEmoticon(int ClientId, int Emoticon)
{
	CNetMsg_Sv_Emoticon Msg;
	Msg.m_Cid = ClientId;
	Msg.m_Emoticon = Emoticon;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
}

void CGameContext::SendWeaponPickup(int ClientId, int Weapon)
{
	CNetMsg_Sv_WeaponPickup Msg;
	Msg.m_Weapon = Weapon;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientId);
}


void CGameContext::SendBroadcast(const char *pText, int ClientId)
{
	CNetMsg_Sv_Broadcast Msg;
	Msg.m_pMessage = pText;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientId);
}

//
void CGameContext::StartVote(const char *pDesc, const char *pCommand)
{
	// check if a vote is already running
	if(m_VoteCloseTime)
		return;

	// reset votes
	m_VoteEnforce = VOTE_ENFORCE_UNKNOWN;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i])
		{
			m_apPlayers[i]->m_Vote = 0;
			m_apPlayers[i]->m_VotePos = 0;
		}
	}

	// start vote
	m_VoteCloseTime = time_get() + time_freq()*25;
	str_copy(m_aVoteDescription, pDesc, sizeof(m_aVoteDescription));
	str_copy(m_aVoteCommand, pCommand, sizeof(m_aVoteCommand));
	SendVoteSet(-1);
	m_VoteUpdate = true;
}


void CGameContext::EndVote()
{
	m_VoteCloseTime = 0;
	SendVoteSet(-1);
}

void CGameContext::SendVoteSet(int ClientId)
{
	CNetMsg_Sv_VoteSet Msg;
	if(m_VoteCloseTime)
	{
		Msg.m_Timeout = (m_VoteCloseTime-time_get())/time_freq();
		Msg.m_pDescription = m_aVoteDescription;
		Msg.m_pCommand = "";
	}
	else
	{
		Msg.m_Timeout = 0;
		Msg.m_pDescription = "";
		Msg.m_pCommand = "";
	}
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientId);
}

void CGameContext::SendVoteStatus(int ClientId, int Total, int Yes, int No)
{
	CNetMsg_Sv_VoteStatus Msg = {0};
	Msg.m_Total = Total;
	Msg.m_Yes = Yes;
	Msg.m_No = No;
	Msg.m_Pass = Total - (Yes+No);

	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientId);

}

void CGameContext::AbortVoteKickOnDisconnect(int ClientId)
{
	if(m_VoteCloseTime && !str_comp_num(m_aVoteCommand, "kick ", 5) && str_toint(&m_aVoteCommand[5]) == ClientId)
		m_VoteCloseTime = -1;
}


void CGameContext::CheckPureTuning()
{
	// might not be created yet during start up
	if(!m_pController)
		return;

	if(	str_comp(m_pController->m_pGameType, "DM")==0 ||
		str_comp(m_pController->m_pGameType, "TDM")==0 ||
		str_comp(m_pController->m_pGameType, "CTF")==0)
	{
		CTuningParams p;
		if(mem_comp(&p, &m_Tuning, sizeof(p)) != 0)
		{
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "resetting tuning due to pure server");
			m_Tuning = p;
		}
	}
}

void CGameContext::SendTuningParams(int Cid)
{
	CheckPureTuning();

	CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
	int *pParams = (int *)&m_Tuning;
	for(unsigned i = 0; i < sizeof(m_Tuning)/sizeof(int); i++)
		Msg.AddInt(pParams[i]);
	Server()->SendMsg(&Msg, MSGFLAG_VITAL, Cid);
}

void CGameContext::OnTick()
{
	// check tuning
	CheckPureTuning();

	// copy tuning
	m_World.m_Core.m_Tuning = m_Tuning;
	m_World.Tick();

	//if(world.paused) // make sure that the game object always updates
	m_pController->Tick();

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i])
			m_apPlayers[i]->Tick();
	}

	// update voting
	if(m_VoteCloseTime)
	{
		// abort the kick-vote on player-leave
		if(m_VoteCloseTime == -1)
		{
			SendChat(-1, CGameContext::CHAT_ALL, "Vote aborted");
			EndVote();
		}
		else
		{
			int Total = 0, Yes = 0, No = 0;
			if(m_VoteUpdate)
			{
				// count votes
				char aaBuf[MAX_CLIENTS][64] = {{0}};
				for(int i = 0; i < MAX_CLIENTS; i++)
					if(m_apPlayers[i])
						Server()->GetClientIP(i, aaBuf[i], 64);
				bool aVoteChecked[MAX_CLIENTS] = {0};
				for(int i = 0; i < MAX_CLIENTS; i++)
				{
					if(!m_apPlayers[i] || m_apPlayers[i]->GetTeam() == -1 || aVoteChecked[i])	// don't count in votes by spectators
						continue;
					if(m_VoteKick && 
						GetPlayerChar(m_VoteCreator) && 
						GetPlayerChar(m_VoteCreator)->Team() != GetPlayerChar(i)->Team()) continue;
					int ActVote = m_apPlayers[i]->m_Vote;
					int ActVotePos = m_apPlayers[i]->m_VotePos;

					// check for more players with the same ip (only use the vote of the one who voted first)
					for(int j = i+1; j < MAX_CLIENTS; ++j)
					{
						if(!m_apPlayers[j] || aVoteChecked[j] || str_comp(aaBuf[j], aaBuf[i]))
							continue;

						aVoteChecked[j] = true;
						if(m_apPlayers[j]->m_Vote && (!ActVote || ActVotePos > m_apPlayers[j]->m_VotePos))
						{
							ActVote = m_apPlayers[j]->m_Vote;
							ActVotePos = m_apPlayers[j]->m_VotePos;
						}
					}

					Total++;
					if(ActVote > 0)
						Yes++;
					else if(ActVote < 0)
						No++;
				}

				if(Yes >= Total/2+1)
					m_VoteEnforce = VOTE_ENFORCE_YES;
				else if(No >= Total/2+1 || Yes+No == Total)
					m_VoteEnforce = VOTE_ENFORCE_NO;
			}

			if(m_VoteEnforce == VOTE_ENFORCE_YES)
			{
				//Console()->ExecuteLine(m_aVoteCommand, 4, -1);
				//EndVote();
				//SendChat(-1, CGameContext::CHAT_ALL, "Vote passed");
				if(m_VoteEnforce == VOTE_ENFORCE_YES)
				{
					Console()->ExecuteLine(m_aVoteCommand, 3,-1);
					SendChat(-1, CGameContext::CHAT_ALL, "Vote passed (enforced by Admin)");
					dbg_msg("Vote","Due to vote enforcing, vote level has been set to 3");
					EndVote();
				}
				else
				{
					Console()->ExecuteLine(m_aVoteCommand, 4,-1);
					dbg_msg("Vote","vote level is set to 4");
					EndVote();
					SendChat(-1, CGameContext::CHAT_ALL, "Vote passed");
				}
				if(m_apPlayers[m_VoteCreator])
					m_apPlayers[m_VoteCreator]->m_Last_VoteCall = 0;
			}
			else if(m_VoteEnforce == VOTE_ENFORCE_NO || time_get() > m_VoteCloseTime)
			{
				EndVote();
				SendChat(-1, CGameContext::CHAT_ALL, "Vote failed");
			}
			else if(m_VoteUpdate)
			{
				m_VoteUpdate = false;
				SendVoteStatus(-1, Total, Yes, No);
			}
		}
	}


#ifdef CONF_DEBUG
	if(g_Config.m_DbgDummies)
	{
		for(int i = 0; i < g_Config.m_DbgDummies ; i++)
		{
			CNetObj_PlayerInput Input = {0};
			Input.m_Direction = (i&1)?-1:1;
			m_apPlayers[MAX_CLIENTS-i-1]->OnPredictedInput(&Input);
		}
	}
#endif
}

// Server hooks
void CGameContext::OnClientDirectInput(int ClientID, void *pInput)
{
	if(!m_World.m_Paused)
		m_apPlayers[ClientID]->OnDirectInput((CNetObj_PlayerInput *)pInput);
}

void CGameContext::OnClientPredictedInput(int ClientID, void *pInput)
{
	if(!m_World.m_Paused)
		m_apPlayers[ClientID]->OnPredictedInput((CNetObj_PlayerInput *)pInput);
}

void CGameContext::OnClientEnter(int ClientId)
{
	//world.insert_entity(&players[client_id]);
	m_apPlayers[ClientId]->Respawn();
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "%s entered and joined the %s", Server()->ClientName(ClientId), m_pController->GetTeamName(m_apPlayers[ClientId]->GetTeam()));
	SendChat(-1, CGameContext::CHAT_ALL, aBuf);
	SendChatTarget(ClientId, "DDRace Mod. Version: " DDRACE_VERSION);
	SendChatTarget(ClientId, "Official site: DDRace.info");
	SendChatTarget(ClientId, "For more Info /CMDList");
	SendChatTarget(ClientId, "Or visit DDRace.info");
	SendChatTarget(ClientId, "To see this again say /info");
	if (g_Config.m_SvWelcome[0]!=0) SendChatTarget(ClientId,g_Config.m_SvWelcome);
	str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' team=%d", ClientId, Server()->ClientName(ClientId), m_apPlayers[ClientId]->GetTeam());
	Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	m_VoteUpdate = true;
}

bool compare_players(CPlayer *pl1, CPlayer *pl2)
{
   if (pl1->m_Authed>pl2->m_Authed)
       return true;
   else
       return false;
}

void CGameContext::OnSetAuthed(int client_id, int Level)
{
	if(m_apPlayers[client_id])
	{
		m_apPlayers[client_id]->m_Authed = Level;
		char buf[11];
		str_format(buf, sizeof(buf), "ban %d %d", client_id, g_Config.m_SvVoteKickBanTime);
		if ( !strcmp(m_aVoteCommand,buf))
		{
			m_VoteEnforce = CGameContext::VOTE_ENFORCE_NO;
			dbg_msg("hooks","Aborting vote");
		}
	}
}

void CGameContext::OnSetResistent(int client_id, int Resistent)
{
   if(m_apPlayers[client_id])
       m_apPlayers[client_id]->m_Resistent = Resistent;
}

void CGameContext::OnClientConnected(int ClientId)
{
	// Check which team the player should be on
	const int StartTeam = g_Config.m_SvTournamentMode ? -1 : m_pController->GetAutoTeam(ClientId);

	m_apPlayers[ClientId] = new(ClientId) CPlayer(this, ClientId, StartTeam);
	//players[client_id].init(client_id);
	//players[client_id].client_id = client_id;

	//(void)m_pController->CheckTeamBalance();

#ifdef CONF_DEBUG
	if(g_Config.m_DbgDummies)
	{
		if(ClientId >= MAX_CLIENTS-g_Config.m_DbgDummies)
			return;
	}
#endif

	// send active vote
	if(m_VoteCloseTime)
		SendVoteSet(ClientId);

	// send motd
	CNetMsg_Sv_Motd Msg;
	Msg.m_pMessage = g_Config.m_SvMotd;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientId);
}

void CGameContext::OnClientDrop(int ClientId)
{
	AbortVoteKickOnDisconnect(ClientId);
	m_apPlayers[ClientId]->OnDisconnect();
	delete m_apPlayers[ClientId];
	m_apPlayers[ClientId] = 0;

	//(void)m_pController->CheckTeamBalance();
	m_VoteUpdate = true;
}

void CGameContext::OnMessage(int MsgId, CUnpacker *pUnpacker, int ClientId)
{
	void *pRawMsg = m_NetObjHandler.SecureUnpackMsg(MsgId, pUnpacker);
	CPlayer *p = m_apPlayers[ClientId];

	if(!pRawMsg)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "dropped weird message '%s' (%d), failed on '%s'", m_NetObjHandler.GetMsgName(MsgId), MsgId, m_NetObjHandler.FailedMsgOn());
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBuf);
		return;
	}

	if(MsgId == NETMSGTYPE_CL_SAY)
	{
		CNetMsg_Cl_Say *pMsg = (CNetMsg_Cl_Say *)pRawMsg;
		int Team = pMsg->m_Team;
		if(Team)
			Team = p->GetTeam();
		else
			Team = CGameContext::CHAT_ALL;

		if(g_Config.m_SvSpamprotection && p->m_Last_Chat && p->m_Last_Chat+Server()->TickSpeed() > Server()->Tick())
			return;
		if(str_length(pMsg->m_pMessage)>370) {
			SendChatTarget(ClientId, "Your Message is too long");
			return;
		}

		p->m_Last_Chat = Server()->Tick();

		// check for invalid chars
		unsigned char *pMessage = (unsigned char *)pMsg->m_pMessage;
		while (*pMessage)
		{
			if(*pMessage < 32)
				*pMessage = ' ';
			pMessage++;
		}
		if(pMsg->m_pMessage[0]=='/') {
			if(!str_comp_nocase(pMsg->m_pMessage, "/Credits"))
			{
				SendChatTarget(ClientId, "This mod was originally Created by 3DA");
				SendChatTarget(ClientId, "But now maintained by GreYFoX@GTi among others:");
				SendChatTarget(ClientId, "[blacktee] den, LemonFace, noother & Fluxid");
				SendChatTarget(ClientId, "please check the changelog on DDRace.info");
				SendChatTarget(ClientId, "also the commit log on github.com/GreYFoXGTi/DDRace");
			} else if(!str_comp_nocase(pMsg->m_pMessage, "/pause"))
				{
					if(g_Config.m_SvPauseable)
					{
						CCharacter* chr = p->GetCharacter();

							if(!p->GetTeam() && (!chr->m_aWeapons[WEAPON_NINJA].m_Got || chr->m_FreezeTime) && chr->IsGrounded() && chr->m_Pos==chr->m_PrevPos)
							{
								p->SaveCharacter();
								p->SetTeam(-1);
							}
							else if (p->GetTeam()==-1)
							{
								p->m_PauseInfo.m_Respawn = true;
								p->SetTeam(0);
								//p->LoadCharacter();//TODO:Check if this system Works
							}
							else
								SendChatTarget(ClientId, (chr->m_aWeapons[WEAPON_NINJA].m_Got)?"You can't use /pause while you are a ninja":(!chr->IsGrounded())?"You can't use /pause while you are a in air":"You can't use /pause while you are moving");

							//if(chr->m_RaceState==RACE_STARTED)
							//	chr->m_RaceState = RACE_PAUSE;
							//else if(chr->m_RaceState==RACE_PAUSE)
							//	chr->m_RaceState = RACE_STARTED;*/
					}
					else
						SendChatTarget(ClientId, "The admin didn't activate /pause");
			}
			else if(!str_comp_nocase(pMsg->m_pMessage, "/info"))
			{
					SendChatTarget(ClientId, "DDRace Mod. Version: " DDRACE_VERSION);
					SendChatTarget(ClientId, "Official site: DDRace.info");
					SendChatTarget(ClientId, "For more Info /CMDList");
					SendChatTarget(ClientId, "Or visit DDRace.info");
			}
			else if(!str_comp_nocase(pMsg->m_pMessage, "/flags"))
			{
				char buf[64];
				float temp1;
				float temp2;
				m_Tuning.Get("player_collision",&temp1);
				m_Tuning.Get("player_hooking",&temp2);
				str_format(buf, sizeof(buf), "Flags: Cheats[%s]%s%s Player Collision[%s] PLayer Hook[%s]",
							g_Config.m_SvCheats?"Y":"N",
							(g_Config.m_SvCheats)?" w/Time":"",
							(g_Config.m_SvCheats)?(g_Config.m_SvCheatTime)?"[Y]":"[N]":"",
							temp1?"Y":"N",
							temp2?"Y":"N"
							);
					SendChatTarget(ClientId, buf);
					str_format(buf, sizeof(buf), "Endless Hook[%s] Weapons Effect Others[%s]",g_Config.m_SvEndlessDrag?"Y":"N",g_Config.m_SvHit?"Y":"N");
					SendChatTarget(ClientId, buf);
			}
			else if(!str_comp_nocase(pMsg->m_pMessage, "/CMDList"))
			{
				char buf[64];
				str_format(buf, sizeof(buf), "/Info /Credits %s",g_Config.m_SvPauseable?"/pause":"");
				SendChatTarget(ClientId, buf);
				SendChatTarget(ClientId, "/rank /top5 /top5 5 or any number");
			}
			else if(!str_comp_num(pMsg->m_pMessage, "/top5", 5))
			{
				if(g_Config.m_SvHideScore)
				{
					SendChatTarget(ClientId, "Showing the Top5 is not allowed on this server.");
					return;
				}
				
				int Num;
				
				if(sscanf(pMsg->m_pMessage, "/top5 %d", &Num) == 1)
					Score()->ShowTop5(p->GetCID(), Num);
				else
					Score()->ShowTop5(p->GetCID());
			}
			else if(!str_comp_num(pMsg->m_pMessage, "/rank", 5))
			{
				char aName[256];
				
				if(g_Config.m_SvHideScore && sscanf(pMsg->m_pMessage, "/rank %s", aName) == 1)
					Score()->ShowRank(p->GetCID(), aName, true);
				else
					Score()->ShowRank(p->GetCID(), Server()->ClientName(ClientId));
			}
			else if(!str_comp(pMsg->m_pMessage, "/show_others"))
			{
				if(!g_Config.m_SvShowOthers && !Server()->IsAuthed(ClientId)) {
					SendChatTarget(ClientId, "This command is not allowed on this server.");
					return;
				}
				if(p->m_IsUsingRaceClient)
					SendChatTarget(ClientId, "Please use the settings to switch this option.");
				else
					p->m_ShowOthers = !p->m_ShowOthers;
			}
			else if (!str_comp_nocase(pMsg->m_pMessage, "/time")&&g_Config.m_SvEmotionalTees)
				{
					CCharacter* pChr = p->GetCharacter();
					if (pChr)
					{
						if(pChr->m_BroadTime)
							pChr->m_BroadTime=false;
						else
							pChr->m_BroadTime=true;
					}
				}
			else if(!str_comp_num(pMsg->m_pMessage, "/team", 5))
			{
				int Num;
				
				if(sscanf(pMsg->m_pMessage, "/team %d", &Num) == 1) {
					if(((CGameControllerDDRace*)m_pController)->m_Teams.SetCharacterTeam(p->GetCID(), Num)) {
						char aBuf[512];
						str_format(aBuf, sizeof(aBuf), "%s joined to Team %d", Server()->ClientName(p->GetCID()), Num);
						SendChat(-1, CGameContext::CHAT_ALL, aBuf);
					} else {
						SendChatTarget(ClientId, "You cannot join to this team");
					}
				} else {
					char aBuf[512];
					str_format(aBuf, sizeof(aBuf), "You are in team %d", p->GetCharacter()->Team());
					SendChatTarget(ClientId, aBuf);
				}
					
			}
			else if (!str_comp_nocase(pMsg->m_pMessage, "/broadcast")&&g_Config.m_SvEmotionalTees)
				{
					CCharacter* pChr = p->GetCharacter();
					if (pChr)
					{
						if(pChr->m_BroadCast)
							pChr->m_BroadCast=false;
						else
							pChr->m_BroadCast=true;
					}
				}
			else if (!str_comp_nocase(pMsg->m_pMessage, "/emotepain")&&g_Config.m_SvEmotionalTees)
				{
					CCharacter* pChr = p->GetCharacter();
					if (pChr)
					{
						pChr->m_EmoteType = EMOTE_PAIN;
						pChr->m_EmoteStop = Server()->Tick() + 1 * Server()->TickSpeed();
					}
				}
			else if (!str_comp_nocase(pMsg->m_pMessage, "/emotehappy")&&g_Config.m_SvEmotionalTees)
				{
					CCharacter* pChr = p->GetCharacter();
					if (pChr)
					{
						pChr->m_EmoteType = EMOTE_HAPPY;
						pChr->m_EmoteStop = Server()->Tick() + 1 * Server()->TickSpeed();
					}
				}
			else if (!str_comp_nocase(pMsg->m_pMessage, "/emoteangry")&&g_Config.m_SvEmotionalTees)
				{
					CCharacter* pChr = p->GetCharacter();
					if (pChr)
					{
						pChr->m_EmoteType = EMOTE_ANGRY;
						pChr->m_EmoteStop = Server()->Tick() + 1 * Server()->TickSpeed();
					}
				}
			else if (!str_comp_nocase(pMsg->m_pMessage, "/emoteblink")&&g_Config.m_SvEmotionalTees)
				{
					CCharacter* pChr = p->GetCharacter();
					if (pChr)
					{
						pChr->m_EmoteType = EMOTE_BLINK;
						pChr->m_EmoteStop = Server()->Tick() + 1 * Server()->TickSpeed();
					}
				}
			else if (!str_comp_nocase(pMsg->m_pMessage, "/emotesurprise")&&g_Config.m_SvEmotionalTees)
			{
				CCharacter* pChr = p->GetCharacter();
				if (pChr)
				{
					pChr->m_EmoteType = EMOTE_SURPRISE;
					pChr->m_EmoteStop = Server()->Tick() + 1 * Server()->TickSpeed();
				}
			}
			else if (!str_comp_nocase(pMsg->m_pMessage, "/moodinpain")&&g_Config.m_SvEmotionalTees)
			{
				CCharacter* pChr = p->GetCharacter();
				if (pChr)
				{
					pChr->m_EmoteType = EMOTE_PAIN;
					pChr->m_EmoteStop = Server()->Tick() + 3600 * Server()->TickSpeed();
				}
			}
			else if (!str_comp_nocase(pMsg->m_pMessage, "/moodhappy")&&g_Config.m_SvEmotionalTees)
			{
				CCharacter* pChr = p->GetCharacter();
				if (pChr)
				{
					pChr->m_EmoteType = EMOTE_HAPPY;
					pChr->m_EmoteStop = Server()->Tick() + 3600 * Server()->TickSpeed();
				}
			}
			else if (!str_comp_nocase(pMsg->m_pMessage, "/moodangry")&&g_Config.m_SvEmotionalTees)
			{
				CCharacter* pChr = p->GetCharacter();
				if (pChr)
				{
					pChr->m_EmoteType = EMOTE_ANGRY;
					pChr->m_EmoteStop = Server()->Tick() + 3600 * Server()->TickSpeed();
				}
			}
			else if (!str_comp_nocase(pMsg->m_pMessage, "/moodsad")&&g_Config.m_SvEmotionalTees)
			{
				CCharacter* pChr = p->GetCharacter();
				if (pChr)
				{
					pChr->m_EmoteType = EMOTE_BLINK;
					pChr->m_EmoteStop = Server()->Tick() + 3600 * Server()->TickSpeed();
				}
			}
			else if (!str_comp_nocase(pMsg->m_pMessage, "/moodsurprised")&&g_Config.m_SvEmotionalTees)
			{
				CCharacter* pChr = p->GetCharacter();
				if (pChr)
				{
					pChr->m_EmoteType = EMOTE_SURPRISE;
					pChr->m_EmoteStop = Server()->Tick() + 3600 * Server()->TickSpeed();
				}
			}
			else
					SendChatTarget(ClientId, "No such command!");

		} else {
			if (m_apPlayers[ClientId]->m_Muted == 0)
				SendChat(ClientId, Team, pMsg->m_pMessage);
			else
				SendChatTarget(ClientId, "You are muted");
		}


	}
	else if(MsgId == NETMSGTYPE_CL_ISRACE)
	{
		p->m_IsUsingRaceClient = true;
		// send time of all players
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_apPlayers[i] && Score()->PlayerData(i)->m_CurrentTime > 0)
			{
				char aBuf[16];
				str_format(aBuf, sizeof(aBuf), "%.0f", Score()->PlayerData(i)->m_CurrentTime*100.0f); // damn ugly but the only way i know to do it
				int TimeToSend;
				sscanf(aBuf, "%d", &TimeToSend);
				CNetMsg_Sv_PlayerTime Msg;
				Msg.m_Time = TimeToSend;
				Msg.m_Cid = i;
				Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientId);
			}
		}
	}
	else if (MsgId == NETMSGTYPE_CL_RACESHOWOTHERS)
	{
		if(!g_Config.m_SvShowOthers && !Server()->IsAuthed(ClientId))
			return;
		if(p->m_Last_ShowOthers && p->m_Last_ShowOthers+Server()->TickSpeed()/2 > Server()->Tick())
			return;
		p->m_Last_ShowOthers = Server()->Tick();
		CNetMsg_Cl_RaceShowOthers *pMsg = (CNetMsg_Cl_RaceShowOthers *)pRawMsg;
		p->m_ShowOthers = (bool)pMsg->m_Active;
	}
	else if(MsgId == NETMSGTYPE_CL_CALLVOTE)
	{
		if(g_Config.m_SvSpamprotection && p->m_Last_VoteTry && p->m_Last_VoteTry+Server()->TickSpeed()*3 > Server()->Tick())
			return;

		int64 Now = Server()->Tick();
		p->m_Last_VoteTry = Now;
		if(p->GetTeam() == -1)
		{
			SendChatTarget(ClientId, "Spectators aren't allowed to start a vote.");
			return;
		}

		if(m_VoteCloseTime)
		{
			SendChatTarget(ClientId, "Wait for current vote to end before calling a new one.");
			return;
		}

		int Timeleft = p->m_Last_VoteCall + Server()->TickSpeed()*60 - Now;
		if(p->m_Last_VoteCall && Timeleft > 0)
		{
			char aChatmsg[512] = {0};
			str_format(aChatmsg, sizeof(aChatmsg), "You must wait %d seconds before making another vote", (Timeleft/Server()->TickSpeed())+1);
			SendChatTarget(ClientId, aChatmsg);
			return;
		}

		char aChatmsg[512] = {0};
		char aDesc[512] = {0};
		char aCmd[512] = {0};
		CNetMsg_Cl_CallVote *pMsg = (CNetMsg_Cl_CallVote *)pRawMsg;
		if(str_comp_nocase(pMsg->m_Type, "option") == 0)
		{
			CVoteOption *pOption = m_pVoteOptionFirst;
			static int64 last_mapvote = 0; //floff
			while(pOption)
			{
				if(str_comp_nocase(pMsg->m_Value, pOption->m_aCommand) == 0)
				{
					if(m_apPlayers[ClientId]->m_Authed == 0 && strncmp(pOption->m_aCommand, "sv_map ", 7) == 0 && time_get() < last_mapvote + (time_freq() * g_Config.m_SvVoteMapTimeDelay))
						{
							char chatmsg[512] = {0};
							str_format(chatmsg, sizeof(chatmsg), "There's a %d second delay between map-votes,Please wait %d Second(s)", g_Config.m_SvVoteMapTimeDelay,((last_mapvote+(g_Config.m_SvVoteMapTimeDelay * time_freq()))/time_freq())-(time_get()/time_freq()));
							SendChatTarget(ClientId, chatmsg);

							return;
						}
					str_format(aChatmsg, sizeof(aChatmsg), "%s called vote to change server option '%s'", Server()->ClientName(ClientId), pOption->m_aCommand);
					str_format(aDesc, sizeof(aDesc), "%s", pOption->m_aCommand);
					str_format(aCmd, sizeof(aCmd), "%s", pOption->m_aCommand);
					last_mapvote = time_get();
					break;
				}

				pOption = pOption->m_pNext;
			}

			if(!pOption)
			{
				str_format(aChatmsg, sizeof(aChatmsg), "'%s' isn't an option on this server", pMsg->m_Value);
				SendChatTarget(ClientId, aChatmsg);
				return;
			}
			last_mapvote = time_get();
			m_VoteKick = false;
		}
		else if(str_comp_nocase(pMsg->m_Type, "kick") == 0)
		{
			if(m_apPlayers[ClientId]->m_Authed == 0 && time_get() < m_apPlayers[ClientId]->m_Last_KickVote + (time_freq() * 5))
			return;
			else if(m_apPlayers[ClientId]->m_Authed == 0 && time_get() < m_apPlayers[ClientId]->m_Last_KickVote + (time_freq() * g_Config.m_SvVoteKickTimeDelay))
			{
				char chatmsg[512] = {0};
				str_format(chatmsg, sizeof(chatmsg), "There's a %d second waittime between kickvotes for each player please wait %d second(s)",
				g_Config.m_SvVoteKickTimeDelay,
				((m_apPlayers[ClientId]->m_Last_KickVote + (m_apPlayers[ClientId]->m_Last_KickVote*time_freq()))/time_freq())-(time_get()/time_freq())
				);
				SendChatTarget(ClientId, chatmsg);
				m_apPlayers[ClientId]->m_Last_KickVote = time_get();
				return;
			}
			else if(!g_Config.m_SvVoteKick)
			{
				SendChatTarget(ClientId, "Server does not allow voting to kick players");
				m_apPlayers[ClientId]->m_Last_KickVote = time_get();
				return;
			}

			int KickId = str_toint(pMsg->m_Value);
			if(KickId < 0 || KickId >= MAX_CLIENTS || !m_apPlayers[KickId])
			{
				SendChatTarget(ClientId, "Invalid client id to kick");
				m_apPlayers[ClientId]->m_Last_KickVote = time_get();
				return;
			}
			if(KickId == ClientId)
			{
				SendChatTarget(ClientId, "You cant kick yourself");
				return;
			}
			if(Server()->IsAuthed(KickId))
			{
				SendChatTarget(ClientId, "You cant kick admins");
				m_apPlayers[ClientId]->m_Last_KickVote = time_get();
				char aBufKick[128];
				str_format(aBufKick, sizeof(aBufKick), "%s called for vote to kick you", Server()->ClientName(ClientId));
				SendChatTarget(KickId, aBufKick);
				return;
			}
			if(GetPlayerChar(ClientId) && GetPlayerChar(KickId) &&
				GetPlayerChar(ClientId)->Team() != GetPlayerChar(KickId)->Team()) {
				SendChatTarget(ClientId, "You can kick only your team member");
				m_apPlayers[ClientId]->m_Last_KickVote = time_get();
				return;
			}
			str_format(aChatmsg, sizeof(aChatmsg), "%s called for vote to kick '%s'", Server()->ClientName(ClientId), Server()->ClientName(KickId));
			str_format(aDesc, sizeof(aDesc), "Kick '%s'", Server()->ClientName(KickId));
			if (!g_Config.m_SvVoteKickBanTime)
				str_format(aCmd, sizeof(aCmd), "kick %d", KickId);
			else
			{
				char aBuf[64] = {0};
				Server()->GetClientIP(KickId, aBuf, sizeof(aBuf));
				str_format(aCmd, sizeof(aCmd), "ban %s %d", aBuf, g_Config.m_SvVoteKickBantime);
			}
			m_apPlayers[ClientId]->m_Last_KickVote = time_get();
			m_VoteKick = true;
		}

		if(aCmd[0])
		{
			SendChat(-1, CGameContext::CHAT_ALL, aChatmsg);
			StartVote(aDesc, aCmd);
			p->m_Vote = 1;
			p->m_VotePos = m_VotePos = 1;
			m_VoteCreator = ClientId;
			p->m_Last_VoteCall = Now;
		}
	}
	else if(MsgId == NETMSGTYPE_CL_VOTE)
	{
		if(!m_VoteCloseTime)
			return;

		if(p->m_Vote == 0)
		{
			CNetMsg_Cl_Vote *pMsg = (CNetMsg_Cl_Vote *)pRawMsg;
			if(!pMsg->m_Vote)
				return;

			p->m_Vote = pMsg->m_Vote;
			p->m_VotePos = ++m_VotePos;
			m_VoteUpdate = true;
		}
	}
	else if (MsgId == NETMSGTYPE_CL_SETTEAM && !m_World.m_Paused)
	{
		CNetMsg_Cl_SetTeam *pMsg = (CNetMsg_Cl_SetTeam *)pRawMsg;

		if(p->GetTeam() == pMsg->m_Team || (g_Config.m_SvSpamprotection && p->m_Last_SetTeam && p->m_Last_SetTeam+Server()->TickSpeed()*3 > Server()->Tick()))
			return;

		// Switch team on given client and kill/respawn him
		if(m_pController->CanJoinTeam(pMsg->m_Team, ClientId))
		{
			//if(m_pController->CanChangeTeam(p, pMsg->m_Team))
			//{
			//CCharacter* pChr=GetPlayerChar(ClientId);
				p->m_Last_SetTeam = Server()->Tick();
				if(p->GetTeam() == -1 || pMsg->m_Team == -1)
					m_VoteUpdate = true;
				p->SetTeam(pMsg->m_Team);
				//if(pChr && pMsg->m_Team!=-1 && pChr->m_Paused)
					//pChr->LoadPauseData();
				//TODO:Check if this system Works

				//(void)m_pController->CheckTeamBalance();
			//}
			//else
				//SendBroadcast("Teams must be balanced, please join other team", ClientId);
		}
		else
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Only %d active players are allowed", g_Config.m_SvMaxClients-g_Config.m_SvSpectatorSlots);
			SendBroadcast(aBuf, ClientId);
		}
	}
	else if (MsgId == NETMSGTYPE_CL_CHANGEINFO || MsgId == NETMSGTYPE_CL_STARTINFO)
	{
		CNetMsg_Cl_ChangeInfo *pMsg = (CNetMsg_Cl_ChangeInfo *)pRawMsg;

		if(g_Config.m_SvSpamprotection && p->m_Last_ChangeInfo && p->m_Last_ChangeInfo+Server()->TickSpeed()*5 > Server()->Tick())
			return;

		p->m_Last_ChangeInfo = time_get();
		if (!p->m_ColorSet|| g_Config.m_SvAllowColorChange)
		{
			p->m_TeeInfos.m_UseCustomColor = pMsg->m_UseCustomColor;
			p->m_TeeInfos.m_ColorBody = pMsg->m_ColorBody;
			p->m_TeeInfos.m_ColorFeet = pMsg->m_ColorFeet;
		}

		// copy old name
		char aOldName[MAX_NAME_LENGTH];
		str_copy(aOldName, Server()->ClientName(ClientId), MAX_NAME_LENGTH);

		Server()->SetClientName(ClientId, pMsg->m_pName);
		if(MsgId == NETMSGTYPE_CL_CHANGEINFO && str_comp(aOldName, Server()->ClientName(ClientId)) != 0)
		{
			char aChatText[256];
			str_format(aChatText, sizeof(aChatText), "%s changed name to %s", aOldName, Server()->ClientName(ClientId));
			SendChat(-1, CGameContext::CHAT_ALL, aChatText);
		}

		// set skin
		str_copy(p->m_TeeInfos.m_SkinName, pMsg->m_pSkin, sizeof(p->m_TeeInfos.m_SkinName));

		//m_pController->OnPlayerInfoChange(p);

		if(MsgId == NETMSGTYPE_CL_STARTINFO)
		{
			// send vote options
			CNetMsg_Sv_VoteClearOptions ClearMsg;
			Server()->SendPackMsg(&ClearMsg, MSGFLAG_VITAL, ClientId);
			CVoteOption *pCurrent = m_pVoteOptionFirst;
			while(pCurrent)
			{
				CNetMsg_Sv_VoteOption OptionMsg;
				OptionMsg.m_pCommand = pCurrent->m_aCommand;
				Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, ClientId);
				pCurrent = pCurrent->m_pNext;
			}

			// send tuning parameters to client
			SendTuningParams(ClientId);

			//
			CNetMsg_Sv_ReadyToEnter m;
			Server()->SendPackMsg(&m, MSGFLAG_VITAL|MSGFLAG_FLUSH, ClientId);
		}
	}
	else if (MsgId == NETMSGTYPE_CL_EMOTICON && !m_World.m_Paused)
	{
		CNetMsg_Cl_Emoticon *pMsg = (CNetMsg_Cl_Emoticon *)pRawMsg;

		if(g_Config.m_SvSpamprotection && p->m_Last_Emote && p->m_Last_Emote+Server()->TickSpeed()*3 > Server()->Tick())
			return;

		p->m_Last_Emote = Server()->Tick();

		SendEmoticon(ClientId, pMsg->m_Emoticon);
		CCharacter* pChr = p->GetCharacter();
		if (pChr && g_Config.m_SvEmotionalTees)
		{
			switch(pMsg->m_Emoticon)
			{
				case EMOTICON_2:
				case EMOTICON_8:
					pChr->m_EmoteType = EMOTE_SURPRISE;
					break;
				case EMOTICON_1:
				case EMOTICON_4:
				case EMOTICON_7:
				case EMOTICON_13:
					pChr->m_EmoteType = EMOTE_BLINK;
					break;
				case EMOTICON_3:
				case EMOTICON_6:
					pChr->m_EmoteType = EMOTE_HAPPY;
					break;
				case EMOTICON_9:
				case EMOTICON_15:
					pChr->m_EmoteType = EMOTE_PAIN;
					break;
				case EMOTICON_10:
				case EMOTICON_11:
				case EMOTICON_12:
					pChr->m_EmoteType = EMOTE_ANGRY;
					break;
				default:
					break;
			}
			pChr->m_EmoteStop = Server()->Tick() + 2 * Server()->TickSpeed();
		}
	}
	
	else if (MsgId == NETMSGTYPE_CL_KILL && !m_World.m_Paused)
	{
		if(p->m_Last_Kill && p->m_Last_Kill+Server()->TickSpeed()*3 > Server()->Tick())
			return;

		p->m_Last_Kill = Server()->Tick();
		p->KillCharacter(WEAPON_SELF);
		p->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()*3;
	}
}

void CGameContext::ConTuneParam(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pParamName = pResult->GetString(0);
	float NewValue = pResult->GetFloat(1);

	char aBuf[256];
	if(pSelf->Tuning()->Set(pParamName, NewValue))
	{
		str_format(aBuf, sizeof(aBuf), "%s changed to %.2f", pParamName, NewValue);
		pSelf->SendTuningParams(-1);
	}
	else
		str_format(aBuf, sizeof(aBuf), "No such tuning parameter");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
}

void CGameContext::ConTuneReset(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CTuningParams p;
	*pSelf->Tuning() = p;
	pSelf->SendTuningParams(-1);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "Tuning reset");
}

void CGameContext::ConTuneDump(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	char aBuf[256];
	for(int i = 0; i < pSelf->Tuning()->Num(); i++)
	{
		float v;
		pSelf->Tuning()->Get(i, &v);
		str_format(aBuf, sizeof(aBuf), "%s %.2f", pSelf->Tuning()->m_apNames[i], v);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
	}
}

void CGameContext::ConChangeMap(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_pController->ChangeMap(pResult->GetString(0));
}

void CGameContext::ConRestart(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pResult->NumArguments())
		pSelf->m_pController->DoWarmup(pResult->GetInteger(0));
	else
		pSelf->m_pController->StartRound();
}

void CGameContext::ConBroadcast(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SendBroadcast(pResult->GetString(0), -1);
}

void CGameContext::ConSay(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SendChat(-1, CGameContext::CHAT_ALL, pResult->GetString(0));
}

void CGameContext::ConSetTeam(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int ClientId = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
	int Team = clamp(pResult->GetInteger(1), -1, 1);
	
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "moved client %d to team %d", ClientId, Team);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
	if(!pSelf->m_apPlayers[ClientId] || !compare_players(pSelf->m_apPlayers[cid], pSelf->m_apPlayers[ClientId]))
	if(!pSelf->m_apPlayers[ClientId])
		return;

	pSelf->m_apPlayers[ClientId]->SetTeam(Team);
	//(void)pSelf->m_pController->CheckTeamBalance();
}

void CGameContext::ConAddVote(IConsole::IResult *pResult, void *pUserData, int ClientID)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Len = str_length(pResult->GetString(0));

	CGameContext::CVoteOption *pOption = (CGameContext::CVoteOption *)pSelf->m_pVoteOptionHeap->Allocate(sizeof(CGameContext::CVoteOption) + Len);
	pOption->m_pNext = 0;
	pOption->m_pPrev = pSelf->m_pVoteOptionLast;
	if(pOption->m_pPrev)
		pOption->m_pPrev->m_pNext = pOption;
	pSelf->m_pVoteOptionLast = pOption;
	if(!pSelf->m_pVoteOptionFirst)
		pSelf->m_pVoteOptionFirst = pOption;

	mem_copy(pOption->m_aCommand, pResult->GetString(0), Len+1);
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "added option '%s'", pOption->m_aCommand);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	CNetMsg_Sv_VoteOption OptionMsg;
	OptionMsg.m_pCommand = pOption->m_aCommand;
	pSelf->Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, -1);
}

void CGameContext::ConVote(IConsole::IResult *pResult, void *pUserData, int ClientID)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(str_comp_nocase(pResult->GetString(0), "yes") == 0)
		pSelf->m_VoteEnforce = CGameContext::VOTE_ENFORCE_YES;
	else if(str_comp_nocase(pResult->GetString(0), "no") == 0)
		pSelf->m_VoteEnforce = CGameContext::VOTE_ENFORCE_NO;
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "forcing vote %s", pResult->GetString(0));
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
}

void CGameContext::ConchainSpecialMotdupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData, -1);
	if(pResult->NumArguments())
	{
		CNetMsg_Sv_Motd Msg;
		Msg.m_pMessage = g_Config.m_SvMotd;
		CGameContext *pSelf = (CGameContext *)pUserData;
		for(int i = 0; i < MAX_CLIENTS; ++i)
			if(pSelf->m_apPlayers[i])
				pSelf->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
	}
}


bool CGameContext::CheatsAvailable(int cid) {
	if(!g_Config.m_SvCheats) {
		((CServer*)Server())->SendRconLine(cid, "Cheats are not available on this server.");
	}
	return g_Config.m_SvCheats;
}

void CGameContext::ConGoLeft(IConsole::IResult *pResult, void *pUserData, int cid) {
	CGameContext *pSelf = (CGameContext *)pUserData;

	if (!pSelf->CheatsAvailable(cid))
		return;
	CCharacter* chr = pSelf->GetPlayerChar(cid);
	if(chr)
	{
		chr->m_Core.m_Pos.x -= 16;
		if(!g_Config.m_SvCheatTime)
			chr->m_RaceState = RACE_CHEAT;
	}
}
void  CGameContext::ConGoRight(IConsole::IResult *pResult, void *pUserData, int cid) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!pSelf->CheatsAvailable(cid))
		return;
	CCharacter* chr = pSelf->GetPlayerChar(cid);
	if(chr)
	{
		chr->m_Core.m_Pos.x += 16;
		if(!g_Config.m_SvCheatTime)
			chr->m_RaceState = RACE_CHEAT;
	}
}
void  CGameContext::ConGoUp(IConsole::IResult *pResult, void *pUserData, int cid) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!pSelf->CheatsAvailable(cid))
		return;
	CCharacter* chr = pSelf->GetPlayerChar(cid);
	if(chr)
	{
		chr->m_Core.m_Pos.y -= 16;
		if(!g_Config.m_SvCheatTime)
			chr->m_RaceState = RACE_CHEAT;
	}
}
void  CGameContext::ConGoDown(IConsole::IResult *pResult, void *pUserData, int cid) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!pSelf->CheatsAvailable(cid))
		return;
	CCharacter* chr = pSelf->GetPlayerChar(cid);
	if(chr)
	{
		chr->m_Core.m_Pos.y += 16;
		if(!g_Config.m_SvCheatTime)
			chr->m_RaceState = RACE_CHEAT;
	}
}

void  CGameContext::ConMute(IConsole::IResult *pResult, void *pUserData, int cid) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	int cid1 = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
	int Seconds = pResult->GetInteger(1);
	char buf[512];
	if (Seconds < 10)
		Seconds = 10;
	if (pSelf->m_apPlayers[cid1] && (compare_players(pSelf->m_apPlayers[cid],pSelf->m_apPlayers[cid1])))
	{
		if (pSelf->m_apPlayers[cid1]->m_Muted < Seconds * pSelf->Server()->TickSpeed()) {
			pSelf->m_apPlayers[cid1]->m_Muted = Seconds * pSelf->Server()->TickSpeed();
		}
		else
			Seconds = pSelf->m_apPlayers[cid1]->m_Muted / pSelf->Server()->TickSpeed();
		str_format(buf, sizeof(buf), "%s muted by %s for %d seconds", pSelf->Server()->ClientName(cid1), pSelf->Server()->ClientName(cid), Seconds);
		pSelf->SendChat(-1, CGameContext::CHAT_ALL, buf);
	}
}

void  CGameContext::ConSetlvl(IConsole::IResult *pResult, void *pUserData, int cid) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	int cid1 = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
	int level = clamp(pResult->GetInteger(1), 0, 3);

	if (pSelf->m_apPlayers[cid1] && (pSelf->m_apPlayers[cid1]->m_Authed > level) && (compare_players(pSelf->m_apPlayers[cid],pSelf->m_apPlayers[cid1]) || cid == cid1))
	{
		pSelf->m_apPlayers[cid1]->m_Authed = level;
	}
}

void CGameContext::ConKillPlayer(IConsole::IResult *pResult, void *pUserData, int cid) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	int cid1 = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
	if(!pSelf->m_apPlayers[cid1])
		return;

	if (pSelf->m_apPlayers[cid1] && compare_players(pSelf->m_apPlayers[cid],pSelf->m_apPlayers[cid1]))
	{
		pSelf->m_apPlayers[cid1]->KillCharacter(WEAPON_GAME);
		char buf[512];
		str_format(buf, sizeof(buf), "%s killed by %s", pSelf->Server()->ClientName(cid1), pSelf->Server()->ClientName(cid));
		pSelf->SendChat(-1, CGameContext::CHAT_ALL, buf);
	}
}

void CGameContext::ConNinjaMe(IConsole::IResult *pResult, void *pUserData, int cid) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!pSelf->CheatsAvailable(cid)) return;

	CCharacter* chr = pSelf->GetPlayerChar(cid);
	if(chr) {
		chr->GiveNinja();
		if(!g_Config.m_SvCheatTime)
			chr->m_RaceState = RACE_CHEAT;
	}
}

void CGameContext::ConNinja(IConsole::IResult *pResult, void *pUserData, int cid) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!pSelf->CheatsAvailable(cid)) return;
	int cid1 = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
	CCharacter* chr = pSelf->GetPlayerChar(cid1);
	if(chr)
	{
		if (pSelf->m_apPlayers[cid1] && compare_players(pSelf->m_apPlayers[cid],pSelf->m_apPlayers[cid1]))
		{
			chr->GiveNinja();
			if(!g_Config.m_SvCheatTime)
				chr->m_RaceState = RACE_CHEAT;
		}
	}
}


void CGameContext::ConHammer(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!pSelf->CheatsAvailable(cid)) return;
	char buf[128];

	int cid1 = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
	int type = pResult->GetInteger(1);
	CCharacter* chr = pSelf->GetPlayerChar(cid1);
	if (!chr)
		return;
	CServer* serv = (CServer*)pSelf->Server();
	if (type>3 || type<0)
	{
		serv->SendRconLine(cid, "Select hammer between 0 and 3");
	}
	else
	{
		if (pSelf->m_apPlayers[cid1] && compare_players(pSelf->m_apPlayers[cid],pSelf->m_apPlayers[cid1]))
		{
			chr->m_HammerType = type;
			if(!g_Config.m_SvCheatTime)
				chr->m_RaceState = RACE_CHEAT;
			str_format(buf, sizeof(buf), "Hammer of cid=%d setted to %d",cid1,type);
			serv->SendRconLine(cid1, buf);
		}
	}
}

void CGameContext::ConHammerMe(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!pSelf->CheatsAvailable(cid)) return;
	char buf[128];
	int type = pResult->GetInteger(0);
	CCharacter* chr = pSelf->GetPlayerChar(cid);
	if (!chr)
		return;
	CServer* serv = (CServer*)pSelf->Server();
	if (type>3 || type<0)
	{
		serv->SendRconLine(cid, "Select hammer between 0 and 3");
	}
	else
	{
		chr->m_HammerType = type;
		if(!g_Config.m_SvCheatTime)
			chr->m_RaceState = RACE_CHEAT;
		str_format(buf, sizeof(buf), "Hammer setted to %d",type);
		serv->SendRconLine(cid, buf);
	}
}


void CGameContext::ConSuper(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!pSelf->CheatsAvailable(cid)) return;
	int cid1 = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
	if (pSelf->m_apPlayers[cid1] && compare_players(pSelf->m_apPlayers[cid],pSelf->m_apPlayers[cid1]))
	{
		CCharacter* chr = pSelf->GetPlayerChar(cid1);
		if(chr)
		{
			chr->m_Super = true;
			chr->UnFreeze();
			if(!g_Config.m_SvCheatTime)
				chr->m_RaceState = RACE_CHEAT;
		}
	}
}

void CGameContext::ConUnSuper(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!pSelf->CheatsAvailable(cid)) return;
	int cid1 = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
	if (pSelf->m_apPlayers[cid1] && compare_players(pSelf->m_apPlayers[cid],pSelf->m_apPlayers[cid1]))
	{
		CCharacter* chr = pSelf->GetPlayerChar(cid1);
		if(chr)
		{
			chr->m_Super = false;
		}
	}
}

void CGameContext::ConSuperMe(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!pSelf->CheatsAvailable(cid)) return;
	if (pSelf->m_apPlayers[cid])
	{
		CCharacter* chr = pSelf->GetPlayerChar(cid);
		if(chr)
		{
			chr->m_Super = true;
			chr->UnFreeze();
			if(!g_Config.m_SvCheatTime)
				chr->m_RaceState = RACE_CHEAT;
		}
	}
}

void CGameContext::ConUnSuperMe(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!pSelf->CheatsAvailable(cid)) return;
	if (pSelf->m_apPlayers[cid])
	{
		CCharacter* chr = pSelf->GetPlayerChar(cid);
		if(chr)
		{
			chr->m_Super = false;
		}
	}
}

void CGameContext::ConShotgun(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!pSelf->CheatsAvailable(cid)) return;
	int cid1 = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
	if (pSelf->m_apPlayers[cid1] && compare_players(pSelf->m_apPlayers[cid],pSelf->m_apPlayers[cid1]))
	{
		CCharacter* chr = pSelf->GetPlayerChar(cid1);
		if(chr)
		{
			chr->GiveWeapon(WEAPON_SHOTGUN,-1);
			if(!g_Config.m_SvCheatTime)
				chr->m_RaceState = RACE_CHEAT;
		}
	}
}

void CGameContext::ConShotgunMe(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!pSelf->CheatsAvailable(cid)) return;
	CCharacter* chr = pSelf->GetPlayerChar(cid);
	if(chr)
	{
		chr->GiveWeapon(WEAPON_SHOTGUN,-1);
		if(!g_Config.m_SvCheatTime)
			chr->m_RaceState = RACE_CHEAT;
	}
}

void CGameContext::ConGrenade(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!pSelf->CheatsAvailable(cid)) return;
	int cid1 = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
	if (pSelf->m_apPlayers[cid1] && compare_players(pSelf->m_apPlayers[cid],pSelf->m_apPlayers[cid1]))
	{
		CCharacter* chr = pSelf->GetPlayerChar(cid1);
		if(chr)
		{
			chr->GiveWeapon(WEAPON_GRENADE,-1);
			if(!g_Config.m_SvCheatTime)
				chr->m_RaceState = RACE_CHEAT;
		}
	}
}

void CGameContext::ConGrenadeMe(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!pSelf->CheatsAvailable(cid)) return;
	CCharacter* chr = pSelf->GetPlayerChar(cid);
	if(chr)
	{
		chr->GiveWeapon(WEAPON_GRENADE,-1);
		if(!g_Config.m_SvCheatTime)
			chr->m_RaceState = RACE_CHEAT;
	}
}

void CGameContext::ConLaser(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!pSelf->CheatsAvailable(cid)) return;
	int cid1 = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
	if (pSelf->m_apPlayers[cid1] && compare_players(pSelf->m_apPlayers[cid],pSelf->m_apPlayers[cid1]))
	{
		CCharacter* chr = pSelf->GetPlayerChar(cid1);
		if(chr)
		{
			chr->GiveWeapon(WEAPON_RIFLE,-1);
			if(!g_Config.m_SvCheatTime)
				chr->m_RaceState = RACE_CHEAT;
		}
	}
}

void CGameContext::ConLaserMe(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!pSelf->CheatsAvailable(cid)) return;
	CCharacter* chr = pSelf->GetPlayerChar(cid);
	if(chr)
	{
		chr->GiveWeapon(WEAPON_RIFLE,-1);
		if(!g_Config.m_SvCheatTime)
			chr->m_RaceState = RACE_CHEAT;
	}
}

void CGameContext::ConWeapons(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!pSelf->CheatsAvailable(cid)) return;
	int cid1 = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
	if (pSelf->m_apPlayers[cid1] && compare_players(pSelf->m_apPlayers[cid],pSelf->m_apPlayers[cid1]))
	{
		CCharacter* chr = pSelf->GetPlayerChar(cid1);
		if(chr)
		{
			chr->GiveAllWeapons();
			if(!g_Config.m_SvCheatTime)
				chr->m_RaceState = RACE_CHEAT;
		}
	}
}

void CGameContext::ConWeaponsMe(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!pSelf->CheatsAvailable(cid)) return;
	CCharacter* chr = pSelf->GetPlayerChar(cid);
	if(chr)
	{
		chr->GiveAllWeapons();
		if(!g_Config.m_SvCheatTime)
			chr->m_RaceState = RACE_CHEAT;
	}
}

void CGameContext::ConTeleport(IConsole::IResult *pResult, void *pUserData, int cid) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!pSelf->CheatsAvailable(cid)) return;
	int cid1 = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
 	int cid2 = clamp(pResult->GetInteger(1), 0, (int)MAX_CLIENTS-1);
	if(pSelf->m_apPlayers[cid1] && pSelf->m_apPlayers[cid2])
	{
		if (cid==cid1
			|| (compare_players(pSelf->m_apPlayers[cid],pSelf->m_apPlayers[cid1]) &&  compare_players(pSelf->m_apPlayers[cid],pSelf->m_apPlayers[cid2]))
			|| (compare_players(pSelf->m_apPlayers[cid],pSelf->m_apPlayers[cid1]) && cid2==cid))
		{
			CCharacter* chr = pSelf->GetPlayerChar(cid1);
			if(chr)
			{
				chr->m_Core.m_Pos = pSelf->m_apPlayers[cid2]->m_ViewPos;
				if(!g_Config.m_SvCheatTime)
					chr->m_RaceState = RACE_CHEAT;
			}
		}
	}
}

void CGameContext::ConTimerStop(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	char buf[128];
	CServer* serv = (CServer*)pSelf->Server();
	if(!g_Config.m_SvTimer)
	{

		int cid1 = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
		CCharacter* chr = pSelf->GetPlayerChar(cid1);
		if (!chr)
			return;
		if (pSelf->m_apPlayers[cid1] && compare_players(pSelf->m_apPlayers[cid],pSelf->m_apPlayers[cid1]))
		{
			chr->m_RaceState=RACE_CHEAT;
			str_format(buf, sizeof(buf), "Cid=%d Hasn't time now (Timer Stopped)",cid1);
			serv->SendRconLine(cid1, buf);
		}
	}
	else
	{

		serv->SendRconLine(cid, "Command timer does't allowed");
	}
}

void CGameContext::ConTimerStart(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	char buf[128];
	CServer* serv = (CServer*)pSelf->Server();
	if(!g_Config.m_SvTimer)
	{

		int cid1 = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
		CCharacter* chr = pSelf->GetPlayerChar(cid1);
		if (!chr)
			return;
		if (pSelf->m_apPlayers[cid1] && compare_players(pSelf->m_apPlayers[cid],pSelf->m_apPlayers[cid1]))
		{
			chr->m_RaceState = RACE_STARTED;
			str_format(buf, sizeof(buf), "Cid=%d Has time now (Timer Started)",cid1);
			serv->SendRconLine(cid1, buf);
		}
	}
	else
	{

		serv->SendRconLine(cid, "Command timer does't allowed");
	}
}

void CGameContext::ConTimerZero(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!pSelf->CheatsAvailable(cid)) return;
	char buf[128];

	int cid1 = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);

	CCharacter* chr = pSelf->GetPlayerChar(cid1);
	if (!chr)
		return;
	if (pSelf->m_apPlayers[cid1] && compare_players(pSelf->m_apPlayers[cid],pSelf->m_apPlayers[cid1]))
	{
		chr->m_StartTime = pSelf->Server()->Tick();
		chr->m_RefreshTime = pSelf->Server()->Tick();
		chr->m_RaceState=RACE_CHEAT;
		str_format(buf, sizeof(buf), "Cid=%d time has been reset & stopped.",cid1);
		CServer* serv = (CServer*)pSelf->Server();
		serv->SendRconLine(cid1, buf);
	}

}

void CGameContext::ConTimerReStart(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!pSelf->CheatsAvailable(cid)) return;
	char buf[128];

	int cid1 = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);

	CCharacter* chr = pSelf->GetPlayerChar(cid1);
	if (!chr)
		return;
	if (pSelf->m_apPlayers[cid1] && compare_players(pSelf->m_apPlayers[cid],pSelf->m_apPlayers[cid1]))
	{
		chr->m_StartTime = pSelf->Server()->Tick();
		chr->m_RefreshTime = pSelf->Server()->Tick();
		chr->m_RaceState=RACE_STARTED;
		str_format(buf, sizeof(buf), "Cid=%d time has been reset & stopped.",cid1);
		CServer* serv = (CServer*)pSelf->Server();
		serv->SendRconLine(cid1, buf);
	}

}

void CGameContext::ConFreeze(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	//if(!pSelf->CheatsAvailable(cid)) return;
	char buf[128];
	int time=-1;
	int cid1 = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
	if(pResult->NumArguments()>1)
		time = clamp(pResult->GetInteger(1), -1, 29999);
	CCharacter* chr = pSelf->GetPlayerChar(cid1);
	if (!chr)
		return;
	if (pSelf->m_apPlayers[cid1] && compare_players(pSelf->m_apPlayers[cid],pSelf->m_apPlayers[cid1]))
	{
		chr->Freeze(((time!=0&&time!=-1)?(pSelf->Server()->TickSpeed()*time):(-1)));
		chr->m_pPlayer->m_RconFreeze = true;
		str_format(buf, sizeof(buf), "Cid=%d has been Frozen.",cid1);
		CServer* serv = (CServer*)pSelf->Server();
		serv->SendRconLine(cid1, buf);
	}

}

void CGameContext::ConUnFreeze(IConsole::IResult *pResult, void *pUserData, int cid)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	//if(!pSelf->CheatsAvailable(cid)) return;
	char buf[128];
	int cid1 = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
	CCharacter* chr = pSelf->GetPlayerChar(cid1);
	if (!chr)
		return;
	chr->m_FreezeTime=2;
	chr->m_pPlayer->m_RconFreeze = false;
	str_format(buf, sizeof(buf), "Cid=%d has been UnFreezed.",cid1);
	CServer* serv = (CServer*)pSelf->Server();
	serv->SendRconLine(cid1, buf);

}

void CGameContext::OnConsoleInit()
{
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	Console()->Register("freeze", "i?i", CFGFLAG_SERVER, ConFreeze, this, "Freezes Player i1 for i2 seconds Default Infinity",2);
	Console()->Register("unfreeze", "i", CFGFLAG_SERVER, ConUnFreeze, this, "UnFreezes Player i",2);
	Console()->Register("timerstop", "i", CFGFLAG_SERVER, ConTimerStop, this, "Stops The Timer of Player i",2);
	Console()->Register("timerstart", "i", CFGFLAG_SERVER, ConTimerStart, this, "Starts The Timer of Player i",2);
	Console()->Register("timerrestart", "i", CFGFLAG_SERVER, ConTimerReStart, this, "Starts The Timer of Player i with the time of 00:00:00",2);
	Console()->Register("timerzero", "i", CFGFLAG_SERVER, ConTimerZero, this, "00:00:00 the timer of Player i and Stops it",1);

	Console()->Register("tele", "ii", CFGFLAG_SERVER, ConTeleport, this, "Teleports Player i1 to i2",2);

	Console()->Register("shotgun", "i", CFGFLAG_SERVER, ConShotgun, this, "Give shotgun weapon to player i",2);
	Console()->Register("shotgun_me", "", CFGFLAG_SERVER, ConShotgunMe, this, "Give shotgun weapon to self",1);
	Console()->Register("grenade", "i", CFGFLAG_SERVER, ConGrenade, this, "Give grenade weapon to player i",2);
	Console()->Register("grenade_me", "", CFGFLAG_SERVER, ConGrenadeMe, this, "Give grenade weapon to self",1);
	Console()->Register("laser", "i", CFGFLAG_SERVER, ConLaser, this, "Give rifle weapon to player i",2);
	Console()->Register("laser_me", "", CFGFLAG_SERVER, ConLaserMe, this, "Give rifle weapon to self",1);
	Console()->Register("weapons", "i", CFGFLAG_SERVER, ConWeapons, this, "Give all weapons to player i",2);
	Console()->Register("weapons_me", "", CFGFLAG_SERVER, ConWeaponsMe, this, "Give all weapons to self",1);

	Console()->Register("super", "i", CFGFLAG_SERVER, ConSuper, this, "Make player i super",2);
	Console()->Register("unsuper", "i", CFGFLAG_SERVER, ConUnSuper, this, "Remove super from player i",2);
	Console()->Register("super_me", "", CFGFLAG_SERVER, ConSuperMe, this, "Make player self super",1);
	Console()->Register("unsuper_me", "", CFGFLAG_SERVER, ConUnSuperMe, this, "Remove super from self",1);// Mo

	Console()->Register("hammer_me", "i", CFGFLAG_SERVER, ConHammerMe, this, "Sets the hammer of self to the power of i",1);
	Console()->Register("hammer", "ii", CFGFLAG_SERVER, ConHammer, this, "Sets the hammer of player i1 to the power of i2",2);

	Console()->Register("ninja", "i", CFGFLAG_SERVER, ConNinja, this, "Makes Player i have ninja power-up",2);
	Console()->Register("ninja_me", "", CFGFLAG_SERVER, ConNinjaMe, this, "Makes self have ninja power-up",1);


	Console()->Register("kill_pl", "i", CFGFLAG_SERVER, ConKillPlayer, this, "Kills player with id i and announces the kill",2);
	Console()->Register("auth", "ii", CFGFLAG_SERVER, ConSetlvl, this, "Authenticates player i1 to the level of i2 ( warining he can use sv_rcon_password_XXXX and get the passwords ) level 0 = logout",3);
	Console()->Register("mute", "ii", CFGFLAG_SERVER, ConMute, this, "mutes player i1 for i2 seconds", 2);

	Console()->Register("tune", "si", CFGFLAG_SERVER, ConTuneParam, this, "", 4);
	Console()->Register("tune_reset", "", CFGFLAG_SERVER, ConTuneReset, this, "", 4);
	Console()->Register("tune_dump", "", CFGFLAG_SERVER, ConTuneDump, this, "", 4);

	Console()->Register("change_map", "r", CFGFLAG_SERVER, ConChangeMap, this, "", 3);
	Console()->Register("restart", "?i", CFGFLAG_SERVER, ConRestart, this, "", 3);
	Console()->Register("broadcast", "r", CFGFLAG_SERVER, ConBroadcast, this, "", 3);
	Console()->Register("say", "r", CFGFLAG_SERVER, ConSay, this, "", 3);
	Console()->Register("set_team", "ii", CFGFLAG_SERVER, ConSetTeam, this, "", 2);

	Console()->Register("left", "", CFGFLAG_SERVER, ConGoLeft, this, "",1);
	Console()->Register("right", "", CFGFLAG_SERVER, ConGoRight, this, "",1);
	Console()->Register("up", "", CFGFLAG_SERVER, ConGoUp, this, "",1);
	Console()->Register("down", "", CFGFLAG_SERVER, ConGoDown, this, "",1);
	//Console()->Register("sv_cheats", "i", CFGFLAG_SERVER, ConCheats , 0, "Turns Cheats On/Off",4);
	Console()->Register("addvote", "r", CFGFLAG_SERVER, ConAddVote, this, "", 4);
	Console()->Register("vote", "r", CFGFLAG_SERVER, ConVote, this, "", 3);

	Console()->Chain("sv_motd", ConchainSpecialMotdupdate, this);

}

void CGameContext::OnInit(/*class IKernel *pKernel*/)
{
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_World.SetGameServer(this);
	m_Events.SetGameServer(this);
	m_Size = 0;
	//if(!data) // only load once
		//data = load_data_from_memory(internal_data);

	for(int i = 0; i < NUM_NETOBJTYPES; i++)
		Server()->SnapSetStaticsize(i, m_NetObjHandler.GetObjSize(i));

	m_Layers.Init(Kernel());
	m_Collision.Init(&m_Layers);

	// reset everything here
	//world = new GAMEWORLD;
	//players = new CPlayer[MAX_CLIENTS];

	//TODO: No need any more?
	char buf[512];
	str_format(buf, sizeof(buf), "data/maps/%s.cfg", g_Config.m_SvMap); //
	Console()->ExecuteFile(buf);
	str_format(buf, sizeof(buf), "data/maps/%s.map.cfg", g_Config.m_SvMap);
	Console()->ExecuteFile(buf);
//   dbg_msg("Note","For map cfgs in windows and linux u need the files");
//   dbg_msg("Note","in a folder i nthe same dir as teeworlds_srv");
//   dbg_msg("Note","data/maps/%s.cfg", config.sv_map);
	// select gametype
	m_pController = new CGameControllerDDRace(this);
	//float temp;
	//m_Tuning.Get("player_hooking",&temp);
	//g_Config.m_SvPhook = temp;
	//m_Tuning.Get("player_collision",&temp);
	//g_Config.m_SvNpc=(!temp);

	Server()->SetBrowseInfo(m_pController->m_pGameType, -1);


	// delete old score object
	if(m_pScore)
		delete m_pScore;
		
	// create score object (add sql later)
	if(g_Config.m_SvUseSQL)
		m_pScore = new CSqlScore(this);
	else
		m_pScore = new CFileScore(this);
	// setup core world
	//for(int i = 0; i < MAX_CLIENTS; i++)
	//	game.players[i].core.world = &game.world.core;

	// create all entities from the game layer
	CMapItemLayerTilemap *pTileMap = m_Layers.GameLayer();
	CTile *pTiles = (CTile *)Kernel()->RequestInterface<IMap>()->GetData(pTileMap->m_Data);
	//CMapItemLayerTilemap *pFrontMap = m_Layers.FrontLayer(); not needed game layer and front layer are always the same size
	CTile *pFront=0;
	if (m_Layers.FrontLayer())
	pFront = (CTile *)Kernel()->RequestInterface<IMap>()->GetData(pTileMap->m_Front);
	CTeleTile *pSwitch=0;
	if (m_Layers.SwitchLayer())
	pSwitch = (CTeleTile *)Kernel()->RequestInterface<IMap>()->GetData(pTileMap->m_Switch);
	if (pSwitch)
	{
		if(Collision()->Layers()->SwitchLayer())
			for(int y = 0; y < pTileMap->m_Height; y++)
				for(int x = 0; x < pTileMap->m_Width; x++)
				{
					int sides[8][2];
					sides[0][0]=pSwitch[(x)+pTileMap->m_Width*(y+1)].m_Type - ENTITY_OFFSET;
					sides[1][0]=pSwitch[(x+1)+pTileMap->m_Width*(y+1)].m_Type - ENTITY_OFFSET;
					sides[2][0]=pSwitch[(x+1)+pTileMap->m_Width*(y)].m_Type - ENTITY_OFFSET;
					sides[3][0]=pSwitch[(x+1)+pTileMap->m_Width*(y-1)].m_Type - ENTITY_OFFSET;
					sides[4][0]=pSwitch[(x)+pTileMap->m_Width*(y-1)].m_Type - ENTITY_OFFSET;
					sides[5][0]=pSwitch[(x-1)+pTileMap->m_Width*(y-1)].m_Type - ENTITY_OFFSET;
					sides[6][0]=pSwitch[(x-1)+pTileMap->m_Width*(y)].m_Type - ENTITY_OFFSET;
					sides[7][0]=pSwitch[(x-1)+pTileMap->m_Width*(y+1)].m_Type - ENTITY_OFFSET;
					sides[0][1]=pSwitch[(x)+pTileMap->m_Width*(y+1)].m_Number;
					sides[1][1]=pSwitch[(x+1)+pTileMap->m_Width*(y+1)].m_Number;
					sides[2][1]=pSwitch[(x+1)+pTileMap->m_Width*(y)].m_Number;
					sides[3][1]=pSwitch[(x+1)+pTileMap->m_Width*(y-1)].m_Number;
					sides[4][1]=pSwitch[(x)+pTileMap->m_Width*(y-1)].m_Number;
					sides[5][1]=pSwitch[(x-1)+pTileMap->m_Width*(y-1)].m_Number;
					sides[6][1]=pSwitch[(x-1)+pTileMap->m_Width*(y)].m_Number;
					sides[7][1]=pSwitch[(x-1)+pTileMap->m_Width*(y+1)].m_Number;
					for(int i=0; i<8;i++)
						if ((sides[i][0] >= ENTITY_LASER_SHORT && sides[i][0] <= ENTITY_LASER_LONG) && Collision()->SwitchLayer()[y*pTileMap->m_Width+x].m_Number == sides[i][1])
							m_Size++;
				}
		if(m_Size)
		{
			m_SDoors = new SDoors[m_Size];
			int num=0;
			for(int y = 0; y < pTileMap->m_Height; y++)
				for(int x = 0; x < pTileMap->m_Width; x++)
					if(Collision()->SwitchLayer()[y*pTileMap->m_Width+x].m_Type == (ENTITY_DOOR + ENTITY_OFFSET))
					{
						int sides[8][2];
						sides[0][0]=pSwitch[(x)+pTileMap->m_Width*(y+1)].m_Type - ENTITY_OFFSET;
						sides[1][0]=pSwitch[(x+1)+pTileMap->m_Width*(y+1)].m_Type - ENTITY_OFFSET;
						sides[2][0]=pSwitch[(x+1)+pTileMap->m_Width*(y)].m_Type - ENTITY_OFFSET;
						sides[3][0]=pSwitch[(x+1)+pTileMap->m_Width*(y-1)].m_Type - ENTITY_OFFSET;
						sides[4][0]=pSwitch[(x)+pTileMap->m_Width*(y-1)].m_Type - ENTITY_OFFSET;
						sides[5][0]=pSwitch[(x-1)+pTileMap->m_Width*(y-1)].m_Type - ENTITY_OFFSET;
						sides[6][0]=pSwitch[(x-1)+pTileMap->m_Width*(y)].m_Type - ENTITY_OFFSET;
						sides[7][0]=pSwitch[(x-1)+pTileMap->m_Width*(y+1)].m_Type - ENTITY_OFFSET;
						sides[0][1]=pSwitch[(x)+pTileMap->m_Width*(y+1)].m_Number;
						sides[1][1]=pSwitch[(x+1)+pTileMap->m_Width*(y+1)].m_Number;
						sides[2][1]=pSwitch[(x+1)+pTileMap->m_Width*(y)].m_Number;
						sides[3][1]=pSwitch[(x+1)+pTileMap->m_Width*(y-1)].m_Number;
						sides[4][1]=pSwitch[(x)+pTileMap->m_Width*(y-1)].m_Number;
						sides[5][1]=pSwitch[(x-1)+pTileMap->m_Width*(y-1)].m_Number;
						sides[6][1]=pSwitch[(x-1)+pTileMap->m_Width*(y)].m_Number;
						sides[7][1]=pSwitch[(x-1)+pTileMap->m_Width*(y+1)].m_Number;
						for(int i=0; i<8;i++)
							if ((sides[i][0] >= ENTITY_LASER_SHORT && sides[i][0] <= ENTITY_LASER_LONG) && Collision()->SwitchLayer()[y*pTileMap->m_Width+x].m_Number == sides[i][1])
							{
								vec2 Pos(x*32.0f+16.0f, y*32.0f+16.0f);
								m_SDoors[num].m_Address = new CDoor(&m_World, Pos, pi/4*i, (32*3 + 32*(sides[i][0] - ENTITY_LASER_SHORT)*3), false);
								m_SDoors[num].m_Pos = Pos;
								m_SDoors[num++].m_Number = Collision()->SwitchLayer()[y*pTileMap->m_Width+x].m_Number;
							}
					}
		}
	}

	for(int y = 0; y < pTileMap->m_Height; y++)
	{
		for(int x = 0; x < pTileMap->m_Width; x++)
		{
			int Index = pTiles[y*pTileMap->m_Width+x].m_Index;
			if(Index == TILE_NPC)
				g_Config.m_SvNpc = 1;
			else if (Index == TILE_EHOOK)
				g_Config.m_SvEndlessDrag = 1;
			else if (Index == TILE_NOHIT)
				g_Config.m_SvHit = 0;
			else if (Index == TILE_NPH)
				g_Config.m_SvPhook = 0;
			if(Index >= ENTITY_OFFSET)
			{
				vec2 Pos(x*32.0f+16.0f, y*32.0f+16.0f);
				m_pController->OnEntity(Index-ENTITY_OFFSET, Pos,false);
			}
			if (pFront)
			{
				int Index = pFront[y*pTileMap->m_Width+x].m_Index;
				if(Index == TILE_NPC)
					g_Config.m_SvNpc = 1;
				else if (Index == TILE_EHOOK)
					g_Config.m_SvEndlessDrag = 1;
				else if (Index == TILE_NOHIT)
					g_Config.m_SvHit = 0;
				else if (Index == TILE_NPH)
					g_Config.m_SvPhook = 0;

				if(Index >= ENTITY_OFFSET)
				{
					vec2 Pos(x*32.0f+16.0f, y*32.0f+16.0f);
					m_pController->OnEntity(Index-ENTITY_OFFSET, Pos,true);
				}
			}
			if (pSwitch)
			{
				int Index = pSwitch[y*pTileMap->m_Width+x].m_Type - ENTITY_OFFSET;
				vec2 Pos(x*32.0f+16.0f, y*32.0f+16.0f);
				if(Index == ENTITY_TRIGGER)
					for(int i=0;i<m_Size;i++)
						if(m_SDoors[i].m_Number == pSwitch[y*pTileMap->m_Width+x].m_Number)
							new CTrigger(&m_World,Pos, m_SDoors[i].m_Address);
			}
		}
	}

#ifdef CONF_DEBUG
	if(g_Config.m_DbgDummies)
	{
		for(int i = 0; i < g_Config.m_DbgDummies ; i++)
		{
			OnClientConnected(MAX_CLIENTS-i-1);
		}
	}
#endif
}

void CGameContext::OnShutdown()
{
	delete m_pController;
	m_pController = 0;
	Clear();
}

void CGameContext::OnSnap(int ClientId)
{
	m_World.Snap(ClientId);
	m_pController->Snap(ClientId);
	m_Events.Snap(ClientId);

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i])
			m_apPlayers[i]->Snap(ClientId);
	}
}
void CGameContext::OnPreSnap() {}
void CGameContext::OnPostSnap()
{
	m_Events.Clear();
}

const char *CGameContext::Version() { return GAME_VERSION; }
const char *CGameContext::NetVersion() { return GAME_NETVERSION; }

IGameServer *CreateGameServer() { return new CGameContext; }
