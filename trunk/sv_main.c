/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// sv_main.c -- server main program

#include "quakedef.h"

server_t	sv;
server_static_t	svs;

// Determine size of arrays for localmodels so that they can hold all the
// strings given in SV_Init
#define STRINGIFY(s) #s
#define LOCALMODELS_STRING_SIZE (sizeof("*" STRINGIFY(MAX_MODELS)) / sizeof(char))
char		localmodels[MAX_MODELS][LOCALMODELS_STRING_SIZE];	// inline model names for precache

int			sv_protocol = PROTOCOL_NETQUAKE;

extern qboolean	pr_alpha_supported; //johnfitz 

//============================================================================

/*
===============
SV_Init
===============
*/
void SV_Init (void)
{
	int	i;
	const char *p;

	sv.edicts = NULL; // ericw -- sv.edicts switched to use malloc() 

	Cvar_Register (&sv_maxvelocity);
	Cvar_Register (&sv_gravity);
	Cvar_Register (&sv_friction);
	Cvar_Register (&sv_edgefriction);
	Cvar_Register (&sv_stopspeed);
	Cvar_Register (&sv_maxspeed);
	Cvar_Register (&sv_accelerate);
	Cvar_Register (&sv_idealpitchscale);
	Cvar_Register (&sv_aim);
	Cvar_Register (&sv_nostep);

	for (i=0 ; i<MAX_MODELS ; i++)
		sprintf (localmodels[i], "*%i", i);

	i = COM_CheckParm("-protocol");
	if (i && i < com_argc - 1)
	{
		sv_protocol = atoi(com_argv[i + 1]);
		switch (sv_protocol)
		{
		case PROTOCOL_NETQUAKE:
			p = "NetQuake";
			break;
		case PROTOCOL_FITZQUAKE:
			p = "FitzQuake";
			break;
		case PROTOCOL_RMQ:
			p = "RMQ";
			break;
		default:
			Sys_Error("Bad protocol version request %i. Accepted values: %i, %i, %i.",
				sv_protocol, PROTOCOL_NETQUAKE, PROTOCOL_FITZQUAKE, PROTOCOL_RMQ);
			return; /* silence compiler */
		}
	}
	else
	{
		sv_protocol = PROTOCOL_NETQUAKE;
		p = "NetQuake";
	}
	Sys_Printf("Server using protocol %i (%s)\n", sv_protocol, p);
}

/*
=============================================================================

EVENT MESSAGES

=============================================================================
*/

/*  
==================
SV_StartParticle

Make sure the event gets sent to all clients
==================
*/
void SV_StartParticle (vec3_t org, vec3_t dir, int color, int count)
{
	int	i, v;

	if (sv.datagram.cursize > MAX_DATAGRAM - 16)
		return;	

	MSG_WriteByte (&sv.datagram, svc_particle);
	MSG_WriteCoord (&sv.datagram, org[0], sv.protocolflags);
	MSG_WriteCoord (&sv.datagram, org[1], sv.protocolflags);
	MSG_WriteCoord (&sv.datagram, org[2], sv.protocolflags);
	for (i=0 ; i<3 ; i++)
	{
		v = dir[i]*16;
		if (v > 127)
			v = 127;
		else if (v < -128)
			v = -128;
		MSG_WriteChar (&sv.datagram, v);
	}
	MSG_WriteByte (&sv.datagram, count);
	MSG_WriteByte (&sv.datagram, color);
}           

/*  
==================
SV_StartSound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
allready running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.  (max 4 attenuation)
==================
*/  
void SV_StartSound (edict_t *entity, int channel, char *sample, int volume, float attenuation)
{       
	int	sound_num, field_mask, i, ent;

	if (volume < 0 || volume > 255)
		Sys_Error ("SV_StartSound: volume = %i", volume);

	if (attenuation < 0 || attenuation > 4)
		Sys_Error ("SV_StartSound: attenuation = %f", attenuation);

	if (channel < 0 || channel > 7)
		Sys_Error ("SV_StartSound: channel = %i", channel);

	if (sv.datagram.cursize > MAX_DATAGRAM - 16)
		return;	

// find precache number for sound
	for (sound_num = 1 ; sound_num < MAX_SOUNDS && sv.sound_precache[sound_num] ; sound_num++)
		if (!strcmp(sample, sv.sound_precache[sound_num]))
			break;

	if (sound_num == MAX_SOUNDS || !sv.sound_precache[sound_num])
	{
		Con_Printf ("SV_StartSound: %s not precached\n", sample);
		return;
	}

	ent = NUM_FOR_EDICT(entity);

	field_mask = 0;
	if (volume != DEFAULT_SOUND_PACKET_VOLUME)
		field_mask |= SND_VOLUME;
	if (attenuation != DEFAULT_SOUND_PACKET_ATTENUATION)
		field_mask |= SND_ATTENUATION;

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (ent >= 8192)
	{
		if (sv.protocol == PROTOCOL_NETQUAKE)
			return; //don't send any info protocol can't support
		else
			field_mask |= SND_LARGEENTITY;
	}
	if (sound_num >= 256 || channel >= 8)
	{
		if (sv.protocol == PROTOCOL_NETQUAKE)
			return; //don't send any info protocol can't support
		else
			field_mask |= SND_LARGESOUND;
	}
	//johnfitz

// directed messages go only to the entity the are targeted on
	MSG_WriteByte (&sv.datagram, svc_sound);
	MSG_WriteByte (&sv.datagram, field_mask);
	if (field_mask & SND_VOLUME)
		MSG_WriteByte (&sv.datagram, volume);
	if (field_mask & SND_ATTENUATION)
		MSG_WriteByte (&sv.datagram, attenuation * 64);

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (field_mask & SND_LARGEENTITY)
	{
		MSG_WriteShort(&sv.datagram, ent);
		MSG_WriteByte(&sv.datagram, channel);
	}
	else
		MSG_WriteShort(&sv.datagram, (ent << 3) | channel);
	if (field_mask & SND_LARGESOUND)
		MSG_WriteShort(&sv.datagram, sound_num);
	else
		MSG_WriteByte(&sv.datagram, sound_num);
	//johnfitz
	for (i=0 ; i<3 ; i++)
		MSG_WriteCoord (&sv.datagram, entity->v.origin[i] + 0.5 * (entity->v.mins[i]+entity->v.maxs[i]), sv.protocolflags);
}           

