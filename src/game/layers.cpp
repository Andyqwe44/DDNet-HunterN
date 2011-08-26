/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "layers.h"

CLayers::CLayers()
{
	m_GroupsNum = 0;
	m_GroupsStart = 0;
	m_LayersNum = 0;
	m_LayersStart = 0;
	m_pGameGroup = 0;
	m_pGameLayer = 0;
	m_pMap = 0;

	m_pTeleLayer = 0;
	m_pSpeedupLayer = 0;
	m_pFrontLayer = 0;
	m_pSwitchLayer = 0;
}

void CLayers::Init(class IKernel *pKernel)
{
	m_pMap = pKernel->RequestInterface<IMap>();
	m_pMap->GetType(MAPITEMTYPE_GROUP, &m_GroupsStart, &m_GroupsNum);
	m_pMap->GetType(MAPITEMTYPE_LAYER, &m_LayersStart, &m_LayersNum);

	m_pTeleLayer = 0;
	m_pSpeedupLayer = 0;
	m_pFrontLayer = 0;
	m_pSwitchLayer = 0;

	for(int g = 0; g < NumGroups(); g++)
	{
		CMapItemGroup *pGroup = GetGroup(g);
		for(int l = 0; l < pGroup->m_NumLayers; l++)
		{
			CMapItemLayer *pLayer = GetLayer(pGroup->m_StartLayer+l);

			if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				CMapItemLayerTilemap *pTilemap = reinterpret_cast<CMapItemLayerTilemap *>(pLayer);

				if(pTilemap->m_Version <= 2)
				{
					pTilemap->m_Switch = *((int*)(pTilemap) + 18);
					pTilemap->m_Front = *((int*)(pTilemap) + 17);
					pTilemap->m_Speedup = *((int*)(pTilemap) + 16);
					pTilemap->m_Tele = *((int*)(pTilemap) + 15);
				}

				if(pTilemap->m_Flags&TILESLAYERFLAG_GAME)
				{
					m_pGameLayer = pTilemap;
					m_pGameGroup = pGroup;

					// make sure the game group has standard settings
					m_pGameGroup->m_OffsetX = 0;
					m_pGameGroup->m_OffsetY = 0;
					m_pGameGroup->m_ParallaxX = 100;
					m_pGameGroup->m_ParallaxY = 100;

					if(m_pGameGroup->m_Version >= 2)
					{
						m_pGameGroup->m_UseClipping = 0;
						m_pGameGroup->m_ClipX = 0;
						m_pGameGroup->m_ClipY = 0;
						m_pGameGroup->m_ClipW = 0;
						m_pGameGroup->m_ClipH = 0;
					}

					//break;
				}
				if(pTilemap->m_Flags&TILESLAYERFLAG_TELE)
					m_pTeleLayer = pTilemap;
				if(pTilemap->m_Flags&TILESLAYERFLAG_SPEEDUP)
					m_pSpeedupLayer = pTilemap;
				if(pTilemap->m_Flags&TILESLAYERFLAG_FRONT)
					m_pFrontLayer = pTilemap;
				if(pTilemap->m_Flags&TILESLAYERFLAG_SWITCH)
					m_pSwitchLayer = pTilemap;
			}
		}
	}
}

CMapItemGroup *CLayers::GetGroup(int Index) const
{
	return static_cast<CMapItemGroup *>(m_pMap->GetItem(m_GroupsStart+Index, 0, 0));
}

CMapItemLayer *CLayers::GetLayer(int Index) const
{
	return static_cast<CMapItemLayer *>(m_pMap->GetItem(m_LayersStart+Index, 0, 0));
}

// DDRace

void CLayers::Dest()
{
	/*m_pTeleLayer = 0;
	m_pSpeedupLayer = 0;
	m_pFrontLayer = 0;
	m_pSwitchLayer = 0;*/
}
