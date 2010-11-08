/*
Copyright (C) 2002, J.P. Grossman

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
// iplog.c

#include "quakedef.h"

iplog_t	*iplogs, *iplog_head;
int	iplog_size, iplog_next, iplog_full;

#define	DEFAULT_IPLOGSIZE	0x10000

/*
====================
IPLog_Init
====================
*/
void IPLog_Init (void)
{
	int	p;
	FILE	*f;
	iplog_t	temp;

	// Allocate space for the IP logs
	iplog_size = 0;
	if (!(p = COM_CheckParm("-iplog")))
		return;

	if (p < com_argc - 1)
		iplog_size = Q_atoi(com_argv[p+1]) * 1024 / sizeof(iplog_t);

	if (!iplog_size)
		iplog_size = DEFAULT_IPLOGSIZE;

	iplogs = (iplog_t *)Hunk_AllocName (iplog_size * sizeof(iplog_t), "iplog");
	iplog_next = 0;
	iplog_head = NULL;
	iplog_full = 0;

	// Attempt to load log data from iplog.dat
	Sys_GetLock ();
	if ((f = fopen("iplog.dat", "r")))
	{
		while (fread(&temp, 20, 1, f))
			IPLog_Add (temp.addr, temp.name);
		fclose (f);
	}
	Sys_ReleaseLock ();

	Cmd_AddCommand ("identify", IPLog_Identify_f);
	Cmd_AddCommand ("ipdump", IPLog_Dump);
	Cmd_AddCommand ("ipmerge", IPLog_Import);
}

/*
====================
IPLog_Import
====================
*/
void IPLog_Import (void)
{
	FILE	*f;
	iplog_t	temp;

	if (Cmd_Argc() < 2)
	{
		Con_Printf ("Usage: ipmerge <filename>\n");
		return;
	}

	if ((f = fopen(va("%s", Cmd_Argv(1)), "r")))
	{
		while (fread(&temp, 20, 1, f))
			IPLog_Add (temp.addr, temp.name);
		fclose (f);
		Con_Printf ("Merged %s\n", Cmd_Argv(1));
	}
	else
	{
		Con_Printf ("Could not open %s\n", Cmd_Argv(1));
	}
}

/*
====================
IPLog_WriteLog
====================
*/
void IPLog_WriteLog (void)
{
	int	i;
	FILE	*f;
	iplog_t	temp;

	if (!iplog_size)
		return;

	Sys_GetLock ();

	// first merge
	if ((f = fopen("iplog.dat", "r")))
	{
		while (fread(&temp, 20, 1, f))
			IPLog_Add (temp.addr, temp.name);
		fclose (f);
	}

	// then write
	if ((f = fopen("iplog.dat", "w")))
	{
		if (iplog_full)
		{
			for (i = iplog_next + 1 ; i < iplog_size ; i++)
				fwrite (&iplogs[i], 20, 1, f);
		}
		for (i = 0 ; i < iplog_next ; i++)
			fwrite (&iplogs[i], 20, 1, f);

		fclose (f);
	}	
	else
	{
		Con_Printf ("Could not write iplog.dat\n");
	}

	Sys_ReleaseLock ();
}

#define	MAX_REPITITION	64

/*
====================
IPLog_Add
====================
*/
void IPLog_Add (int addr, char *name)
{
	int	i, cmatch;	// limit 128 entries per IP
	char	*ch, name2[16];
	iplog_t	*iplog_new, *parent, **ppnew, *match[MAX_REPITITION];

	if (!iplog_size)
		return;

	// delete trailing spaces
	strncpy (name2, name, 15);
	ch = &name2[15];
	*ch = 0;
	while (ch >= name2 && (*ch == 0 || *ch == ' '))
		*ch-- = 0;
	if (ch < name2)
		return;

	iplog_new = &iplogs[iplog_next];

	cmatch = 0;
	parent = NULL;
	ppnew = &iplog_head;
	while (*ppnew)
	{
		if ((*ppnew)->addr == addr)
		{
			if (!strcmp(name2, (*ppnew)->name))
				return;

			match[cmatch] = *ppnew;
			if (++cmatch == MAX_REPITITION)
			{
				// shift up the names and replace the last one
				for (i = 0 ; i < MAX_REPITITION - 1 ; i++)
					strcpy (match[i]->name, match[i+1]->name);
				strcpy (match[i]->name, name2);
				return;
			}
		}
		parent = *ppnew;
		ppnew = &(*ppnew)->children[addr > (*ppnew)->addr];
	}
	*ppnew = iplog_new;
	strcpy (iplog_new->name, name2);
	iplog_new->addr = addr;
	iplog_new->parent = parent;
	iplog_new->children[0] = NULL;
	iplog_new->children[1] = NULL;

	if (++iplog_next == iplog_size)
	{
		iplog_next = 0;
		iplog_full = 1;
	}

	if (iplog_full)
		IPLog_Delete (&iplogs[iplog_next]);
}

