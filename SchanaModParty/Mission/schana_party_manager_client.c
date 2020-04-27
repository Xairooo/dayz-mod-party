class SchanaPartyManagerClient
{
    private ref map<string, ref SchanaPartyNametagsMenu> m_SchanaNametags;
    private ref map<string, vector> positions;
    private ref map<string, float> healths;
	private ref map<string, string> allPlayers;
	private ref map<string, string> sortingMap;
    private bool hasRegistered = false;
	private bool canRegisterAgain = true;

    void SchanaPartyManagerClient()
    {
        Print("[SchanaParty] Client Init");
        positions = new ref map<string, vector>();
		healths = new ref map<string, float>();
		allPlayers = new ref map<string, string>();
		sortingMap = new ref map<string, string>();
        m_SchanaNametags = new map<string, ref SchanaPartyNametagsMenu>();
        GetRPCManager().AddRPC("SchanaModParty", "ClientUpdatePartyInfoRPC", this, SingleplayerExecutionType.Both);
		GetRPCManager().AddRPC("SchanaModParty", "ClientUpdatePlayersInfoRPC", this, SingleplayerExecutionType.Both);

        GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(this.Update, 1000, true);
		GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(this.ResetRegisterLock, 1000, true);
        GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(this.RenewRegistration, 5000, true);
    }

    void ~SchanaPartyManagerClient()
    {
        GetGame().GetCallQueue(CALL_CATEGORY_GUI).Remove(this.Update);
		GetGame().GetCallQueue(CALL_CATEGORY_GUI).Remove(this.ResetRegisterLock);
        GetGame().GetCallQueue(CALL_CATEGORY_GUI).Remove(this.RenewRegistration);
    }

    void RenewRegistration()
    {
        hasRegistered = false;
    }
	
	void ResetRegisterLock()
	{
		canRegisterAgain = true;
	}

    void ClientUpdatePartyInfoRPC(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
    {
        Param3<ref array<ref string>, ref array<ref vector>, ref array<ref float>> data;
        if (!ctx.Read(data))
            return;

        string result;
        JsonSerializer().WriteToString(data, false, result);
        Print("[SchanaParty] PartyInfo " + result);

        ClientUpdatePartyInfo(data.param1, data.param2, data.param3);
    }

    void ClientUpdatePartyInfo(ref array<ref string> party_ids, ref array<ref vector> server_positions, ref array<ref float> server_healths)
    {
        Print("[SchanaParty] ClientUpdatePartyInfo");

        positions.Clear();
        healths.Clear();

        int i;
        for (i = 0; i < party_ids.Count(); ++i)
        {
            positions.Insert(party_ids[i], server_positions[i]);
            healths.Insert(party_ids[i], server_healths[i]);
        }
    }
	
	void ClientUpdatePlayersInfoRPC(CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target)
    {
        Param2<ref array<ref string>, ref array<ref string>> data;
        if (!ctx.Read(data))
            return;

        string result;
        JsonSerializer().WriteToString(data, false, result);
        Print("[SchanaParty] PlayersInfo " + result);

        ClientUpdatePlayersInfo(data.param1, data.param2);
    }
	
	void ClientUpdatePlayersInfo(ref array<ref string> player_ids, ref array<ref string> player_names)
    {
        Print("[SchanaParty] ClientUpdatePlayersInfo");

        allPlayers.Clear();
		
		PlayerBase activePlayer = PlayerBase.Cast(GetGame().GetPlayer());
		if (activePlayer && activePlayer.GetIdentity())
        {
			string activePlayerId = activePlayer.GetIdentity().GetId();
	
	        int i;
	        for (i = 0; i < player_ids.Count(); ++i)
	        {
				if(player_ids[i] != activePlayerId)
				{
	            	allPlayers.Insert(player_ids[i], player_names[i]);
				}
	        }
		}
    }

    private void Update()
    {
        //Print("[SchanaParty] Update");
        PlayerBase activePlayer = PlayerBase.Cast(GetGame().GetPlayer());

        if (activePlayer && activePlayer.GetIdentity())
        {
            string activePlayerId = activePlayer.GetIdentity().GetId();

            if (!hasRegistered && canRegisterAgain)
            {
				Print("[SchanaParty] Update Register");
                auto members = SchanaModPartySettings.Get().GetMembers();
                auto data = new Param2<string, ref array<ref string>>(activePlayerId, members);
                GetRPCManager().SendRPC("SchanaModParty", "ServerRegisterPartyRPC", data, true);
                hasRegistered = true;
				canRegisterAgain = false;
            }
            foreach (string party_id, vector position : positions)
            {
                if (!m_SchanaNametags.Get(party_id))
                {
					Print("[SchanaParty] Adding nametag " + party_id);
                    m_SchanaNametags[party_id] = new SchanaPartyNametagsMenu(null);
                }
                m_SchanaNametags[party_id].SchanaPartyUpdatePosition(position);
                m_SchanaNametags[party_id].SchanaPartyUpdateHealth(healths.Get(party_id));
                m_SchanaNametags[party_id].SchanaPartyUpdateName(SchanaModPartySettings.Get().GetName(party_id));
            }
			
			auto member_ids = m_SchanaNametags.GetKeyArray();
			foreach (auto member_id : member_ids)
			{
				if(!positions.Get(member_id))
				{
					Print("[SchanaParty] Removing nametag " + member_id);
					m_SchanaNametags[member_id].SchanaPartySetRemoveFlag();
					m_SchanaNametags.Remove(member_id);
				}
			}
			
			sortingMap.Clear();
			member_ids = m_SchanaNametags.GetKeyArray();
			foreach (auto sorting_id : member_ids)
			{
				sortingMap.Insert(SchanaModPartySettings.Get().GetName(sorting_id) + sorting_id, sorting_id);
			}
			auto sorted_keys = sortingMap.GetKeyArray();
			sorted_keys.Sort();
			int sortedIndex = 0;
			foreach (auto sorted_key : sorted_keys)
			{
				m_SchanaNametags[sortingMap.Get(sorted_key)].SchanaPartyUpdateListIndex(sortedIndex);
				++sortedIndex;
			}

            foreach (Man man : ClientData.m_PlayerBaseList)
            {
                PlayerBase player = PlayerBase.Cast(man);
                if (player && player.GetIdentity())
                {
                    string id = player.GetIdentity().GetId();
                    if (positions.Get(id) && id != activePlayerId)
                    {
                        m_SchanaNametags[id].SchanaPartyUpdatePlayer(player);
                    }
                }
            }
        }
    }

    void AddPlayerToParty(string id)
    {
		Print("[SchanaParty] AddPlayerToParty " + id);
		SchanaModPartySettings.Get().Add(id, GetOnlinePlayers().Get(id));
		hasRegistered = false;
		Update();
    }

    void RemovePlayerFromParty(string id)
    {
		Print("[SchanaParty] RemovePlayerFromParty " + id);
        SchanaModPartySettings.Get().Remove(id);
		positions.Remove(id);
		hasRegistered = false;
		Update();
    }
	
	bool IsPartyMemberOnline(string id)
	{
		return positions.Contains(id);
	}
	
	bool IsPartyMemberOnlineButNotMutual(string id)
	{
		return !positions.Contains(id) && allPlayers.Contains(id);
	}
	
	ref map<string, string> GetOnlinePlayers()
	{
		return allPlayers;
	}
}

static ref SchanaPartyManagerClient g_SchanaPartyManagerClient;
static ref SchanaPartyManagerClient GetSchanaPartyManagerClient()
{
    if (!g_Game.IsServer() && !g_SchanaPartyManagerClient)
    {
        g_SchanaPartyManagerClient = new SchanaPartyManagerClient;
    }
    return g_SchanaPartyManagerClient;
}
