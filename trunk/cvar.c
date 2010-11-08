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
// cvar.c -- dynamic variable tracking

#include "quakedef.h"

cvar_t	*cvar_vars;
char	*cvar_null_string = "";

cvar_t	cvar_savevars = {"cvar_savevars", "0"};

qboolean Cvar_Delete (char *name);

/*
============
Cvar_FindVar
============
*/
cvar_t *Cvar_FindVar (char *var_name)
{
	cvar_t	*var;

	for (var = cvar_vars ; var ; var = var->next)
		if (!Q_strcasecmp(var_name, var->name))
			return var;

	return NULL;
}

/*
============
Cvar_ResetVar
============
*/
void Cvar_ResetVar (cvar_t *var)
{
	if (var && strcmp(var->string, var->defaultvalue))
		Cvar_Set (var, var->defaultvalue);
}

/*
============
Cvar_Reset_f
============
*/
void Cvar_Reset_f (void)
{
	int	c;
	char	*s;
	cvar_t	*var;

	if ((c = Cmd_Argc()) != 2)
	{
		Con_Printf ("Usage: %s <variable>\n", Cmd_Argv(0));
		return;
	}

	s = Cmd_Argv(1);

	if ((var = Cvar_FindVar(s)))
		Cvar_ResetVar (var);
	else
		Con_Printf ("%s : No variable with name %s\n", Cmd_Argv(0), s);
}

/*
============
Cvar_SetDefault
============
*/
void Cvar_SetDefault (cvar_t *var, float value)
{
	int	i;
	char	val[128];
	
	Q_snprintfz (val, sizeof(val), "%f", value);

	for (i = strlen(val) - 1 ; i > 0 && val[i] == '0' ; i--)
		val[i] = 0;
	if (val[i] == '.')
		val[i] = 0;

	Z_Free (var->defaultvalue);
	var->defaultvalue = CopyString (val);
	Cvar_Set (var, val);
}

/*
============
Cvar_VariableValue
============
*/
float Cvar_VariableValue (char *var_name)
{
	cvar_t	*var;

	if (!(var = Cvar_FindVar(var_name)))
		return 0;

	return Q_atof (var->string);
}

/*
============
Cvar_VariableString
============
*/
char *Cvar_VariableString (char *var_name)
{
	cvar_t	*var;

	if (!(var = Cvar_FindVar(var_name)))
		return cvar_null_string;

	return var->string;
}

/*
============
Cvar_CompleteVariable
============
*/
char *Cvar_CompleteVariable (char *partial)
{
	cvar_t	*cvar;
	int	len;

	if (!(len = strlen(partial)))
		return NULL;

	// check functions
	for (cvar = cvar_vars ; cvar ; cvar = cvar->next)
		if (!Q_strncasecmp(partial, cvar->name, len))
			return cvar->name;

	return NULL;
}

/*
============
Cvar_CompleteCountPossible
============
*/
int Cvar_CompleteCountPossible (char *partial)
{
	cvar_t	*cvar;
	int	len, c = 0;

	if (!(len = strlen(partial)))
		return 0;

	// check partial match
	for (cvar = cvar_vars ; cvar ; cvar = cvar->next)
		if (!Q_strncasecmp(partial, cvar->name, len))
			c++;

	return c;
}

/*
============
Cvar_Set
============
*/
void Cvar_Set (cvar_t *var, char *value)
{
	qboolean	changed;
	static qboolean	changing = false;

	if (!var || !Cvar_FindVar(var->name))
		return;

	if (var->flags & CVAR_ROM)
	{
		if (con_initialized)
			Con_Printf ("\"%s\" is write protected\n", var->name);
		return;
	}

	if ((var->flags & CVAR_INIT) && host_initialized)
	{
		if (cl_warncmd.value || developer.value)
			Con_Printf ("\"%s\" can only be changed with \"+set %s %s\" on the command line.\n", var->name, var->name, value);
		return;
	}

	changed = strcmp (var->string, value);

	if (var->OnChange && !changing)
	{
		changing = true;
		if (var->OnChange(var, value))
		{
			changing = false;
			return;
		}
		changing = false;
	}

	Z_Free (var->string);	// free the old value string

	var->string = CopyString (value);
	var->value = Q_atof (var->string);

	if ((var->flags & CVAR_SERVER) && changed && sv.active)
		SV_BroadcastPrintf ("\"%s\" changed to \"%s\"\n", var->name, var->string);

	// joe, from ProQuake: rcon (64 doesn't mean anything special,
	// but we need some extra space because NET_MAXMESSAGE == RCON_BUFF_SIZE)
	if (rcon_active && (rcon_message.cursize < rcon_message.maxsize - strlen(var->name) - strlen(var->string) - 64))
	{
		rcon_message.cursize--;
		MSG_WriteString (&rcon_message, va("\"%s\" set to \"%s\"\n", var->name, var->string));
	}
}

/*
============
Cvar_ForceSet
============
*/
void Cvar_ForceSet (cvar_t *var, char *value)
{
	int	saved_flags;

	if (!var)
		return;

	saved_flags = var->flags;
	var->flags &= ~CVAR_ROM;
	Cvar_Set (var, value);
	var->flags = saved_flags;
}

/*
============
Cvar_SetValue
============
*/
void Cvar_SetValue (cvar_t *var, float value)
{
	char	val[128];
	int	i;
	
	Q_snprintfz (val, sizeof(val), "%f", value);

	for (i = strlen(val) - 1 ; i > 0 && val[i] == '0' ; i--)
		val[i] = 0;
	if (val[i] == '.')
		val[i] = 0;

	Cvar_Set (var, val);
}