/*
==============================================================================

CLIENT SPAWNING

==============================================================================
*/

/*
================
SV_SendServerinfo

Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each server load.
================
*/
void SV_SendServerinfo (client_t *client)
{
	char	**s, message[2048];
	int		i; //johnfitz

	MSG_WriteByte (&client->message, svc_print);
	sprintf (message, "%c\nVERSION %4.2f SERVER (%i CRC)", 2, SERVER_VERSION, pr_crc);
	MSG_WriteString (&client->message, message);

	MSG_WriteByte (&client->message, svc_serverinfo);
	MSG_WriteLong(&client->message, sv.protocol); //johnfitz -- sv.protocol instead of PROTOCOL_VERSION

	if (sv.protocol == PROTOCOL_RMQ)
	{
		// mh - now send protocol flags so that the client knows the protocol features to expect
		MSG_WriteLong(&client->message, sv.protocolflags);
	}

	MSG_WriteByte (&client->message, svs.maxclients);

	if (!coop.value && deathmatch.value)
		MSG_WriteByte (&client->message, GAME_DEATHMATCH);
	else
		MSG_WriteByte (&client->message, GAME_COOP);

	MSG_WriteString (&client->message, PR_GetString(sv.edicts->v.message));

	//johnfitz -- only send the first 256 model and sound precaches if protocol is 15
	for (i = 0, s = sv.model_precache + 1; *s; s++, i++)
		if (sv.protocol != PROTOCOL_NETQUAKE || i < 256)
			MSG_WriteString(&client->message, *s);
	MSG_WriteByte(&client->message, 0);

	for (i = 0, s = sv.sound_precache + 1; *s; s++, i++)
		if (sv.protocol != PROTOCOL_NETQUAKE || i < 256)
			MSG_WriteString(&client->message, *s);
	MSG_WriteByte(&client->message, 0);
	//johnfitz 

// send music
	MSG_WriteByte (&client->message, svc_cdtrack);
	MSG_WriteByte (&client->message, sv.edicts->v.sounds);
	MSG_WriteByte (&client->message, sv.edicts->v.sounds);

// set view	
	MSG_WriteByte (&client->message, svc_setview);
	MSG_WriteShort (&client->message, NUM_FOR_EDICT(client->edict));

// Sphere -- if server is paused we also have to inform the client about that,
// but only if it was issued by a client through Host_Pause_f (i.e. both
// sv.paused and cl.paused are true)
	MSG_WriteByte (&client->message, svc_setpause);
	MSG_WriteByte (&client->message, sv.paused && cl.paused);

	MSG_WriteByte (&client->message, svc_signonnum);
	MSG_WriteByte (&client->message, 1);

	client->sendsignon = true;
	client->spawned = false;		// need prespawn, spawn, etc
}

/*
================
SV_ConnectClient

Initializes a client_t for a new net connection. This will only be called
once for a player each game, not once for each level change.
================
*/
void SV_ConnectClient (int clientnum)
{
	int		i, edictnum;
	float		spawn_parms[NUM_SPAWN_PARMS];
	edict_t		*ent;
	client_t	*client;
	struct qsocket_s *netconnection;

	client = svs.clients + clientnum;

	if (client->netconnection->mod == MOD_JOEQUAKE)
		Con_DPrintf ("JoeQuake Client %s connected\n", client->netconnection->address);
	else
		Con_DPrintf ("Client %s connected\n", client->netconnection->address);

	edictnum = clientnum + 1;

	ent = EDICT_NUM(edictnum);
	
// set up the client_t
	netconnection = client->netconnection;
	
	if (sv.loadgame)
		memcpy (spawn_parms, client->spawn_parms, sizeof(spawn_parms));
	memset (client, 0, sizeof(*client));
	client->netconnection = netconnection;

	strcpy (client->name, "unconnected");
	client->active = true;
	client->spawned = false;
	client->edict = ent;
	client->message.data = client->msgbuf;
	client->message.maxsize = sizeof(client->msgbuf);
	client->message.allowoverflow = true;		// we can catch it

	if (sv.loadgame)
	{
		memcpy (client->spawn_parms, spawn_parms, sizeof(spawn_parms));
	}
	else
	{
	// call the progs to get default spawn parms for the new client
		PR_ExecuteProgram (pr_global_struct->SetNewParms);
		for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
			client->spawn_parms[i] = (&pr_global_struct->parm1)[i];
	}

	SV_SendServerinfo (client);
}

/*
===================
SV_CheckForNewClients
===================
*/
void SV_CheckForNewClients (void)
{
	struct qsocket_s	*ret;
	int			i;

// check for new connections
	while (1)
	{
		if (!(ret = NET_CheckNewConnections()))
			break;

	// init a new client structure
		for (i=0 ; i<svs.maxclients ; i++)
			if (!svs.clients[i].active)
				break;
		if (i == svs.maxclients)
			Sys_Error ("SV_CheckForNewClients: no free clients");

		svs.clients[i].netconnection = ret;
		SV_ConnectClient (i);

		net_activeconnections++;
	}
}

/*
===============================================================================

FRAME UPDATES

===============================================================================
*/

/*
==================
SV_ClearDatagram
==================
*/
void SV_ClearDatagram (void)
{
	SZ_Clear (&sv.datagram);
}

