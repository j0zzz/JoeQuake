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
// cmd.c -- Quake script command processing module

#include "quakedef.h"
#include "winquake.h"

// joe: ReadDir()'s stuff
#ifndef _WIN32
#include <glob.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

cvar_t		cl_warncmd = {"cl_warncmd", "0"};

cmd_alias_t	*cmd_alias;
qboolean	cmd_wait;

//=============================================================================

/*
============
Cmd_Wait_f

Causes execution of the remainder of the command buffer to be delayed until
next frame.  This allows commands like:
bind g "impulse 5 ; +attack ; wait ; -attack ; impulse 2"
============
*/
void Cmd_Wait_f (void)
{
	cmd_wait = true;
}

/*
=============================================================================

				COMMAND BUFFER

=============================================================================
*/

sizebuf_t	cmd_text;

/*
============
Cbuf_Init
============
*/
void Cbuf_Init (void)
{
	SZ_Alloc(&cmd_text, 1 << 18);	// space for commands and script files. spike -- was 8192, but modern configs can be _HUGE_, at least if they contain lots of comments/docs for things.
}

/*
============
Cbuf_AddText

Adds command text at the end of the buffer
============
*/
void Cbuf_AddText (char *text)
{
	int	l;
	
	l = strlen (text);

	if (cmd_text.cursize + l >= cmd_text.maxsize)
	{
		Con_Printf ("Cbuf_AddText: overflow\n");
		return;
	}

	SZ_Write (&cmd_text, text, strlen(text));
}

/*
============
Cbuf_InsertText

Adds command text immediately after the current command
Adds a \n to the text
FIXME: actually change the command buffer to do less copying
============
*/
void Cbuf_InsertText (char *text)
{
	char	*temp = NULL;
	int	templen;

// copy off any commands still remaining in the exec buffer
	if ((templen = cmd_text.cursize))
	{
		temp = Z_Malloc (templen);
		memcpy (temp, cmd_text.data, templen);
		SZ_Clear (&cmd_text);
	}

// add the entire text of the file
	Cbuf_AddText (text);

// add the copied off data
	if (templen)
	{
		SZ_Write (&cmd_text, temp, templen);
		Z_Free (temp);
	}
}

/*
============
Cbuf_Execute
============
*/
void Cbuf_Execute (void)
{
	int	i, quotes;
	char	*text, line[1024];

	while (cmd_text.cursize)
	{
	// find a \n or ; line break
		text = (char *)cmd_text.data;

		quotes = 0;
		for (i=0 ; i<cmd_text.cursize ; i++)
		{
			if (text[i] == '"')
				quotes++;
			if (!(quotes & 1) &&  text[i] == ';')
				break;	// don't break if inside a quoted string
			if (text[i] == '\n')
				break;
		}

		memcpy (line, text, i);
		line[i] = 0;

	// delete the text from the command buffer and move remaining commands down
	// this is necessary because commands (exec, alias) can insert data at the
	// beginning of the text buffer
		if (i == cmd_text.cursize)
		{
			cmd_text.cursize = 0;
		}
		else
		{
			i++;
			cmd_text.cursize -= i;
			memmove (text, text + i, cmd_text.cursize);
		}

	// execute the command line
		Cmd_ExecuteString (line, src_command);

		if (cmd_wait)
		{	// skip out while text still remains in buffer, leaving it for next frame
			cmd_wait = false;
			break;
		}
	}
}

/*
==============================================================================

				SCRIPT COMMANDS

==============================================================================
*/

/*
===============
Set commands are added early, so they are guaranteed to be set before
the client and server initialize for the first time.

Other commands are added late, after all initialization is complete.
===============
*/
void Cbuf_AddEarlyCommands (void)
{
	int	i;

	for (i = 0 ; i < COM_Argc() - 2 ; i++)
	{
		if (Q_strcasecmp(COM_Argv(i), "+set"))
			continue;
		Cbuf_AddText (va("set %s %s\n", COM_Argv(i+1), COM_Argv(i+2)));
		i += 2;
	}
}

/*
===============
Cmd_StuffCmds_f

Adds command line parameters as script statements
Commands lead with a +, and continue until a - or another +
quake +prog jctest.qp +cmd amlev1
quake -nosound +cmd amlev1
===============
*/
void Cmd_StuffCmds_f (void)
{
	int	i, j, s;
	char	*text, *build, c;

// build the combined string to parse from
	s = 0;
	for (i=1 ; i<com_argc ; i++)
	{
		if (!com_argv[i])
			continue;		// NEXTSTEP nulls out -NXHost
		s += strlen (com_argv[i]) + 1;
	}
	if (!s)
		return;

	text = Z_Malloc (s + 1);
	text[0] = 0;
	for (i=1 ; i<com_argc ; i++)
	{
		if (!com_argv[i])
			continue;		// NEXTSTEP nulls out -NXHost
		strcat (text, com_argv[i]);
		if (i != com_argc-1)
			strcat (text, " ");
	}

// pull out the commands
	build = Z_Malloc (s + 1);
	build[0] = 0;

	for (i=0 ; i<s-1 ; i++)
	{
		if (text[i] == '+')
		{
			i++;

			for (j=i ; (text[j] != '+') && (text[j] != '-') && (text[j] != 0) ; j++)
				;

			c = text[j];
			text[j] = 0;

			strcat (build, text + i);
			strcat (build, "\n");
			text[j] = c;
			i = j - 1;
		}
	}

	if (build[0])
		Cbuf_InsertText (build);

	Z_Free (text);
	Z_Free (build);
}

