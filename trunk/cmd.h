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
// cmd.h -- Command buffer and command execution

//===========================================================================

/*

Any number of commands can be added in a frame, from several different sources.
Most commands come from either keybindings or console line input, but remote
servers can also send across commands and entire text files can be execed.

The + command line options are also added to the command buffer.

The game starts with a Cbuf_AddText ("exec quake.rc\n"); Cbuf_Execute ();

*/


void Cbuf_Init (void);
// allocates an initial text buffer that will grow as needed

void Cbuf_AddText (char *text);
// as new commands are generated from the console or keybindings,
// the text is added to the end of the command buffer.

void Cbuf_InsertText (char *text);
// when a command wants to issue other commands immediately, the text is
// inserted at the beginning of the buffer, before any remaining unexecuted
// commands.

void Cbuf_Execute (void);
// Pulls off \n terminated lines of text from the command buffer and sends
// them through Cmd_ExecuteString.  Stops when the buffer is empty.
// Normally called once per frame, but may be explicitly invoked.
// Do not call inside a command function!

//===========================================================================

/*

Command execution takes a null terminated string, breaks it into tokens,
then searches for a command or variable that matches the first token.

Commands can come from three sources, but the handler functions may choose
to dissallow the action or forward it to a remote server if the source is
not apropriate.

*/

typedef void (*xcommand_t) (void);

typedef enum
{
	src_client,		// came in over a net connection as a clc_stringcmd
				// host_client will be valid during this state.
	src_command		// from the command buffer
} cmd_source_t;

extern	cmd_source_t	cmd_source;

#define	MAX_FILELENGTH	64

typedef struct direntry_s
{
	int	type;	// 0 = file, 1 = dir, 2 = ".."
	char	*name;
	int	size;
    float total_time;
    int kills;
    int total_kills;
    int secrets;
    int total_secrets;
    int skill;
    int num_maps;
    char mapname[30];
    char playername[30];
} direntry_t;

extern	direntry_t	*filelist;
extern	int		num_files;
extern	int		RDFlags;

#define	RD_MENU_DEMOS		1	// for demos menu printing
#define	RD_MENU_DEMOS_MAIN	2	// to avoid printing ".." in the main Quake folder
#define	RD_COMPLAIN		4	// to avoid printing "No such file"
#define	RD_STRIPEXT		8	// for stripping file's extension
#define	RD_NOERASE		16	// to avoid deleting the filelist
#define	RD_SKYBOX		32	// for skyboxes
#define	RD_GAMEDIR		64	// for the "gamedir" command

void EraseDirEntries (void);

void Cmd_CmdList_f (void);
void Cmd_CvarList_f (void);
void Cmd_Dir_f (void);
void Cmd_DemDir_f (void);

void ReadDir (char *path, char *the_arg);

extern	int	pak_files;

void FindFilesInPak (char *the_arg);
void Cmd_CompleteParameter (char *partial, char *attachment);
void CompleteCommand (void);

typedef struct cmd_function_s
{
	struct cmd_function_s	*next;
	char			*name;
	xcommand_t		function;
} cmd_function_t;

void Cmd_StuffCmds_f (void);
void Cbuf_AddEarlyCommands (void);
void Cmd_Init (void);

void Cmd_AddCommand (char *cmd_name, xcommand_t function);
// called by the init functions of other parts of the program to
// register commands and functions to call for them.
// The cmd_name is referenced later, so it should not be in temp memory

cmd_function_t *Cmd_FindCommand (char *cmd_name);
// used by the cvar code to check for cvar / command name overlap

char *Cmd_CompleteCommand (char *partial);
// attempts to match a partial command for automatic command line completion
// returns NULL if nothing fits

int Cmd_Argc (void);
char *Cmd_Argv (int arg);
char *Cmd_Args (void);
// The functions that execute commands get their parameters with these
// functions. Cmd_Argv () will return an empty string, not a NULL
// if arg > argc, so string operations are always safe.

int Cmd_CheckParm (char *parm);
// Returns the position (1 to argc-1) in the command's argument list
// where the given parameter apears, or 0 if not present

void Cmd_TokenizeString (char *text);
// Takes a null terminated string.  Does not need to be /n terminated.
// breaks the string up into arg tokens.

void Cmd_ExecuteString (char *text, cmd_source_t src);
// Parses a single line of text into arguments and tries to execute it.
// The text can come from the command buffer, a remote client, or stdin.

void Cmd_ForwardToServer (void);
// adds the current command line as a clc_stringcmd to the client message.
// things like godmode, noclip, etc, are commands directed to the server,
// so when they are typed in at the console, they will need to be forwarded.

void Cmd_Print (char *text);
// used by command functions to send output to either the graphics console or
// passed as a print message to the client

typedef struct legacycmd_s
{
	char		*oldname, *newname;
	struct legacycmd_s *next;
} legacycmd_t;

void Cmd_AddLegacyCommand (char *oldname, char *newname);
legacycmd_t *Cmd_FindLegacyCommand (char *oldname);

#define	MAX_ALIAS_NAME	32

typedef struct cmd_alias_s
{
	struct cmd_alias_s *next;
	char		name[MAX_ALIAS_NAME];
	char		*value;
} cmd_alias_t;

cmd_alias_t *Cmd_FindAlias (char *name);