/*
=============================================================================

The PVS must include a small area around the client to allow head bobbing
or other small motion on the client side.  Otherwise, a bob might cause an
entity that should be visible to not show up, especially when the bob
crosses a waterline.

=============================================================================
*/

static int	fatbytes;
static byte	*fatpvs;
static int	fatpvs_capacity;

void SV_AddToFatPVS (vec3_t org, mnode_t *node, model_t *worldmodel) //johnfitz -- added worldmodel as a parameter 
{
	int		i;
	byte		*pvs;
	mplane_t	*plane;
	float		d;

	while (1)
	{
	// if this is a leaf, accumulate the pvs bits
		if (node->contents < 0)
		{
			if (node->contents != CONTENTS_SOLID)
			{
				pvs = Mod_LeafPVS ((mleaf_t *)node, worldmodel); //johnfitz -- worldmodel as a parameter 
				for (i=0 ; i<fatbytes ; i++)
					fatpvs[i] |= pvs[i];
			}
			return;
		}
	
		plane = node->plane;
		d = PlaneDiff (org, plane);
		if (d > 8)
			node = node->children[0];
		else if (d < -8)
			node = node->children[1];
		else
		{	// go down both
			SV_AddToFatPVS (org, node->children[0], worldmodel); //johnfitz -- worldmodel as a parameter 
			node = node->children[1];
		}
	}
}

/*
=============
SV_FatPVS

Calculates a PVS that is the inclusive or of all leafs within 8 pixels of the
given point.
=============
*/
byte *SV_FatPVS(vec3_t org, model_t *worldmodel) //johnfitz -- added worldmodel as a parameter
{
	fatbytes = (worldmodel->numleafs + 7) >> 3; // ericw -- was +31, assumed to be a bug/typo
	if (fatpvs == NULL || fatbytes > fatpvs_capacity)
	{
		fatpvs_capacity = fatbytes;
		fatpvs = (byte *)Q_realloc(fatpvs, fatpvs_capacity);
		if (!fatpvs)
			Sys_Error("SV_FatPVS: realloc() failed on %d bytes", fatpvs_capacity);
	}

	memset(fatpvs, 0, fatbytes);
	SV_AddToFatPVS(org, worldmodel->nodes, worldmodel); //johnfitz -- worldmodel as a parameter
	return fatpvs;
}

//=============================================================================