/*
===============
Cmd_Exec_f
===============
*/
void Cmd_Exec_f (void)
{
	char	*f, name[MAX_OSPATH];
	int	mark;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("exec <filename> : execute a script file\n");
		return;
	}

	Q_strncpyz (name, Cmd_Argv(1), sizeof(name));
	mark = Hunk_LowMark ();
	if (!(f = (char *)COM_LoadHunkFile(name)))
	{
		char	*p;

		p = COM_SkipPath (name);
		if (!strchr(p, '.'))
		{	// no extension, so try the default (.cfg)
			strcat (name, ".cfg");
			f = (char *)COM_LoadHunkFile (name);
		}

		if (!f)
		{
			Con_Printf ("couldn't exec %s\n", name);
			return;
		}
	}
	if (cl_warncmd.value || developer.value)
		Con_Printf ("execing %s\n",name);

	Cbuf_InsertText (f);
	Hunk_FreeToLowMark (mark);
}

/*
===============
Cmd_Echo_f

Just prints the rest of the line to the console
===============
*/
void Cmd_Echo_f (void)
{
	int	i;
	
	for (i=1 ; i<Cmd_Argc() ; i++)
		Con_Printf ("%s ", Cmd_Argv(i));
	Con_Printf ("\n");
}

/*
===============
Cmd_Alias_f

Creates a new command that executes a command string (possibly ; seperated)
===============
*/
void Cmd_Alias_f (void)
{
	cmd_alias_t	*a;
	char		*s, cmd[1024];
	int		i, c;

	if (Cmd_Argc() == 1)
	{
		Con_Printf ("Current alias commands:\n");
		for (a = cmd_alias ; a ; a = a->next)
			Con_Printf ("%s : %s\n", a->name, a->value);
		return;
	}

	s = Cmd_Argv (1);
	if (strlen(s) >= MAX_ALIAS_NAME)
	{
		Con_Printf ("Alias name is too long\n");
		return;
	}

// if the alias already exists, reuse it
	for (a = cmd_alias ; a ; a = a->next)
	{
		if (!strcmp(s, a->name))
		{
			Z_Free (a->value);
			break;
		}
	}

	if (!a)
	{
		a = Z_Malloc (sizeof(cmd_alias_t));
		a->next = cmd_alias;
		cmd_alias = a;
	}
	strcpy (a->name, s);

// copy the rest of the command line
	cmd[0] = 0;		// start out with a null string
	c = Cmd_Argc ();
	for (i=2 ; i<c ; i++)
	{
		strcat (cmd, Cmd_Argv(i));
		if (i != c)
			strcat (cmd, " ");
	}
	strcat (cmd, "\n");
	
	a->value = CopyString (cmd);
}

/*
============
Cmd_FindAlias
============
*/
cmd_alias_t *Cmd_FindAlias (char *name)
{
	cmd_alias_t	*alias;

	for (alias = cmd_alias ; alias ; alias = alias->next)
		if (!Q_strcasecmp(name, alias->name))
			return alias;

	return NULL;
}

// joe: legacy commands, from FuhQuake
static	legacycmd_t	*legacycmds = NULL;

void Cmd_AddLegacyCommand (char *oldname, char *newname)
{
	legacycmd_t	*cmd;

	cmd = (legacycmd_t *)Q_malloc (sizeof(legacycmd_t));
	cmd->next = legacycmds;
	legacycmds = cmd;

	cmd->oldname = CopyString (oldname);
	cmd->newname = CopyString (newname);
}

legacycmd_t *Cmd_FindLegacyCommand (char *oldname)
{
	legacycmd_t	*cmd;

	for (cmd = legacycmds ; cmd ; cmd = cmd->next)
		if (!Q_strcasecmp(cmd->oldname, oldname))
			return cmd;

	return NULL;
}

static qboolean Cmd_LegacyCommand (void)
{
	char		text[1024];
	qboolean	recursive = false;
	legacycmd_t	*cmd;

	for (cmd = legacycmds ; cmd ; cmd = cmd->next)
		if (!Q_strcasecmp(cmd->oldname, Cmd_Argv(0)))
			break;

	if (!cmd)
		return false;

	if (!cmd->newname[0])
		return true;		// just ignore this command

	// build new command string
	Q_strncpyz (text, cmd->newname, sizeof(text) - 1);
	strcat (text, " ");
	strncat (text, Cmd_Args(), sizeof(text) - strlen(text) - 1);

	if (recursive)
	{
		Host_Error("Cmd_LegacyCommand: resursive call detected.");
	}
	recursive = true;
	Cmd_ExecuteString (text, src_command);
	recursive = false;

	return true;
}

/*
=============================================================================

				COMMAND EXECUTION

=============================================================================
*/

static	cmd_function_t	*cmd_functions;		// possible commands to execute

#define	MAX_ARGS	80