/*
====================
IPLog_Delete
====================
*/
void IPLog_Delete (iplog_t *node)
{
	iplog_t	*newlog;

	if ((newlog = IPLog_Merge(node->children[0], node->children[1])))
		newlog->parent = node->parent;
	if (node->parent)
		node->parent->children[node->addr > node->parent->addr] = newlog;
	else
		iplog_head = newlog;
}

/*
====================
IPLog_Merge
====================
*/
iplog_t *IPLog_Merge (iplog_t *left, iplog_t *right)
{
	if (!left)
		return right;
	if (!right)
		return left;

	if (rand() & 1)
	{
		left->children[1] = IPLog_Merge (left->children[1], right);
		left->children[1]->parent = left;
		return left;
	}

	right->children[0] = IPLog_Merge (left, right->children[0]);
	right->children[0]->parent = right;
	return right;
}

/*
====================
IPLog_Identify
====================
*/
void IPLog_Identify (int addr)
{
	int	count = 0;
	iplog_t	*node;

	node = iplog_head;
	while (node)
	{
		if (node->addr == addr)
		{
			Con_Printf ("%s\n", node->name);
			count++;
		}
		node = node->children[addr > node->addr];
	}
	Con_Printf ("%d %s found\n", count, (count == 1) ? "entry" : "entries");
}

// used to translate to non-fun characters for identify <name>
char unfun[129] = 
"................[]olzeasbt89...."
"........[]......olzeasbt89..[.]."
"aabcdefghijklmnopqrstuvwxyz[.].."
".abcdefghijklmnopqrstuvwxyz[.]..";

// try to find s1 inside of s2
int unfun_match (char *s1, char *s2)
{
	int	i;

	for ( ; *s2 ; s2++)
	{
		for (i=0 ; s1[i] ; i++)
		{
			if (unfun[s1[i] & 127] != unfun[s2[i] & 127])
				break;
		}
		if (!s1[i])
			return true;
	}

	return false;
}

/*
==================
IPLog_Identify_f

Print all known names for the specified player's ip address
==================
*/
void IPLog_Identify_f (void)
{
	int	i, a, b, c;
	char	name[16];

	if (Cmd_Argc() < 2)
	{
		Con_Printf ("usage: identify <player_number | name>\n");
		return;
	}

	if (sscanf(Cmd_Argv(1), "%d.%d.%d", &a, &b, &c) == 3)
	{
		Con_Printf ("known aliases for %d.%d.%d:\n", a, b, c);
		IPLog_Identify ((a << 16) | (b << 8) | c);
		return;
	}

	i = Q_atoi(Cmd_Argv(1)) - 1;
	if (i == -1)
	{
		if (sv.active)
		{
			for (i=0 ; i<svs.maxclients ; i++)
			{
				if (svs.clients[i].active && unfun_match(Cmd_Argv(1), svs.clients[i].name))
					break;
			}
		}
		else
		{
			for (i=0 ; i<cl.maxclients ; i++)
			{
				if (unfun_match(Cmd_Argv(1), cl.scores[i].name))
					break;
			}
		}
	}

	if (sv.active)
	{
		if (i < 0 || i >= svs.maxclients || !svs.clients[i].active)
		{
			Con_Printf ("No such player\n");
			return;
		}
		if (sscanf(svs.clients[i].netconnection->address, "%d.%d.%d", &a, &b, &c) != 3)
		{
			Con_Printf ("Could not determine IP information for %s\n", svs.clients[i].name);
			return;
		}
		Q_strncpyz (name, svs.clients[i].name, sizeof(name));
		Con_Printf ("known aliases for %s:\n", name);
		IPLog_Identify ((a << 16) | (b << 8) | c);
	}
	else
	{
		if (i < 0 || i >= cl.maxclients || !cl.scores[i].name[0])
		{
			Con_Printf ("No such player\n");
			return;
		}
		if (!cl.scores[i].addr)
		{
			Con_Printf ("No IP information for %.15s\nUse 'status'\n", cl.scores[i].name);
			return;
		}
		Q_strncpyz (name, cl.scores[i].name, sizeof(name));
		Con_Printf ("known aliases for %s:\n", name);
		IPLog_Identify (cl.scores[i].addr);
	}
}

/*
====================
IPLog_DumpTree
====================
*/
void IPLog_DumpTree (iplog_t *root, FILE *f)
{
	char	address[16], name[16];

	if (!root)
		return;

	IPLog_DumpTree (root->children[0], f);

	sprintf (address, "%d.%d.%d.xxx", root->addr >> 16, (root->addr >> 8) & 0xff, root->addr & 0xff);
	strcpy (name, root->name);
	fprintf (f, "%-16s  %s\n", address, name);

	IPLog_DumpTree (root->children[1], f);
}

/*
====================
IPLog_Dump
====================
*/
void IPLog_Dump (void)
{
	FILE	*f;

	if (!(f = fopen(va("%s/iplog.txt", com_gamedir), "w")))
	{
		Con_Printf ("Couldn't write iplog.txt\n");
		return;
	}

	IPLog_DumpTree (iplog_head, f);
	fclose (f);
	Con_Printf ("Wrote iplog.txt\n");
}