/*
=============
SV_WriteEntitiesToClient
=============
*/
void SV_WriteEntitiesToClient (edict_t *clent, sizebuf_t *msg, qboolean nomap)
{
	int		e, i, bits;
	float	miss;
	byte	*pvs;
	vec3_t	org;
	edict_t	*ent;
#ifdef GLQUAKE
	float	alpha, fullbright;
	eval_t  *val;
#endif

// find the client's PVS
	VectorAdd (clent->v.origin, clent->v.view_ofs, org);
	pvs = SV_FatPVS (org, sv.worldmodel);

// send over all entities (excpet the client) that touch the pvs
	ent = NEXT_EDICT(sv.edicts);
	for (e = 1; e < sv.num_edicts; e++, ent = NEXT_EDICT(ent))
	{
		// ignore if not touching a PV leaf
		if (ent != clent)	// clent is ALWAYS sent
		{
			// ignore ents without visible models
			if (!ent->v.modelindex || !PR_GetString(ent->v.model)[0])
				continue;

			//johnfitz -- don't send model>255 entities if protocol is 15
			if (sv.protocol == PROTOCOL_NETQUAKE && (int)ent->v.modelindex & 0xFF00)
				continue;

			// ignore if not touching a PV leaf
			for (i = 0; i < ent->num_leafs; i++)
				if (pvs[ent->leafnums[i] >> 3] & (1 << (ent->leafnums[i] & 7)))
					break;

			// ericw -- added ent->num_leafs < MAX_ENT_LEAFS condition.
			//
			// if ent->num_leafs == MAX_ENT_LEAFS, the ent is visible from too many leafs
			// for us to say whether it's in the PVS, so don't try to vis cull it.
			// this commonly happens with rotators, because they often have huge bboxes
			// spanning the entire map, or really tall lifts, etc.
			if (i == ent->num_leafs && ent->num_leafs < MAX_ENT_LEAFS)
				continue;		// not visible

			// joe, from ProQuake: don't send updates if the client doesn't have the map
			if (nomap)
				continue;
		}

		//johnfitz -- max size for protocol 15 is 18 bytes, not 16 as originally
		//assumed here.  And, for protocol 85 the max size is actually 24 bytes.
		if (msg->cursize + 24 > msg->maxsize)
		{
			Con_Printf("Packet overflow!\n");
			return;
		}

		// send an update
		bits = 0;

		for (i = 0; i < 3; i++)
		{
			miss = ent->v.origin[i] - ent->baseline.origin[i];
			if (miss < -0.1 || miss > 0.1)
				bits |= U_ORIGIN1 << i;
		}

		if (ent->v.angles[0] != ent->baseline.angles[0])
			bits |= U_ANGLE1;

		if (ent->v.angles[1] != ent->baseline.angles[1])
			bits |= U_ANGLE2;

		if (ent->v.angles[2] != ent->baseline.angles[2])
			bits |= U_ANGLE3;

		if (ent->v.movetype == MOVETYPE_STEP)
			bits |= U_NOLERP;	// don't mess up the step animation

		if (ent->baseline.colormap != ent->v.colormap)
			bits |= U_COLORMAP;

		if (ent->baseline.skin != ent->v.skin)
			bits |= U_SKIN;

		if (ent->baseline.frame != ent->v.frame)
			bits |= U_FRAME;

		if (ent->baseline.effects != ent->v.effects)
			bits |= U_EFFECTS;

		if (ent->baseline.modelindex != ent->v.modelindex)
			bits |= U_MODEL;

		if ((val = GETEDICTFIELDVALUE(ent, eval_alpha)))
		{
			alpha = val->_float;
			if (pr_alpha_supported)
				ent->alpha = ENTALPHA_ENCODE(val->_float);
		}
		else
			alpha = ENTALPHA_DECODE(ent->alpha);

		//don't send invisible entities unless they have effects
		if (ent->alpha == ENTALPHA_ZERO && !ent->v.effects)
			continue;
		//johnfitz

		if (sv.protocol == PROTOCOL_NETQUAKE)
		{
#ifdef GLQUAKE
			// nehahra: model alpha
			if ((val = GETEDICTFIELDVALUE(ent, eval_fullbright)))
				fullbright = val->_float;
			else
				fullbright = 0;

			if ((alpha < 1 && alpha > 0) || fullbright)
				bits |= U_TRANS;
#endif
		}
		else //johnfitz -- PROTOCOL_FITZQUAKE
		{
			if (ent->baseline.alpha != ent->alpha)
				bits |= U_ALPHA;

			if (bits & U_FRAME && (int)ent->v.frame & 0xFF00)
				bits |= U_FRAME2;

			if (bits & U_MODEL && (int)ent->v.modelindex & 0xFF00)
				bits |= U_MODEL2;

			if (ent->sendinterval)
				bits |= U_LERPFINISH;

			if (bits >= 65536)
				bits |= U_EXTEND1;

			if (bits >= 16777216)
				bits |= U_EXTEND2;
		}
		//johnfitz

		if (e >= 256)
			bits |= U_LONGENTITY;

		if (bits >= 256)
			bits |= U_MOREBITS;

		// write the message
		MSG_WriteByte(msg, bits | U_SIGNAL);

		if (bits & U_MOREBITS)
			MSG_WriteByte(msg, bits >> 8);

		//johnfitz -- PROTOCOL_FITZQUAKE
		if (cl.protocol != PROTOCOL_NETQUAKE)
		{
			if (bits & U_EXTEND1)
				MSG_WriteByte(msg, bits >> 16);
			if (bits & U_EXTEND2)
				MSG_WriteByte(msg, bits >> 24);
		}
		//johnfitz

		if (bits & U_LONGENTITY)
			MSG_WriteShort(msg, e);
		else
			MSG_WriteByte(msg, e);

		if (bits & U_MODEL)
			MSG_WriteByte(msg, ent->v.modelindex);
		if (bits & U_FRAME)
			MSG_WriteByte(msg, ent->v.frame);
		if (bits & U_COLORMAP)
			MSG_WriteByte(msg, ent->v.colormap);
		if (bits & U_SKIN)
			MSG_WriteByte(msg, ent->v.skin);
		if (bits & U_EFFECTS)
			MSG_WriteByte(msg, ent->v.effects);
		if (bits & U_ORIGIN1)
			MSG_WriteCoord(msg, ent->v.origin[0], sv.protocolflags);
		if (bits & U_ANGLE1)
			MSG_WriteAngle(msg, ent->v.angles[0], sv.protocolflags);
		if (bits & U_ORIGIN2)
			MSG_WriteCoord(msg, ent->v.origin[1], sv.protocolflags);
		if (bits & U_ANGLE2)
			MSG_WriteAngle(msg, ent->v.angles[1], sv.protocolflags);
		if (bits & U_ORIGIN3)
			MSG_WriteCoord(msg, ent->v.origin[2], sv.protocolflags);
		if (bits & U_ANGLE3)
			MSG_WriteAngle(msg, ent->v.angles[2], sv.protocolflags);

		if (sv.protocol == PROTOCOL_NETQUAKE)
		{
#ifdef GLQUAKE
			if (bits & U_TRANS)
			{
				MSG_WriteFloat(msg, 2);
				MSG_WriteFloat(msg, alpha);
				MSG_WriteFloat(msg, fullbright);
			}
#endif
		}

		//johnfitz -- PROTOCOL_FITZQUAKE
		if (bits & U_ALPHA)
			MSG_WriteByte(msg, ent->alpha);
		if (bits & U_FRAME2)
			MSG_WriteByte(msg, (int)ent->v.frame >> 8);
		if (bits & U_MODEL2)
			MSG_WriteByte(msg, (int)ent->v.modelindex >> 8);
		if (bits & U_LERPFINISH)
			MSG_WriteByte(msg, (byte)(Q_rint((ent->v.nextthink - sv.time) * 255)));
		//johnfitz
	}
}

/*
=============
SV_CleanupEnts
=============
*/
void SV_CleanupEnts (void)
{
	int	e;
	edict_t	*ent;
	
	ent = NEXT_EDICT(sv.edicts);
	for (e = 1 ; e < sv.num_edicts ; e++, ent = NEXT_EDICT(ent))
		ent->v.effects = (int)ent->v.effects & ~EF_MUZZLEFLASH;
}