static	int	cmd_argc;
static	char	*cmd_argv[MAX_ARGS];
static	char	*cmd_null_string = "";
static	char	*cmd_args = NULL;

cmd_source_t	cmd_source;

/*
============
Cmd_Argc
============
*/
int Cmd_Argc (void)
{
	return cmd_argc;
}

/*
============
Cmd_Argv
============
*/
char *Cmd_Argv (int arg)
{
	if ((unsigned)arg >= cmd_argc)
		return cmd_null_string;
	return cmd_argv[arg];
}

/*
============
Cmd_Args
============
*/
char *Cmd_Args (void)
{
	if (!cmd_args)
		return "";
	return cmd_args;
}

/*
============
Cmd_TokenizeString

Parses the given string into command line tokens.
============
*/
void Cmd_TokenizeString (char *text)
{
	int		idx;
	static	char	argv_buf[1024];

	idx = 0;

	cmd_argc = 0;
	cmd_args = NULL;

	while (1)
	{
	// skip whitespace up to a /n
		while (*text == ' ' || *text == '\t' || *text == '\r')
			text++;

		if (*text == '\n')
		{	// a newline seperates commands in the buffer
			text++;
			break;
		}

		if (!*text)
			return;

		if (cmd_argc == 1)
			 cmd_args = text;

		if (!(text = COM_Parse(text)))
			return;

		if (cmd_argc < MAX_ARGS)
		{
			cmd_argv[cmd_argc] = argv_buf + idx;
			strcpy (cmd_argv[cmd_argc], com_token);
			idx += strlen(com_token) + 1;
			cmd_argc++;
		}
	}
}

/*
============
Cmd_AddCommand
============
*/
void Cmd_AddCommand (char *cmd_name, xcommand_t function)
{
	cmd_function_t	*cmd;

	if (host_initialized)	// because hunk allocation would get stomped
		Sys_Error ("Cmd_AddCommand after host_initialized");

// fail if the command is a variable name
	if (Cvar_VariableString(cmd_name)[0])
	{
		Con_Printf ("Cmd_AddCommand: %s already defined as a var\n", cmd_name);
		return;
	}

// fail if the command already exists
	if (Cmd_FindCommand(cmd_name))
	{
		Con_Printf ("Cmd_AddCommand: %s already defined\n", cmd_name);
		return;
	}

	cmd = Hunk_Alloc (sizeof(cmd_function_t));
	cmd->name = cmd_name;
	cmd->function = function;
	cmd->next = cmd_functions;
	cmd_functions = cmd;
}

/*
============
Cmd_FindCommand
============
*/
cmd_function_t *Cmd_FindCommand (char *cmd_name)
{
	cmd_function_t	*cmd;

	for (cmd = cmd_functions ; cmd ; cmd = cmd->next)
		if (!Q_strcasecmp(cmd_name, cmd->name))
			return cmd;

	return NULL;
}

/*
============
Cmd_CompleteCommand
============
*/
char *Cmd_CompleteCommand (char *partial)
{
	cmd_function_t	*cmd;
	legacycmd_t	*lcmd;
	int		len;

	if (!(len = strlen(partial)))
		return NULL;

// check functions
	for (cmd = cmd_functions ; cmd ; cmd = cmd->next)
		if (!Q_strncasecmp(partial, cmd->name, len))
			return cmd->name;

	for (lcmd = legacycmds ; lcmd ; lcmd = lcmd->next)
		if (!Q_strncasecmp(partial, lcmd->oldname, len))
			return lcmd->oldname;

	return NULL;
}

/*
============
Cmd_CompleteCountPossible
============
*/
int Cmd_CompleteCountPossible (char *partial)
{
	cmd_function_t	*cmd;
	legacycmd_t	*lcmd;
	int		len, c = 0;

	if (!(len = strlen(partial)))
		return 0;

	for (cmd = cmd_functions ; cmd ; cmd = cmd->next)
		if (!Q_strncasecmp(partial, cmd->name, len))
			c++;

	for (lcmd = legacycmds ; lcmd ; lcmd = lcmd->next)
		if (!Q_strncasecmp(partial, lcmd->oldname, len))
			c++;

	return c;
}

//===================================================================

int		RDFlags = 0;
direntry_t	*filelist = NULL;
int		num_files = 0;

static	char	filetype[8] = "file";

static	char	compl_common[MAX_FILELENGTH];
static	int	compl_len;
static	int	compl_clen;

static void FindCommonSubString (char *s)
{
	if (!compl_clen)
	{
		Q_strncpyz (compl_common, s, sizeof(compl_common));
		compl_clen = strlen (compl_common);
	} 
	else
	{
		while (compl_clen > compl_len && Q_strncasecmp(s, compl_common, compl_clen))
			compl_clen--;
	}
}

static void CompareParams (void)
{
	int	i;

	compl_clen = 0;

	for (i=0 ; i<num_files ; i++)
		FindCommonSubString (filelist[i].name);

	if (compl_clen)
		compl_common[compl_clen] = 0;
}

static void PrintEntries (void);
extern char key_lines[64][MAXCMDLINE];
extern int	edit_line;
extern int	key_linepos;

