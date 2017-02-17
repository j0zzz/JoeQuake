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
// cvar.h

/*

cvar_t variables are used to hold scalar or string variables that can be changed
or displayed at the console or prog code as well as accessed directly in C code.

it is sufficient to initialize a cvar_t with just the first two fields, or
you can add a ,true flag for variables that you want saved to the configuration
file when the game is quit:

cvar_t	r_draworder = {"r_draworder","1"};
cvar_t	scr_screensize = {"screensize","1",true};

Cvars must be registered before use, or they will have a 0 value instead of the
float interpretation of the string. Generally, all cvar_t declarations should be
registered in the apropriate init function before any console commands are executed:
Cvar_Register (&host_framerate);

C code usually just references a cvar in place:
if (r_draworder.value)

It could optionally ask for the value to be looked up for a string name:
if (Cvar_VariableValue ("r_draworder"))

Interpreted prog code can access cvars with the cvar(name) or
cvar_set (name, value) internal functions:
teamplay = cvar("teamplay");
cvar_set ("registered", "1");

The user can access cvars from the console in two ways:
r_draworder		prints the current value
r_draworder 0		sets the current value to 0
Cvars are restricted from having the same names as commands to keep this
interface from being ambiguous.

*/

#define CVAR_ARCHIVE		1	// set to true to cause it to be saved to config.cfg
#define CVAR_SERVER		2	// notifies players when changed
#define CVAR_ROM		4	// read only
#define CVAR_INIT		8	// can only be set during initialization
#define	CVAR_USER_CREATED	16	// created by a set command

typedef struct cvar_s
{
	char	*name;
	char	*string;
	int		flags;
	qboolean (*OnChange)(struct cvar_s *var, char *value);
	float	value;
	char	*defaultvalue;
	struct cvar_s *next;
} cvar_t;

// registers a cvar that allready has the name, string, and optionally the
// archive elements set.
void Cvar_Register (cvar_t *variable);

// equivalent to "<name> <variable>" typed at the console
void Cvar_Set (cvar_t *var, char *value);

// force a set even if the cvar is read only
void Cvar_ForceSet (cvar_t *var, char *string);

// expands value to a string and calls Cvar_Set
void Cvar_SetValue (cvar_t *var, float value);

void Cvar_ResetVar (cvar_t *var);
void Cvar_SetDefault (cvar_t *var, float value);

// returns 0 if not defined or non numeric
float Cvar_VariableValue (char *var_name);

// returns an empty string if not defined
char *Cvar_VariableString (char *var_name);

int Cvar_CompleteCountPossible (char *partial);

// attempts to match a partial variable name for command line completion
// returns NULL if nothing fits
char *Cvar_CompleteVariable (char *partial);

// called by Cmd_ExecuteString when Cmd_Argv(0) doesn't match a known
// command.  Returns true if the command was a variable reference that
// was handled. (print or change)
qboolean Cvar_Command (void);

// Writes lines containing "set variable value" for all variables
// with the archive flag set to true.
void Cvar_WriteVariables (FILE *f);

cvar_t *Cvar_FindVar (char *var_name);
void Cvar_Init (void);

extern cvar_t	*cvar_vars;

extern cvar_t	cvar_savevars;