/*
==================
SV_WriteClientdataToMessage
==================
*/
void SV_WriteClientdataToMessage (edict_t *ent, sizebuf_t *msg)
{
	int	bits, i, items;
	edict_t	*other;
	eval_t	*val;

// send a damage message
	if (ent->v.dmg_take || ent->v.dmg_save)
	{
		other = PROG_TO_EDICT(ent->v.dmg_inflictor);
		MSG_WriteByte (msg, svc_damage);
		MSG_WriteByte (msg, ent->v.dmg_save);
		MSG_WriteByte (msg, ent->v.dmg_take);
		for (i=0 ; i<3 ; i++)
			MSG_WriteCoord (msg, other->v.origin[i] + 0.5 * (other->v.mins[i] + other->v.maxs[i]), sv.protocolflags);

		ent->v.dmg_take = 0;
		ent->v.dmg_save = 0;
	}

// send the current viewpos offset from the view entity
	SV_SetIdealPitch ();		// how much to look up / down ideally

// a fixangle might get lost in a dropped packet. Oh well.
	if (ent->v.fixangle)
	{
		MSG_WriteByte (msg, svc_setangle);
		for (i=0 ; i<3 ; i++)
			MSG_WriteAngle (msg, ent->v.angles[i], sv.protocolflags);
		ent->v.fixangle = 0;
	}

	bits = 0;

	if (ent->v.view_ofs[2] != DEFAULT_VIEWHEIGHT)
		bits |= SU_VIEWHEIGHT;

	if (ent->v.idealpitch)
		bits |= SU_IDEALPITCH;

// stuff the sigil bits into the high bits of items for sbar, or else mix in items2
	if ((val = GETEDICTFIELDVALUE(ent, eval_items2)))
		items = (int)ent->v.items | ((int)val->_float << 23);
	else
		items = (int)ent->v.items | ((int)pr_global_struct->serverflags << 28);

	bits |= SU_ITEMS;

	if ((int)ent->v.flags & FL_ONGROUND)
		bits |= SU_ONGROUND;

	if (ent->v.waterlevel >= 2)
		bits |= SU_INWATER;

	for (i=0 ; i<3 ; i++)
	{
		if (ent->v.punchangle[i])
			bits |= (SU_PUNCH1 << i);
		if (ent->v.velocity[i])
			bits |= (SU_VELOCITY1 << i);
	}

	if (ent->v.weaponframe)
		bits |= SU_WEAPONFRAME;

	if (ent->v.armorvalue)
		bits |= SU_ARMOR;

//	if (ent->v.weapon)
		bits |= SU_WEAPON;

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (sv.protocol != PROTOCOL_NETQUAKE)
	{
		if (bits & SU_WEAPON && SV_ModelIndex(PR_GetString(ent->v.weaponmodel)) & 0xFF00)
			bits |= SU_WEAPON2;
			
		if ((int)ent->v.armorvalue & 0xFF00) 
			bits |= SU_ARMOR2;
			
		if ((int)ent->v.currentammo & 0xFF00) 
			bits |= SU_AMMO2;
			
		if ((int)ent->v.ammo_shells & 0xFF00) 
			bits |= SU_SHELLS2;
			
		if ((int)ent->v.ammo_nails & 0xFF00) 
			bits |= SU_NAILS2;
			
		if ((int)ent->v.ammo_rockets & 0xFF00) 
			bits |= SU_ROCKETS2;
			
		if ((int)ent->v.ammo_cells & 0xFF00) 
			bits |= SU_CELLS2;
			
		if (bits & SU_WEAPONFRAME && (int)ent->v.weaponframe & 0xFF00) 
			bits |= SU_WEAPONFRAME2;
			
		if (bits & SU_WEAPON && ent->alpha != ENTALPHA_DEFAULT) 
			bits |= SU_WEAPONALPHA; //for now, weaponalpha = client entity alpha
			
		if (bits >= 65536) 
			bits |= SU_EXTEND1;
			
		if (bits >= 16777216) 
			bits |= SU_EXTEND2;
	}
	//johnfitz

// send the data
	MSG_WriteByte (msg, svc_clientdata);
	MSG_WriteShort (msg, bits);

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (bits & SU_EXTEND1) 
		MSG_WriteByte(msg, bits >> 16);
	if (bits & SU_EXTEND2) 
		MSG_WriteByte(msg, bits >> 24);
	//johnfitz

	if (bits & SU_VIEWHEIGHT)
		MSG_WriteChar (msg, ent->v.view_ofs[2]);

	if (bits & SU_IDEALPITCH)
		MSG_WriteChar (msg, ent->v.idealpitch);

	for (i=0 ; i<3 ; i++)
	{
		if (bits & (SU_PUNCH1 << i))
			MSG_WriteChar (msg, ent->v.punchangle[i]);
		if (bits & (SU_VELOCITY1 << i))
			MSG_WriteChar (msg, ent->v.velocity[i]/16);
	}

// [always sent]	if (bits & SU_ITEMS)
	MSG_WriteLong (msg, items);

	if (bits & SU_WEAPONFRAME)
		MSG_WriteByte (msg, ent->v.weaponframe);
	if (bits & SU_ARMOR)
		MSG_WriteByte (msg, ent->v.armorvalue);
	if (bits & SU_WEAPON)
		MSG_WriteByte (msg, SV_ModelIndex(PR_GetString(ent->v.weaponmodel)));
	
	MSG_WriteShort (msg, ent->v.health);
	MSG_WriteByte (msg, ent->v.currentammo);
	MSG_WriteByte (msg, ent->v.ammo_shells);
	MSG_WriteByte (msg, ent->v.ammo_nails);
	MSG_WriteByte (msg, ent->v.ammo_rockets);
	MSG_WriteByte (msg, ent->v.ammo_cells);

	if (hipnotic || rogue)
	{
		for (i=0 ; i<32 ; i++)
		{
			if (((int)ent->v.weapon) & (1<<i))
			{
				MSG_WriteByte (msg, i);
				break;
			}
		}
	}
	else
	{
		MSG_WriteByte (msg, ent->v.weapon);
	}

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (bits & SU_WEAPON2)
		MSG_WriteByte(msg, SV_ModelIndex(PR_GetString(ent->v.weaponmodel)) >> 8);
	if (bits & SU_ARMOR2)
		MSG_WriteByte(msg, (int)ent->v.armorvalue >> 8);
	if (bits & SU_AMMO2)
		MSG_WriteByte(msg, (int)ent->v.currentammo >> 8);
	if (bits & SU_SHELLS2)
		MSG_WriteByte(msg, (int)ent->v.ammo_shells >> 8);
	if (bits & SU_NAILS2)
		MSG_WriteByte(msg, (int)ent->v.ammo_nails >> 8);
	if (bits & SU_ROCKETS2)
		MSG_WriteByte(msg, (int)ent->v.ammo_rockets >> 8);
	if (bits & SU_CELLS2)
		MSG_WriteByte(msg, (int)ent->v.ammo_cells >> 8);
	if (bits & SU_WEAPONFRAME2)
		MSG_WriteByte(msg, (int)ent->v.weaponframe >> 8);
	if (bits & SU_WEAPONALPHA)
		MSG_WriteByte(msg, ent->alpha); //for now, weaponalpha = client entity alpha
	//johnfitz
}