#define	READDIR_ALL_PATH(p)							\
	for (search = com_searchpaths ; search ; search = search->next)\
	{												\
		if (!search->pack)							\
		{											\
			RDFlags |= (RD_STRIPEXT | RD_NOERASE);	\
			if (skybox)								\
				RDFlags |= RD_SKYBOX;				\
			ReadDir (va("%s/%s", search->filename, subdir), p);\
		}											\
	}

/*
============
Cmd_CompleteParameter

parameter completion for various commands
============
*/
void Cmd_CompleteParameter (char *partial, char *attachment)
{
	char		*s, *param, stay[MAX_QPATH], subdir[MAX_QPATH] = "", param2[MAX_QPATH];
	qboolean	skybox = false;

	Q_strncpyz (stay, partial, sizeof(stay));

// we don't need "<command> + space(s)" included
	param = strrchr (stay, ' ') + 1;
	if (!*param)		// no parameter was written in, so quit
		return;

	compl_len = strlen (param);
	strcat (param, attachment);

	if (!strcmp(attachment, "*.bsp"))
	{
		Q_strncpyz (subdir, "maps/", sizeof(subdir));
	}
	else if (!strcmp(attachment, "*.tga"))
	{
		if (strstr(stay, "loadsky ") == stay || strstr(stay, "r_skybox ") == stay)
		{
			Q_strncpyz (subdir, "env/", sizeof(subdir));
			skybox = true;
		}
		else if (strstr(stay, "loadcharset ") == stay || strstr(stay, "gl_consolefont ") == stay)
		{
			Q_strncpyz (subdir, "textures/charsets/", sizeof(subdir));
		}
		else if (strstr(stay, "crosshairimage ") == stay)
		{
			Q_strncpyz (subdir, "crosshairs/", sizeof(subdir));
		}
	}

	if (strstr(stay, "gamedir ") == stay)
	{
		RDFlags |= RD_GAMEDIR;
		ReadDir (com_basedir, param);

		pak_files = 0;	// so that previous pack searches are cleared
	}
	else if (strstr(stay, "load ") == stay || strstr(stay, "printtxt ") == stay)
	{
		RDFlags |= RD_STRIPEXT;
		ReadDir (com_gamedir, param);

		pak_files = 0;	// same here
	}
	else
	{
		searchpath_t	*search;

		EraseDirEntries ();
		pak_files = 0;

		READDIR_ALL_PATH(param);
		if (!strcmp(param + strlen(param)-3, "tga"))
		{
			Q_strncpyz (param2, param, strlen(param)-3);
			strcat (param2, "png");
			READDIR_ALL_PATH(param2);
			FindFilesInPak (va("%s%s", subdir, param2));
		}
		else if (!strcmp(param + strlen(param)-3, "dem"))
		{
			Q_strncpyz (param2, param, strlen(param)-3);
			strcat (param2, "dz");
			READDIR_ALL_PATH(param2);
			FindFilesInPak (va("%s%s", subdir, param2));
		}
		FindFilesInPak (va("%s%s", subdir, param));
	}

	if (!filelist)
		return;

	s = strchr (partial, ' ') + 1;
// just made this to avoid printing the filename twice when there's only one match
	if (num_files == 1)
	{
		*s = '\0';
		strcat (partial, filelist[0].name);
		key_linepos = strlen(partial) + 1;
		key_lines[edit_line][key_linepos] = 0;
		return;
	}

	CompareParams ();

	Con_Printf ("]%s\n", partial);
	PrintEntries ();

	*s = '\0';
	strcat (partial, compl_common);
	key_linepos = strlen(partial) + 1;
	key_lines[edit_line][key_linepos] = 0;
}

/*
============
Cmd_ExecuteString

A complete command line has been parsed, so try to execute it
FIXME: lookupnoadd the token to speed search?
============
*/
void Cmd_ExecuteString (char *text, cmd_source_t src)
{	
	cmd_function_t	*cmd;
	cmd_alias_t	*a;

	cmd_source = src;
	Cmd_TokenizeString (text);

// execute the command line
	if (!Cmd_Argc())
		return;		// no tokens

// check functions
	for (cmd = cmd_functions ; cmd ; cmd = cmd->next)
	{
		if (!Q_strcasecmp(cmd_argv[0], cmd->name))
		{
			cmd->function ();
			return;
		}
	}

// check alias
	for (a = cmd_alias ; a ; a = a->next)
	{
		if (!Q_strcasecmp(cmd_argv[0], a->name))
		{
			Cbuf_InsertText (a->value);
			return;
		}
	}

// check cvars
	if (Cvar_Command())
		return;

// joe: check legacy commands
	if (Cmd_LegacyCommand())
		return;

	if (cl_warncmd.value || developer.value)
		Con_Printf ("Unknown command \"%s\"\n", Cmd_Argv(0));
}

/*
===================
Cmd_ForwardToServer

Sends the entire command line over to the server
===================
*/
void Cmd_ForwardToServer (void)
{
	if (cls.state != ca_connected)
	{
		Con_Printf ("Not connected to server\n");
		return;
	}

	if (cls.demoplayback)	// not really connected
		return;

	MSG_WriteByte (&cls.message, clc_stringcmd);
	if (Q_strcasecmp(Cmd_Argv(0), "cmd"))
	{
		SZ_Print (&cls.message, Cmd_Argv(0));
		SZ_Print (&cls.message, " ");
	}
	if (Cmd_Argc() > 1)
		SZ_Print (&cls.message, Cmd_Args());
	else
		SZ_Print (&cls.message, "\n");
}