/*
============
Cvar_Register

Adds a freestanding variable to the variable list.
============
*/
void Cvar_Register (cvar_t *var)
{
	char	string[512];
	cvar_t	*old;

// first check to see if it has already been defined
	old = Cvar_FindVar (var->name);
	if (old && !(old->flags & CVAR_USER_CREATED))
	{
		Con_Printf ("Can't register variable %s, already defined\n", var->name);
		return;
	}

// check for overlap with a command
	if (Cmd_FindCommand(var->name))
	{
		Con_Printf ("Cvar_Register: %s is a command\n", var->name);
		return;
	}

	var->defaultvalue = CopyString (var->string);
	if (old)
	{
		var->flags |= old->flags & ~CVAR_USER_CREATED;
		Q_strncpyz (string, old->string, sizeof(string));
		Cvar_Delete (old->name);
		if (!(var->flags & CVAR_ROM))
			var->string = CopyString (string);
		else
			var->string = CopyString (var->string);
	}
	else
	{	// allocate the string on zone because future sets will Z_Free it
		var->string = CopyString (var->string);
	}
	var->value = Q_atof (var->string);

// link the variable in
	var->next = cvar_vars;
	cvar_vars = var;
}

/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
qboolean Cvar_Command (void)
{
	cvar_t	*var;

// check variables
	if (!(var = Cvar_FindVar(Cmd_Argv(0))))
		return false;

// perform a variable print or set
	if (Cmd_Argc() == 1)
		Con_Printf ("\"%s\" is:\"%s\" default:\"%s\"\n", var->name, var->string, var->defaultvalue);
	else
		Cvar_Set (var, Cmd_Argv(1));

	return true;
}

/*
============
Cvar_WriteVariables

Writes lines containing "set variable value" for all variables
with the archive flag set to true.
============
*/
void Cvar_WriteVariables (FILE *f)
{
	cvar_t	*var;
	
	for (var = cvar_vars ; var ; var = var->next)
	{
		if (var->flags & CVAR_ARCHIVE)
			fprintf (f, "%s \"%s\"\n", var->name, var->string);
		// don't save read-only or initial cvars, nor cl_warncmd (it has different role)
		else if ((var->flags & CVAR_ROM) || (var->flags & CVAR_INIT) || !strcmp(var->name, "cl_warncmd"))
			continue;
		else if ((cvar_savevars.value == 1 && strcmp(var->string, var->defaultvalue)) || cvar_savevars.value == 2)
			fprintf (f, "%s \"%s\"\n", var->name, var->string);
	}
}

/*
============
Cvar_CvarList_f

List all console variables
============
*/
void Cvar_CvarList_f (void)
{
	cvar_t	*var;
	int	counter;
	
	if (cmd_source != src_command)
		return;

	for (counter = 0, var = cvar_vars ; var ; var = var->next, counter++)
		Con_Printf ("%s\n", var->name);

	Con_Printf ("------------\n%d variables\n", counter);
}

/*
============
Cvar_Create
============
*/
cvar_t *Cvar_Create (char *name, char *string, int cvarflags)
{
	cvar_t	*v;

	if ((v = Cvar_FindVar(name)))
		return v;
	v = (cvar_t *)Z_Malloc (sizeof(cvar_t));

	// Cvar doesn't exist, so we create it
	v->next = cvar_vars;
	cvar_vars = v;

	v->name = CopyString (name);
	v->string = CopyString (string);
	v->defaultvalue = CopyString (string);	
	v->flags = cvarflags;
	v->value = Q_atof (v->string);

	return v;
}

/*
============
Cvar_Delete

returns true if the cvar was found (and deleted)
============
*/
qboolean Cvar_Delete (char *name)
{
	cvar_t	*var, *prev;

	if (!(var = Cvar_FindVar(name)))
		return false;

	prev = NULL;
	for (var = cvar_vars ; var ; var = var->next)
	{
		if (!Q_strcasecmp(var->name, name))
		{
			// unlink from cvar list
			if (prev)
				prev->next = var->next;
			else
				cvar_vars = var->next;

			// free
			Z_Free (var->defaultvalue);  
			Z_Free (var->string);
			Z_Free (var->name);
			Z_Free (var);
			return true;
		}
		prev = var;
	}

	return false;
}

void Cvar_Set_f (void)
{
	cvar_t	*var;
	char	*var_name;

	if (Cmd_Argc() != 3)
	{
		Con_Printf ("Usage: %s <cvar> <value>\n", Cmd_Argv(0));
		return;
	}

	var_name = Cmd_Argv (1);
	if ((var = Cvar_FindVar(var_name)))
	{
		Cvar_Set (var, Cmd_Argv(2));
	}
	else 
	{
		if (Cmd_FindCommand(var_name))
		{
			Con_Printf ("\"%s\" is a command\n", var_name);
			return;
		}
		var = Cvar_Create (var_name, Cmd_Argv(2), CVAR_USER_CREATED);
	}
}

/*
============
Cvar_Init
============
*/
void Cvar_Init (void)
{
	Cvar_Register (&cvar_savevars);

	Cmd_AddCommand ("cvarlist", Cvar_CvarList_f);
	Cmd_AddCommand ("cvar_reset", Cvar_Reset_f);
	Cmd_AddCommand ("set", Cvar_Set_f);
}