/*
=======================
SV_SendClientDatagram
=======================
*/
qboolean SV_SendClientDatagram (client_t *client)
{
	byte		buf[MAX_DATAGRAM];
	sizebuf_t	msg;

	msg.data = buf;
	msg.maxsize = sizeof(buf);
	msg.cursize = 0;

	//johnfitz -- if client is nonlocal, use smaller max size so packets aren't fragmented
	if (strcmp(client->netconnection->address, "LOCAL") != 0)
		msg.maxsize = DATAGRAM_MTU;
	//johnfitz

	MSG_WriteByte (&msg, svc_time);
	MSG_WriteFloat (&msg, sv.time);

// add the client specific data to the datagram
	SV_WriteClientdataToMessage (client->edict, &msg);

	SV_WriteEntitiesToClient (client->edict, &msg, client->nomap);

// copy the server datagram if there is space
	if (msg.cursize + sv.datagram.cursize < msg.maxsize)
		SZ_Write (&msg, sv.datagram.data, sv.datagram.cursize);

// send the datagram
	if (NET_SendUnreliableMessage (client->netconnection, &msg) == -1)
	{
		SV_DropClient (true);	// if the message couldn't send, kick off
		return false;
	}

	return true;
}

/*
=======================
SV_UpdateToReliableMessages
=======================
*/
void SV_UpdateToReliableMessages (void)
{
	int		i, j;
	client_t	*client;

// check for changes to be sent over the reliable streams
	for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
	{
		if (host_client->old_frags != (int)host_client->edict->v.frags)
		{
			for (j = 0, client = svs.clients ; j < svs.maxclients ; j++, client++)
			{
				if (!client->active)
					continue;
				MSG_WriteByte (&client->message, svc_updatefrags);
				MSG_WriteByte (&client->message, i);
				MSG_WriteShort (&client->message, host_client->edict->v.frags);
			}

			host_client->old_frags = host_client->edict->v.frags;
		}
	}
	
	for (j=0, client = svs.clients ; j<svs.maxclients ; j++, client++)
	{
		if (!client->active)
			continue;
		SZ_Write (&client->message, sv.reliable_datagram.data, sv.reliable_datagram.cursize);
	}

	SZ_Clear (&sv.reliable_datagram);
}


/*
=======================
SV_SendNop

Send a nop message without trashing or sending the accumulated client
message buffer
=======================
*/
void SV_SendNop (client_t *client)
{
	sizebuf_t	msg;
	byte		buf[4];
	
	msg.data = buf;
	msg.maxsize = sizeof(buf);
	msg.cursize = 0;

	MSG_WriteChar (&msg, svc_nop);

	if (NET_SendUnreliableMessage (client->netconnection, &msg) == -1)
		SV_DropClient (true);	// if the message couldn't send, kick off
	client->last_message = realtime;
}

/*
=======================
SV_SendClientMessages
=======================
*/
void SV_SendClientMessages (void)
{
	int		i;
	
// update frags, names, etc
	SV_UpdateToReliableMessages ();

// build individual updates
	for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
	{
		if (!host_client->active)
			continue;

		if (host_client->spawned)
		{
			if (!SV_SendClientDatagram (host_client))
				continue;
		}
		else
		{
		// the player isn't totally in the game yet
		// send small keepalive messages if too much time has passed
		// send a full message when the next signon stage has been requested
		// some other message data (name changes, etc) may accumulate 
		// between signon stages
			if (!host_client->sendsignon)
			{
				if (realtime - host_client->last_message > 5)
					SV_SendNop (host_client);
				continue;	// don't send out non-signon messages
			}
		}

		// joe: NAT fix from ProQuake
		if (host_client->netconnection->net_wait)
			continue;

		// check for an overflowed message. Should only happen on a very
		// fucked up connection that backs up a lot, then changes level
		if (host_client->message.overflowed)
		{
			SV_DropClient (true);
			host_client->message.overflowed = false;
			continue;
		}
			
		if (host_client->message.cursize || host_client->dropasap)
		{
			if (!NET_CanSendMessage(host_client->netconnection))
				continue;

			if (host_client->dropasap)
			{
				SV_DropClient (false);	// went to another level
			}
			else
			{
				if (NET_SendMessage(host_client->netconnection, &host_client->message) == -1)
					SV_DropClient (true);	// if the message couldn't send, kick off
				SZ_Clear (&host_client->message);
				host_client->last_message = realtime;
				host_client->sendsignon = false;
			}
		}
	}
	
// clear muzzle flashes
	SV_CleanupEnts ();
}

/*
==============================================================================

SERVER SPAWNING

==============================================================================
*/

/*
================
CheckModel
================
*/
unsigned CheckModel (char *mdl)
{
	byte	stackbuf[1024];		// avoid dirtying the cache heap
	byte	*buf;
	unsigned short	crc;

	if (!(buf = (byte *)COM_LoadStackFile(mdl, stackbuf, sizeof(stackbuf))))
		Host_Error ("CheckModel: could not load %s", mdl);
	crc = Security_CRC (buf, com_filesize);

	return crc;
}