/*
================
Cmd_CheckParm

Returns the position (1 to argc-1) in the command's argument list
where the given parameter apears, or 0 if not present
================
*/
int Cmd_CheckParm (char *parm)
{
	int	i;

	if (!parm)
		Sys_Error ("Cmd_CheckParm: NULL");

	for (i=1 ; i<Cmd_Argc() ; i++)
		if (!Q_strcasecmp(parm, Cmd_Argv(i)))
			return i;

	return 0;
}

/*
====================
Cmd_CmdList_f

List all console commands
====================
*/
void Cmd_CmdList_f (void)
{
	cmd_function_t	*cmd;
	int		counter;

	if (cmd_source != src_command)
		return;

	for (counter = 0, cmd = cmd_functions ; cmd ; cmd = cmd->next, counter++)
		Con_Printf ("%s\n", cmd->name);

	Con_Printf ("------------\n%d commands\n", counter);
}

/*
====================
Cmd_Dir_f

List all files in the mod's directory
====================
*/
void Cmd_Dir_f (void)
{
	char	myarg[MAX_FILELENGTH];

	if (cmd_source != src_command)
		return;

	if (!strcmp(Cmd_Argv(1), ""))
	{
		Q_strncpyz (myarg, "*", sizeof(myarg));
		Q_strncpyz (filetype, "file", sizeof(filetype));
	}
	else
	{
		Q_strncpyz (myarg, Cmd_Argv(1), sizeof(myarg));
		// first two are exceptional cases
		if (strstr(myarg, "*"))
			Q_strncpyz (filetype, "file", sizeof(filetype));
		else if (strstr(myarg, "*.dem"))
			Q_strncpyz (filetype, "demo", sizeof(filetype));
		else
		{
			if (strchr(myarg, '.'))
			{
				Q_strncpyz (filetype, COM_FileExtension(myarg), sizeof(filetype));
				filetype[strlen(filetype)] = 0x60;	// right-shadowed apostrophe
			}
			else
			{
				strcat (myarg, "*");
				Q_strncpyz (filetype, "file", sizeof(filetype));
			}
		}
	}

	RDFlags |= RD_COMPLAIN;
	ReadDir (com_gamedir, myarg);
	if (!filelist)
		return;

	Con_Printf ("\x02" "%ss in current folder are:\n", filetype);
	PrintEntries ();
}

static void toLower (char* str)		// for strings
{
	char	*s;
	int	i;

	i = 0;
	s = str;

	while (*s)
	{
		if (*s >= 'A' && *s <= 'Z')
			*(str + i) = *s + 32;
		i++;
		s++;
	}
}

static void AddNewEntry (char *fname, int ftype, long fsize)
{
	int	i, pos;

	filelist = Q_realloc (filelist, (num_files + 1) * sizeof(direntry_t));

#ifdef _WIN32
	toLower (fname);
	// else don't convert, linux is case sensitive
#endif

	// inclusion sort
	for (i=0 ; i<num_files ; i++)
	{
		if (ftype < filelist[i].type)
			continue;
		else if (ftype > filelist[i].type)
			break;

		if (strcmp(fname, filelist[i].name) < 0)
			break;
	}
	pos = i;
	for (i=num_files ; i>pos ; i--)
		filelist[i] = filelist[i-1];

    filelist[i].name = Q_strdup (fname);
	filelist[i].type = ftype;
	filelist[i].size = fsize;

	num_files++;
}

static void AddNewEntry_unsorted (char *fname, int ftype, long fsize)
{
	filelist = Q_realloc (filelist, (num_files + 1) * sizeof(direntry_t));

#ifdef _WIN32
	toLower (fname);
	// else don't convert, linux is case sensitive
#endif
    filelist[num_files].name = Q_strdup (fname);
	filelist[num_files].type = ftype;
	filelist[num_files].size = fsize;

	num_files++;
}

void EraseDirEntries (void)
{
	if (filelist)
	{
		free (filelist);
		filelist = NULL;
		num_files = 0;
	}
}

static qboolean CheckEntryName (char *ename)
{
	int	i;

	for (i=0 ; i<num_files ; i++)
		if (!Q_strcasecmp(ename, filelist[i].name))
			return true;

	return false;
}

qboolean IsBSPTooSmall(int bsplength)
{
	return bsplength <= (32 * 1024);
}

qboolean CheckRealBSP(char* bspname, int bsplength)
{
	if (!IsBSPTooSmall(bsplength) &&		// don't list files under 32k (ammo boxes etc)
		!strncmp(bspname, "maps/", 5) &&	// don't list files outside of maps/
		!strchr(bspname + 5, '/'))			// don't list files in subdirectories
		return true;

	return false;
}

#define SLASHJMP(x, y)	(x = !(x = strrchr(y, '/')) ? y : x + 1)

