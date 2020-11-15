class SchanaPartyManagerServer {
	private ref map<string, ref set<string>> configurations;
	private bool canSendInfo = true;

	void SchanaPartyManagerServer () {
		SchanaPartyUtils.LogMessage ("Server Init " + SCHANA_PARTY_VERSION);
		configurations = new ref map<string, ref set<string>> ();
		GetRPCManager ().AddRPC ("SchanaModParty", "ServerRegisterPartyRPC", this, SingleplayerExecutionType.Both);

		GetGame ().GetCallQueue (CALL_CATEGORY_SYSTEM).CallLater (this.SendInfo, 10000, true);
		GetGame ().GetCallQueue (CALL_CATEGORY_SYSTEM).CallLater (this.ResetSendInfoLock, GetSchanaPartyServerSettings ().GetSendInfoFrequency () * 1000, true);

		int logFrequency = GetSchanaPartyServerSettings ().GetLogFrequency ();
		if (logFrequency > 0) {
			GetGame ().GetCallQueue (CALL_CATEGORY_SYSTEM).CallLater (this.LogParties, logFrequency * 1000, true);
		}
	}

	void ~SchanaPartyManagerServer () {
		GetGame ().GetCallQueue (CALL_CATEGORY_SYSTEM).Remove (this.SendInfo);
		GetGame ().GetCallQueue (CALL_CATEGORY_SYSTEM).Remove (this.ResetSendInfoLock);

		int logFrequency = GetSchanaPartyServerSettings ().GetLogFrequency ();
		if (logFrequency > 0) {
			GetGame ().GetCallQueue (CALL_CATEGORY_SYSTEM).Remove (this.LogParties);
		}
	}

	private void ResetSendInfoLock () {
		canSendInfo = true;
	}

	private void LogParties () {
		string result;
		auto parties = GetParties ();
		JsonSerializer ().WriteToString (parties, false, result);
		SchanaPartyUtils.Warn ("Parties " + result);
	}

	void ServerRegisterPartyRPC (CallType type, ref ParamsReadContext ctx, ref PlayerIdentity sender, ref Object target) {
		Param2<string, ref array<string>> data;
		if (!ctx.Read (data))
			return;

		if (SchanaPartyUtils.WillLog (SchanaPartyUtils.INFO)) {
			string result;
			JsonSerializer ().WriteToString (data, false, result);
			SchanaPartyUtils.Info ("ServerRegisterPartyRPC " + result);
		}

		ServerRegisterParty (data.param1, data.param2);
	}

	private void ServerRegisterParty (string key, ref array<string> ids) {
		SchanaPartyUtils.Info ("Register " + ids.Count ().ToString () + " to " + key);
		auto party_members = new ref set<string> ();
		foreach (string id : ids) {
			party_members.Insert (id);
		}

		string result;

		if (SchanaPartyUtils.WillLog (SchanaPartyUtils.TRACE)) {
			JsonSerializer ().WriteToString (configurations, false, result);
			SchanaPartyUtils.Trace ("ServerRegisterParty Before " + result);
		}

		configurations.Set (key, party_members);

		if (SchanaPartyUtils.WillLog (SchanaPartyUtils.TRACE)) {
			JsonSerializer ().WriteToString (configurations, false, result);
			SchanaPartyUtils.Trace ("ServerRegisterParty After " + result);
		}

		SendInfo ();
	}

	ref map<string, ref set<string>> GetParties () {
		auto parties = new ref map<string, ref set<string>> ();

		foreach (auto owner_id, auto party_ids : configurations) {
			auto validated_party_ids = new ref set<string> ();

			foreach (string member_id : party_ids) {
				if (configurations.Contains (member_id) && configurations.Get (member_id).Find (owner_id) != -1) {
					validated_party_ids.Insert (member_id);
				} else if (configurations.Contains (member_id) && GetSchanaPartyServerSettings ().GetAdminIds ().Find (owner_id) != -1) {
					validated_party_ids.Insert (member_id);
				}
			}
			parties.Insert (owner_id, validated_party_ids);
		}

		return parties;
	}

	ref array<DayZPlayer> GetPartyPlayers (string id) {
		ref map<string, DayZPlayer> id_map = new ref map<string, DayZPlayer> ();
		ref array<Man> game_players = new array<Man>;
		GetGame ().GetPlayers (game_players);

		foreach (Man man : game_players) {
			DayZPlayer player = DayZPlayer.Cast (man);
			if (player && player.GetIdentity ()) {
				id_map.Insert (player.GetIdentity ().GetId (), player);
			}
		}

		ref array<DayZPlayer> players = new ref array<DayZPlayer>;
		ref set<string> member_ids = GetParties ().Get (id);
		if (member_ids) {
			for (int i = 0; i < member_ids.Count (); i++) {
				string member_id = member_ids.Get (i);
				if (id_map.Contains (member_id)) {
					DayZPlayer plr = DayZPlayer.Cast (id_map.Get (member_id));
					if (plr) {
						players.Insert (plr);
					}
				}
			}
		}
		return players;
	}

	private ref map<string, vector> GetPositions () {
		auto positions = new ref map<string, vector> ();

		ref array<Man> players = new array<Man>;
		GetGame ().GetPlayers (players);

		foreach (Man man : players) {
			DayZPlayer player = DayZPlayer.Cast (man);
			if (player && player.GetIdentity ()) {
				positions.Insert (player.GetIdentity ().GetId (), player.GetPosition ());
			}
		}

		return positions;
	}

	private ref map<string, float> GetHealths () {
		auto healths = new ref map<string, float> ();

		ref array<Man> players = new array<Man>;
		GetGame ().GetPlayers (players);

		foreach (Man man : players) {
			DayZPlayer player = DayZPlayer.Cast (man);
			if (player && player.GetIdentity ()) {
				healths.Insert (player.GetIdentity ().GetId (), player.GetHealth ("", ""));
			}
		}

		return healths;
	}

	private void SendInfo () {
		if (canSendInfo) {
			auto id_map = new ref map<string, DayZPlayer> ();

			ref array<Man> players = new array<Man>;
			GetGame ().GetPlayers (players);

			foreach (Man man : players) {
				DayZPlayer player = DayZPlayer.Cast (man);
				if (player && player.GetIdentity ()) {
					id_map.Insert (player.GetIdentity ().GetId (), player);
				}
			}

			GetGame ().GetCallQueue (CALL_CATEGORY_SYSTEM).Call (this.SendPartyInfo, id_map);
			GetGame ().GetCallQueue (CALL_CATEGORY_SYSTEM).Call (this.SendPlayersInfo, id_map);

			canSendInfo = false;
		}
	}

	private void SendPartyInfo (ref map<string, DayZPlayer> id_map) {

		auto positions = GetPositions ();
		auto server_healths = GetHealths ();
		auto parties = GetParties ();

		if (SchanaPartyUtils.WillLog (SchanaPartyUtils.DEBUG)) {
			string result;
			JsonSerializer ().WriteToString (positions, false, result);
			SchanaPartyUtils.Debug ("Positions " + result);
			JsonSerializer ().WriteToString (server_healths, false, result);
			SchanaPartyUtils.Debug ("Healths " + result);
			JsonSerializer ().WriteToString (parties, false, result);
			SchanaPartyUtils.Debug ("Parties " + result);
		}

		int maxPartySize = GetSchanaPartyServerSettings ().GetMaxPartySize ();
		int SendDelay = 1;
		foreach (auto id, auto party_ids : parties) {
			SendDelay++; //To help performance to devide up when the parties are all sent
			SchanaPartyUtils.Trace ("SendInfo Begin " + id);
			if (!positions.Contains (id)) {
				configurations.Remove (id);
			} else {
				DayZPlayer player = DayZPlayer.Cast (id_map.Get (id));
				if (player) {
					GetGame ().GetCallQueue (CALL_CATEGORY_SYSTEM).CallLater (this.SendPartyInfoToPlayer, SendDelay, false, id, party_ids, maxPartySize, positions, server_healths, player);
				}
			}
			SchanaPartyUtils.Trace ("SendInfo End " + id);
		}
	}

	private void SendPartyInfoToPlayer (string id, ref set<string> party_ids, int maxPartySize, ref map<string, vector> positions, ref map<string, float> server_healths, DayZPlayer player) {
		auto ids = new ref array<string>;
		auto locations = new ref array<vector>;
		auto healths = new ref array<float>;
		foreach (string party_id : party_ids) {
			if (positions.Contains (party_id) && (maxPartySize < 0 || ids.Count () < maxPartySize)) {
				ids.Insert (party_id);
				locations.Insert (positions.Get (party_id));
				healths.Insert (server_healths.Get (party_id));
			}
		}
		auto info = new ref Param3<ref array<string>, ref array<vector>, ref array<float>> (ids, locations, healths);

		if (SchanaPartyUtils.WillLog (SchanaPartyUtils.INFO)) {
			string result;
			JsonSerializer ().WriteToString (info, false, result);
			SchanaPartyUtils.Info ("SendInfo to " + id + " " + result);
		}

		if (player && player.GetIdentity ()) {
			GetRPCManager ().SendRPC ("SchanaModParty", "ClientUpdatePartyInfoRPC", info, false, player.GetIdentity ());
		} else {
			SchanaPartyUtils.Warn ("SendInfo failed to " + id);
		}
	}

	private void SendPlayersInfo (ref map<string, DayZPlayer> id_map) {
		auto all_player_ids = new ref array<string>;
		auto all_player_names = new ref array<string>;
		foreach (auto player_id, auto player_base_player : id_map) {
			all_player_ids.Insert (player_id);
			all_player_names.Insert (player_base_player.GetIdentity ().GetName ());
		}

		auto all_player_info = new ref Param2<ref array<string>, ref array<string>> (all_player_ids, all_player_names);

		if (SchanaPartyUtils.WillLog (SchanaPartyUtils.DEBUG)) {
			string result;
			JsonSerializer ().WriteToString (all_player_info, false, result);
			SchanaPartyUtils.Debug ("SendPlayers " + result);
		}

		GetRPCManager ().SendRPC ("SchanaModParty", "ClientUpdatePlayersInfoRPC", all_player_info);
	}
}

static ref SchanaPartyManagerServer g_SchanaPartyManagerServer;
static ref SchanaPartyManagerServer GetSchanaPartyManagerServer () {
	if (g_Game.IsServer () && !g_SchanaPartyManagerServer) {
		g_SchanaPartyManagerServer = new SchanaPartyManagerServer;
	}
	return g_SchanaPartyManagerServer;
}