/*
================
SV_ModelIndex
================
*/
int SV_ModelIndex (char *name)
{
	int	i;
	
	if (!name || !name[0])
		return 0;

	for (i=0 ; i<MAX_MODELS && sv.model_precache[i] ; i++)
		if (!strcmp(sv.model_precache[i], name))
			return i;

	if (i == MAX_MODELS || !sv.model_precache[i])
		Host_Error ("SV_ModelIndex: model %s not precached", name);

	return i;
}

/*
================
SV_CreateBaseline
================
*/
void SV_CreateBaseline (void)
{
	int			i, entnum, bits; //johnfitz -- PROTOCOL_FITZQUAKE 
	edict_t		*svent;
		
	for (entnum = 0 ; entnum < sv.num_edicts ; entnum++)
	{
	// get the current server version
		svent = EDICT_NUM(entnum);
		if (svent->free)
			continue;
		if (entnum > svs.maxclients && !svent->v.modelindex)
			continue;

	// create entity baseline
		VectorCopy (svent->v.origin, svent->baseline.origin);
		VectorCopy (svent->v.angles, svent->baseline.angles);
		svent->baseline.frame = svent->v.frame;
		svent->baseline.skin = svent->v.skin;
		if (entnum > 0 && entnum <= svs.maxclients)
		{
			svent->baseline.colormap = entnum;
			svent->baseline.modelindex = SV_ModelIndex ("progs/player.mdl");
			svent->baseline.alpha = ENTALPHA_DEFAULT; //johnfitz -- alpha support 
		}
		else
		{
			svent->baseline.colormap = 0;
			svent->baseline.modelindex = SV_ModelIndex (PR_GetString(svent->v.model));
			svent->baseline.alpha = svent->alpha; //johnfitz -- alpha support
		}

		//johnfitz -- PROTOCOL_FITZQUAKE
		bits = 0;
		if (sv.protocol == PROTOCOL_NETQUAKE) //still want to send baseline in PROTOCOL_NETQUAKE, so reset these values
		{
			if (svent->baseline.modelindex & 0xFF00)
				svent->baseline.modelindex = 0;
			if (svent->baseline.frame & 0xFF00)
				svent->baseline.frame = 0;
			svent->baseline.alpha = ENTALPHA_DEFAULT;
		}
		else //decide which extra data needs to be sent
		{
			if (svent->baseline.modelindex & 0xFF00)
				bits |= B_LARGEMODEL;
			if (svent->baseline.frame & 0xFF00)
				bits |= B_LARGEFRAME;
			if (svent->baseline.alpha != ENTALPHA_DEFAULT)
				bits |= B_ALPHA;
		}
		//johnfitz

	// add to the message
		//johnfitz -- PROTOCOL_FITZQUAKE
		if (bits)
			MSG_WriteByte(&sv.signon, svc_spawnbaseline2);
		else
			MSG_WriteByte(&sv.signon, svc_spawnbaseline);
		//johnfitz
		MSG_WriteShort (&sv.signon, entnum);

		//johnfitz -- PROTOCOL_FITZQUAKE
		if (bits)
			MSG_WriteByte(&sv.signon, bits);

		if (bits & B_LARGEMODEL)
			MSG_WriteShort(&sv.signon, svent->baseline.modelindex);
		else
			MSG_WriteByte(&sv.signon, svent->baseline.modelindex);

		if (bits & B_LARGEFRAME)
			MSG_WriteShort(&sv.signon, svent->baseline.frame);
		else
			MSG_WriteByte(&sv.signon, svent->baseline.frame);
		//johnfitz
		MSG_WriteByte (&sv.signon, svent->baseline.colormap);
		MSG_WriteByte (&sv.signon, svent->baseline.skin);
		for (i=0 ; i<3 ; i++)
		{
			MSG_WriteCoord (&sv.signon, svent->baseline.origin[i], sv.protocolflags);
			MSG_WriteAngle (&sv.signon, svent->baseline.angles[i], sv.protocolflags);
		}

		//johnfitz -- PROTOCOL_FITZQUAKE
		if (bits & B_ALPHA)
			MSG_WriteByte(&sv.signon, svent->baseline.alpha);
		//johnfitz
	}
}

/*
================
SV_SendReconnect

Tell all the clients that the server is changing levels
================
*/
void SV_SendReconnect (void)
{
	char		data[128];
	sizebuf_t	msg;

	msg.data = data;
	msg.cursize = 0;
	msg.maxsize = sizeof(data);

	MSG_WriteChar (&msg, svc_stufftext);
	MSG_WriteString (&msg, "reconnect\n");
	NET_SendToAll (&msg, 5);

	if (cls.state != ca_dedicated)
		Cmd_ExecuteString ("reconnect\n", src_command);
}

/*
================
SV_SaveSpawnparms

Grabs the current state of each client for saving across the
transition to another level
================
*/
void SV_SaveSpawnparms (void)
{
	int	i, j;

	svs.serverflags = pr_global_struct->serverflags;

	for (i = 0, host_client = svs.clients ; i < svs.maxclients ; i++, host_client++)
	{
		if (!host_client->active)
			continue;

	// call the progs to get default spawn parms for the new client
		pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
		PR_ExecuteProgram (pr_global_struct->SetChangeParms);
		for (j=0 ; j<NUM_SPAWN_PARMS ; j++)
			host_client->spawn_parms[j] = (&pr_global_struct->parm1)[j];
	}
}

/*
================
SV_SpawnServer

This is called at the start of each level
================
*/
extern	float	scr_centertime_off;