/*
=================
ReadDir
=================
*/
void ReadDir (char *path, char *the_arg)
{
#ifdef _WIN32
	HANDLE		h;
	WIN32_FIND_DATA	fd;
#else
	int		h, i = 0;
	char		*foundname;
	glob_t		fd;
	struct	stat	fileinfo;
#endif

	if (path[strlen(path)-1] == '/')
		path[strlen(path)-1] = 0;

	if (!(RDFlags & RD_NOERASE))
		EraseDirEntries ();

#ifdef _WIN32
	h = FindFirstFile (va("%s/%s", path, the_arg), &fd);
	if (h == INVALID_HANDLE_VALUE)
#else
	h = glob (va("%s/%s", path, the_arg), 0, NULL, &fd);
	if (h == GLOB_ABORTED)
#endif
	{
		if (RDFlags & RD_MENU_DEMOS)
		{
			AddNewEntry ("Error reading directory", 3, 0);
			num_files = 1;
		}
		else if (RDFlags & RD_COMPLAIN)
		{
			Con_Printf ("No such file\n");
		}
		goto end;
	}

	if (RDFlags & RD_MENU_DEMOS && !(RDFlags & RD_MENU_DEMOS_MAIN))
	{
		AddNewEntry ("..", 2, 0);
		num_files = 1;
	}

	do {
		int	fdtype;
		long	fdsize;
		char	filename[MAX_FILELENGTH];

#ifdef _WIN32
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (!(RDFlags & (RD_MENU_DEMOS | RD_GAMEDIR)) || !strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, ".."))
				continue;

			fdtype = 1;
			fdsize = 0;
			Q_strncpyz (filename, fd.cFileName, sizeof(filename));
		}
		else
		{
			char	ext[8];

			if (RDFlags & RD_GAMEDIR)
				continue;

			Q_strncpyz (ext, COM_FileExtension(fd.cFileName), sizeof(ext));

			if (RDFlags & RD_MENU_DEMOS && Q_strcasecmp(ext, "dem") && Q_strcasecmp(ext, "dz"))
				continue;

			fdtype = 0;
			fdsize = fd.nFileSizeLow;
			if (!Q_strcasecmp(ext, "bsp") && IsBSPTooSmall(fdsize))
				continue;

			if (Q_strcasecmp(ext, "dz") && RDFlags & (RD_STRIPEXT | RD_MENU_DEMOS))
			{
				COM_StripExtension (fd.cFileName, filename);
				if (RDFlags & RD_SKYBOX)
				{
					int	idx = strlen(filename) - 3;

					filename[filename[idx] == '_' ? idx : idx+1] = 0;	// cut off skybox_ext
				}
			}
			else
			{
				Q_strncpyz (filename, fd.cFileName, sizeof(filename));
			}

			if (CheckEntryName(filename))
				continue;	// file already on list
		}
#else
		if (h == GLOB_NOMATCH || !fd.gl_pathc)
			break;

		SLASHJMP(foundname, fd.gl_pathv[i]);
		stat (fd.gl_pathv[i], &fileinfo);

		if (S_ISDIR(fileinfo.st_mode))
		{
			if (!(RDFlags & (RD_MENU_DEMOS | RD_GAMEDIR)))
				continue;

			fdtype = 1;
			fdsize = 0;
			Q_strncpyz (filename, foundname, sizeof(filename));
		}
		else
		{
			char	ext[8];

			if (RDFlags & RD_GAMEDIR)
				continue;

			Q_strncpyz (ext, COM_FileExtension(foundname), sizeof(ext));

			if (RDFlags & RD_MENU_DEMOS && Q_strcasecmp(ext, "dem") && Q_strcasecmp(ext, "dz"))
				continue;

			fdtype = 0;
			fdsize = fileinfo.st_size;
			if (Q_strcasecmp(ext, "dz") && RDFlags & (RD_STRIPEXT | RD_MENU_DEMOS))
			{
				COM_StripExtension (foundname, filename);
				if (RDFlags & RD_SKYBOX)
				{
					int	idx = strlen(filename) - 3;

					filename[filename[idx] == '_' ? idx : idx+1] = 0;	// cut off skybox_ext
				}
			}
			else
			{
				Q_strncpyz (filename, foundname, sizeof(filename));
			}

			if (CheckEntryName(filename))
				continue;	// file already on list
		}
#endif
		AddNewEntry (filename, fdtype, fdsize);
	}
#ifdef _WIN32
	while (FindNextFile(h, &fd));
	FindClose (h);
#else
	while (++i < fd.gl_pathc);
	globfree (&fd);
#endif

	if (!num_files)
	{
		if (RDFlags & RD_MENU_DEMOS)
		{
			AddNewEntry ("[ no files ]", 3, 0);
			num_files = 1;
		}
		else if (RDFlags & RD_COMPLAIN)
		{
			Con_Printf ("No such file\n");
		}
	}

end:
	RDFlags = 0;
}

int	pak_files = 0;

