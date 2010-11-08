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
// iplog.h

typedef struct iplog_s
{
	int		addr;
	char		name[16];
	struct iplog_s	*parent;
	struct iplog_s	*children[2];
} iplog_t;

extern int iplog_size;

void IPLog_Init (void);
void IPLog_WriteLog (void);
void IPLog_Add (int addr, char *name);
void IPLog_Delete (iplog_t *node);
iplog_t *IPLog_Merge (iplog_t *left, iplog_t *right);
void IPLog_Identify (int addr);
void IPLog_Identify_f (void);
void IPLog_Dump (void);
void IPLog_Import (void);