void SV_SpawnServer (char *server)
{
	static char	dummy[LOCALMODELS_STRING_SIZE];
	int	i;
	edict_t	*ent;
	extern	void R_PreMapLoad (char *);
	extern double sv_frametime;
	extern double physframetime;

	memset(dummy, 0, sizeof(dummy));

#ifdef GLQUAKE
	if (nehahra)
	{
		if (NehGameType == TYPE_DEMO)
		{
			M_Menu_Main_f ();
			return;
		}
	}
#endif

	// let's not have any servers with no name
	if (hostname.string[0] == 0)
		Cvar_Set (&hostname, "UNNAMED");
	scr_centertime_off = 0;

	if (!svs.changelevel_issued)	//joe: reset marathon time and counter if not a changelevel call from progs.dat
	{
		cls.marathon_time = 0;
		cls.marathon_level = 0;
	}

	Con_DPrintf ("SpawnServer: %s\n", server);
	svs.changelevel_issued = false;		// now safe to issue another

// tell all connected clients that we are going to a new level
	if (sv.active)
		SV_SendReconnect ();

// make cvars consistant
	if (coop.value)
		Cvar_SetValue (&deathmatch, 0);
	current_skill = (int)(skill.value + 0.5);
	current_skill = bound(0, current_skill, 3);

	Cvar_SetValue (&skill, (float)current_skill);

// set up the new server
	Host_ClearMemory ();

	Q_strncpyz (sv.name, server, sizeof(sv.name));

	sv.protocol = sv_protocol; // johnfitz 

	if (sv.protocol == PROTOCOL_RMQ)
	{
		// set up the protocol flags used by this server
		// (note - these could be cvar-ised so that server admins could choose the protocol features used by their servers)
		sv.protocolflags = PRFL_INT32COORD | PRFL_SHORTANGLE;
	}
	else 
		sv.protocolflags = 0;

// load progs to get entity field count
	PR_LoadProgs ();

// allocate server memory
	sv.max_edicts = bound(MIN_EDICTS, (int)max_edicts.value, MAX_EDICTS); //johnfitz -- max_edicts cvar 
	sv.edicts = (edict_t *)Q_malloc(sv.max_edicts * pr_edict_size); // ericw -- sv.edicts switched to use malloc() 

	sv.datagram.maxsize = sizeof(sv.datagram_buf);
	sv.datagram.cursize = 0;
	sv.datagram.data = sv.datagram_buf;

	sv.reliable_datagram.maxsize = sizeof(sv.reliable_datagram_buf);
	sv.reliable_datagram.cursize = 0;
	sv.reliable_datagram.data = sv.reliable_datagram_buf;

	sv.signon.maxsize = sizeof(sv.signon_buf);
	sv.signon.cursize = 0;
	sv.signon.data = sv.signon_buf;

// leave slots at start for clients only
	sv.num_edicts = svs.maxclients + 1;
	memset(sv.edicts, 0, sv.num_edicts * pr_edict_size); // ericw -- sv.edicts switched to use malloc() 
	for (i=0 ; i<svs.maxclients ; i++)
	{
		ent = EDICT_NUM(i+1);
		svs.clients[i].edict = ent;
	}

	sv.state = ss_loading;
	sv.paused = false;

	sv.time = 1.0;

	R_PreMapLoad (server);

	strcpy (sv.name, server);
	sprintf (sv.modelname, "maps/%s.bsp", server);
	if (!(sv.worldmodel = Mod_ForName(sv.modelname, false)))
	{
		Con_Printf ("Couldn't spawn server %s\n", sv.modelname);
		sv.active = false;
		return;
	}
	sv.models[1] = sv.worldmodel;

// clear world interaction links
	SV_ClearWorld ();

	sv.sound_precache[0] = dummy;
	sv.model_precache[0] = dummy;
	sv.model_precache[1] = sv.modelname;
	for (i=1 ; i<sv.worldmodel->numsubmodels ; i++)
	{
		sv.model_precache[i+1] = localmodels[i];
		sv.models[i+1] = Mod_ForName (localmodels[i], false);
	}

// check player/eyes models for hacks
	if (security_loaded)
	{
		sv.player_model_crc = CheckModel ("progs/player.mdl");
		sv.eyes_model_crc = CheckModel ("progs/eyes.mdl");
	}

// load the rest of the entities
	ent = EDICT_NUM(0);
	memset (&ent->v, 0, progs->entityfields * 4);
	ent->free = false;
	ent->v.model = PR_SetEngineString(sv.worldmodel->name);
	ent->v.modelindex = 1;		// world model
	ent->v.solid = SOLID_BSP;
	ent->v.movetype = MOVETYPE_PUSH;

	if (coop.value)
		pr_global_struct->coop = coop.value;
	else
		pr_global_struct->deathmatch = deathmatch.value;

	pr_global_struct->mapname = PR_SetEngineString(sv.name);

// serverflags are for cross level information (sigils)
	pr_global_struct->serverflags = svs.serverflags;

	ED_LoadFromFile (sv.worldmodel->entities);

	if (sv.protocol == PROTOCOL_NETQUAKE &&
		(
		sv.num_edicts >= 8192 ||
		sv.num_sound_precaches >= 256 ||
		sv.num_model_precaches >= 256
		))
	{
		sv.protocol = PROTOCOL_FITZQUAKE;
	}

	sv.active = true;

// all setup is completed, any further precache statements are errors
	sv.state = ss_active;

// run two frames to allow everything to settle
	physframetime += 0.1;
	sv_frametime = physframetime;
	SV_Physics ();
//	sv.time += 0.1;
	SV_Physics ();
//	sv.time += 0.1;

// create a baseline for more efficient communications
	SV_CreateBaseline ();

// send serverinfo to all connected clients
	for (i = 0, host_client = svs.clients ; i < svs.maxclients ; i++, host_client++)
		if (host_client->active)
			SV_SendServerinfo (host_client);

// joe: this needs to be done here because after spawning a server it runs
// an "empty" frame in which temporary entity updates gets screwed up
	CL_ClearTEnts ();

	Con_DPrintf ("Server spawned\n");
}