/*
=================
FindFilesInPak

Search for files inside a PAK file
=================
*/
void FindFilesInPak (char *the_arg)
{
	int		i, l;
	searchpath_t	*search;
	pack_t		*pak;
	char		*myarg;

	SLASHJMP(myarg, the_arg);
	for (search = com_searchpaths ; search ; search = search->next)
	{
		if (search->pack)
		{
			int	extlen;
			char	*s, *p, ext[8], ext2[8], filename[MAX_FILELENGTH];

			// look through all the pak file elements
			pak = search->pack;
			for (i=0 ; i<pak->numfiles ; i++)
			{
				s = pak->files[i].name;
				l = pak->files[i].filelen;
				Q_strncpyz (ext, COM_FileExtension(s), sizeof(ext));
				Q_strncpyz (ext2, COM_FileExtension(myarg), sizeof(ext2));
				extlen = strlen(ext2);
				if (!Q_strcasecmp(ext, ext2))
				{
					int compare_length = strlen(the_arg) - 2 - extlen;
					int compare_length2 = compare_length - compl_len;
					if (compare_length2 < 0)
						compare_length2 = compare_length;

					SLASHJMP(p, s);
					if (!Q_strcasecmp(ext, "bsp") && !CheckRealBSP(s, l))
						continue;
					if (!Q_strncasecmp(s, the_arg, compare_length) ||
					    (*myarg == '*' && !Q_strncasecmp(s, the_arg, compare_length2)))
					{
						if (Q_strcasecmp(ext, "dz") && Q_strcasecmp(ext, "skin"))
							COM_StripExtension (p, filename);
						else
							Q_strncpyz (filename, p, sizeof(filename));

						if (CheckEntryName(filename))
							continue;

						AddNewEntry(filename, 0, l);
						pak_files++;
					}
				}
			}
		}
	}
}

/*
==================
PaddedPrint
==================
*/
#define	COLUMNWIDTH	20
#define	MINCOLUMNWIDTH	18	// the last column may be slightly smaller

extern	int	con_x;

static void PaddedPrint (char *s)
{
	extern	int	con_linewidth;
	int		nextcolx = 0;

	if (con_x)
		nextcolx = (int)((con_x + COLUMNWIDTH) / COLUMNWIDTH) * COLUMNWIDTH;

	if (nextcolx > con_linewidth - MINCOLUMNWIDTH || 
	    (con_x && nextcolx + strlen(s) >= con_linewidth))
		Con_Printf ("\n");

	if (con_x)
		Con_Printf (" ");
	while (con_x % COLUMNWIDTH)
		Con_Printf (" ");
	Con_Printf ("%s", s);
}

static void PrintEntries (void)
{
	int	i, filectr;

	filectr = pak_files ? (num_files - pak_files) : 0;

	for (i=0 ; i<num_files ; i++)
	{
		if (!filectr-- && pak_files)
		{
			if (con_x)
			{
				Con_Printf ("\n");
				Con_Printf ("\x02" "inside pack file:\n");
			}
			else
			{
				Con_Printf ("\x02" "inside pack file:\n");
			}
		}
		PaddedPrint (filelist[i].name);
	}

	if (con_x)
		Con_Printf ("\n");
}

/*
==================
CompleteCommand

Advanced command completion
==================
*/
void CompleteCommand (void)
{
	int	c, v;
	char	*s, *cmd;

	s = key_lines[edit_line] + 1;
	if (!(compl_len = strlen(s)))
		return;
	compl_clen = 0;

	c = Cmd_CompleteCountPossible (s);
	v = Cvar_CompleteCountPossible (s);

	if (c + v > 1)
	{
		Con_Printf ("\n");

		if (c)
		{
			cmd_function_t	*cmd;
			legacycmd_t	*lcmd;

			Con_Printf ("\x02" "commands:\n");
			// check commands
			for (cmd = cmd_functions ; cmd ; cmd = cmd->next)
			{
				if (!Q_strncasecmp(s, cmd->name, compl_len))
				{
					PaddedPrint (cmd->name);
					FindCommonSubString (cmd->name);
				}
			}

			// joe: check for legacy commands also
			for (lcmd = legacycmds ; lcmd ; lcmd = lcmd->next)
			{
				if (!Q_strncasecmp(s, lcmd->oldname, compl_len))
				{
					PaddedPrint (lcmd->oldname);
					FindCommonSubString (lcmd->oldname);
				}
			}

			if (con_x)
				Con_Printf ("\n");
		}

		if (v)
		{
			cvar_t		*var;

			Con_Printf ("\x02" "variables:\n");
			// check variables
			for (var = cvar_vars ; var ; var = var->next)
			{
				if (!Q_strncasecmp(s, var->name, compl_len))
				{
					PaddedPrint (var->name);
					FindCommonSubString (var->name);
				}
			}

			if (con_x)
				Con_Printf ("\n");
		}
	}

	if (c + v == 1)
	{
		if (!(cmd = Cmd_CompleteCommand(s)))
			cmd = Cvar_CompleteVariable (s);
	}
	else if (compl_clen)
	{
		compl_common[compl_clen] = 0;
		cmd = compl_common;
	}
	else
		return;

	strcpy (key_lines[edit_line]+1, cmd);
	key_linepos = strlen(cmd) + 1;
	if (c + v == 1)
		key_lines[edit_line][key_linepos++] = ' ';
	key_lines[edit_line][key_linepos] = 0;
}

/*
====================
Cmd_DemDir_f

List all demo files
====================
*/
void Cmd_DemDir_f (void)
{
	char	myarg[MAX_FILELENGTH];

	if (cmd_source != src_command)
		return;

	if (!strcmp(Cmd_Argv(1), ""))
	{
		Q_strncpyz (myarg, "*.dem", sizeof(myarg));
	}
	else
	{
		Q_strncpyz (myarg, Cmd_Argv(1), sizeof(myarg));
		if (strchr(myarg, '.'))
		{
			Con_Printf ("You needn`t use dots in demdir parameters\n");
			if (strcmp(COM_FileExtension(myarg), "dem"))
			{
				Con_Printf ("demdir is for demo files only\n");
				return;
			}
		}
		else
		{
			strcat (myarg, "*.dem");
		}
	}

	Q_strncpyz (filetype, "demo", sizeof(filetype));

	RDFlags |= (RD_STRIPEXT | RD_COMPLAIN);
	ReadDir (com_gamedir, myarg);
	if (!filelist)
		return;

	Con_Printf ("\x02" "%ss in current folder are:\n", filetype);
	PrintEntries ();
}

/*
====================
AddTabs

Replaces nasty tab character with spaces
====================
*/
static void AddTabs (char *buf)
{
	unsigned char	*s, tmp[256];
	int		i;

	for (s = buf, i = 0 ; *s ; s++, i++)
	{
		switch (*s)
		{
		case 0xb4:
		case 0x27:
			*s = 0x60;
			break;

		case '\t':
			strcpy (tmp, s + 1);
			while (i++ < 8)
				*s++ = ' ';
			*s-- = '\0';
			strcat (buf, tmp);
			break;
		}

		if (i >= 7)
			i = -1;
	}
}

/*
====================
Cmd_PrintTxt_f

Prints a text file into the console
====================
*/
void Cmd_PrintTxt_f (void)
{
	char	name[MAX_FILELENGTH], buf[256] = {0};
	FILE	*f;

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("printtxt <txtfile> : prints a text file\n");
		return;
	}

	Q_strncpyz (name, Cmd_Argv(1), sizeof(name));

	COM_DefaultExtension (name, ".txt");

	Q_strncpyz (buf, va("%s/%s", com_gamedir, name), sizeof(buf));
	if (!(f = fopen(buf, "rt")))
	{
		Con_Printf ("ERROR: couldn't open %s\n", name);
		return;
	}

	Con_Printf ("\n");
	while (fgets(buf, 256, f))
	{
		AddTabs (buf);
		Con_Printf ("%s", buf);
		memset (buf, 0, sizeof(buf));
	}

	Con_Printf ("\n\n");
	fclose (f);
}

static char* Cmd_TintSubstring(const char* in, const char* substr, char* out, size_t outsize)
{
	int		l;
	char	*m;

	Q_strlcpy(out, in, outsize);
	while ((m = Q_strcasestr(out, substr)))
	{
		l = strlen(substr);
		while (l-- > 0)
			if (*m >= ' ' && *m < 127)
				*m++ |= 0x80;
	}
	return out;
}

/*
============
Cmd_Apropos_f
scans through each command and cvar names+descriptions for the given substring
we don't support descriptions, so this isn't really all that useful, but even without the sake of consistency it still combines cvars+commands under a single command.
============
*/
void Cmd_Apropos_f(void)
{
	char		tmpbuf[256];
	int			hits = 0;
	cmd_function_t *cmd;
	cvar_t		*var;
	const char	*substr = Cmd_Argv(1);

	if (!*substr)
	{
		Con_Printf("%s <substring> : search through commands and cvars for the given substring\n", Cmd_Argv(0));
		return;
	}
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		if (Q_strcasestr(cmd->name, substr))
		{
			hits++;
			Con_Printf("%s\n", Cmd_TintSubstring(cmd->name, substr, tmpbuf, sizeof(tmpbuf)));
		}
	}

	for (var = Cvar_FindVarAfter("", 0); var; var = var->next)
	{
		if (Q_strcasestr(var->name, substr))
		{
			hits++;
			Con_Printf("%s (current value: \"%s\")\n", Cmd_TintSubstring(var->name, substr, tmpbuf, sizeof(tmpbuf)), var->string);
		}
	}
	if (!hits)
		Con_Printf("no cvars nor commands contain that substring\n");
}

/*
============
Cmd_Init
============
*/
void Cmd_Init (void)
{
// register our commands
	Cmd_AddCommand ("stuffcmds", Cmd_StuffCmds_f);
	Cmd_AddCommand ("exec", Cmd_Exec_f);
	Cmd_AddCommand ("echo", Cmd_Echo_f);
	Cmd_AddCommand ("alias", Cmd_Alias_f);
	Cmd_AddCommand ("cmd", Cmd_ForwardToServer);
	Cmd_AddCommand ("wait", Cmd_Wait_f);
	Cmd_AddCommand("apropos", Cmd_Apropos_f);
	Cmd_AddCommand("find", Cmd_Apropos_f);

	Cmd_AddCommand ("cmdlist", Cmd_CmdList_f);
	Cmd_AddCommand ("dir", Cmd_Dir_f);
	Cmd_AddCommand ("demdir", Cmd_DemDir_f);
	Cmd_AddCommand ("printtxt", Cmd_PrintTxt_f);
}
