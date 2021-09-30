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
// net_dgrm.c

// This is enables a simple IP banning mechanism
#define BAN_TEST

#ifdef BAN_TEST
#if defined(_WIN32)
#include <windows.h>
#elif defined (NeXT)
#include <sys/socket.h>
#include <arpa/inet.h>
#else
#define	AF_INET		2	/* internet */
struct in_addr
{
	union
	{
		struct { unsigned char s_b1, s_b2, s_b3, s_b4; } S_un_b;
		struct { unsigned short s_w1, s_w2; } S_un_w;
		unsigned long S_addr;
	} S_un;
};
#define	s_addr	S_un.S_addr	/* can be used for most tcp & ip code */
struct sockaddr_in
{
	short		sin_family;
	unsigned short	sin_port;
	struct in_addr	sin_addr;
	char		sin_zero[8];
};
char *inet_ntoa (struct in_addr in);
unsigned long inet_addr (const char *cp);
#endif
#endif	// BAN_TEST

#include "quakedef.h"
#include "net_dgrm.h"

// these two macros are to make the code more readable
#define sfunc	net_landrivers[sock->landriver]
#define dfunc	net_landrivers[net_landriverlevel]

static int net_landriverlevel;

// statistic counters
int	packetsSent = 0, packetsReSent = 0, packetsReceived = 0;
int	receivedDuplicateCount = 0, shortPacketCount = 0;
int	droppedDatagrams;

static	int	myDriverLevel;

struct
{
	unsigned int	length;
	unsigned int	sequence;
	byte		data[MAX_DATAGRAM];
} packetBuffer;

extern int	m_return_state;
extern int	m_state;
extern qboolean	m_return_onerror;
extern char	m_return_reason[32];

#define MOD_PROQUAKE_VERSION	3.50 // joe: imported ProQuake engine's version to keep compatibility
extern	char	*argv[MAX_NUM_ARGVS];

#if 0	// joe: removed DEBUG
char *StrAddr (struct qsockaddr *addr)
{
	static	char	buf[34];
	byte	*p = (byte *)addr;
	int	n;

	for (n=0 ; n<16 ; n++)
		sprintf (buf + n * 2, "%02x", *p++);

	return buf;
}
#endif

#ifdef BAN_TEST
unsigned long	banAddr = 0x00000000;
unsigned long	banMask = 0xffffffff;

void NET_Ban_f (void)
{
	char	addrStr[32], maskStr[32];
	void	(*print)(char *fmt, ...);

	if (cmd_source == src_command)
	{
		if (!sv.active)
		{
			Cmd_ForwardToServer ();
			return;
		}
		print = Con_Printf;
	}
	else
	{
		if (pr_global_struct->deathmatch)
			return;
		print = SV_ClientPrintf;
	}

	switch (Cmd_Argc())
	{
		case 1:
			if (((struct in_addr *)&banAddr)->s_addr)
			{
				Q_strcpy (addrStr, inet_ntoa(*(struct in_addr *)&banAddr));
				Q_strcpy (maskStr, inet_ntoa(*(struct in_addr *)&banMask));
				print ("Banning %s [%s]\n", addrStr, maskStr);
			}
			else
				print ("Banning not active\n");
			break;

		case 2:
			if (!Q_strcasecmp(Cmd_Argv(1), "off"))
				banAddr = 0x00000000;
			else
				banAddr = inet_addr (Cmd_Argv(1));
			banMask = 0xffffffff;
			break;

		case 3:
			banAddr = inet_addr (Cmd_Argv(1));
			banMask = inet_addr (Cmd_Argv(2));
			break;

		default:
			print ("BAN ip_address [mask]\n");
			break;
	}
}
#endif

int Datagram_SendMessage (qsocket_t *sock, sizebuf_t *data)
{
	unsigned int	packetLen, dataLen, eom;

#if 0	// joe: removed DEBUG
	if (data->cursize == 0)
		Host_Error ("Datagram_SendMessage: zero length message");

	if (data->cursize > NET_MAXMESSAGE)
		Host_Error ("Datagram_SendMessage: message too big %u", data->cursize);

	if (sock->canSend == false)
		Host_Error ("SendMessage: called with canSend == false");
#endif

	memcpy (sock->sendMessage, data->data, data->cursize);
	sock->sendMessageLength = data->cursize;

	if (data->cursize <= MAX_DATAGRAM)
	{
		dataLen = data->cursize;
		eom = NETFLAG_EOM;
	}
	else
	{
		dataLen = MAX_DATAGRAM;
		eom = 0;
	}
	packetLen = NET_HEADERSIZE + dataLen;

	packetBuffer.length = BigLong (packetLen | (NETFLAG_DATA | eom));
	packetBuffer.sequence = BigLong (sock->sendSequence++);
	memcpy (packetBuffer.data, sock->sendMessage, dataLen);

	sock->canSend = false;

	if (sfunc.Write(sock->socket, (byte *)&packetBuffer, packetLen, &sock->addr) == -1)
		return -1;

	sock->lastSendTime = net_time;
	packetsSent++;

	return 1;
}

int SendMessageNext (qsocket_t *sock)
{
	unsigned int	packetLen, dataLen, eom;

	if (sock->sendMessageLength <= MAX_DATAGRAM)
	{
		dataLen = sock->sendMessageLength;
		eom = NETFLAG_EOM;
	}
	else
	{
		dataLen = MAX_DATAGRAM;
		eom = 0;
	}
	packetLen = NET_HEADERSIZE + dataLen;

	packetBuffer.length = BigLong (packetLen | (NETFLAG_DATA | eom));
	packetBuffer.sequence = BigLong (sock->sendSequence++);
	memcpy (packetBuffer.data, sock->sendMessage, dataLen);

	sock->sendNext = false;

	if (sfunc.Write(sock->socket, (byte *)&packetBuffer, packetLen, &sock->addr) == -1)
		return -1;

	sock->lastSendTime = net_time;
	packetsSent++;

	return 1;
}

int ReSendMessage (qsocket_t *sock)
{
	unsigned int	packetLen, dataLen, eom;

	if (sock->sendMessageLength <= MAX_DATAGRAM)
	{
		dataLen = sock->sendMessageLength;
		eom = NETFLAG_EOM;
	}
	else
	{
		dataLen = MAX_DATAGRAM;
		eom = 0;
	}
	packetLen = NET_HEADERSIZE + dataLen;

	packetBuffer.length = BigLong (packetLen | (NETFLAG_DATA | eom));
	packetBuffer.sequence = BigLong (sock->sendSequence - 1);
	memcpy (packetBuffer.data, sock->sendMessage, dataLen);

	sock->sendNext = false;

	if (sfunc.Write(sock->socket, (byte *)&packetBuffer, packetLen, &sock->addr) == -1)
		return -1;

	sock->lastSendTime = net_time;
	packetsReSent++;

	return 1;
}

qboolean Datagram_CanSendMessage (qsocket_t *sock)
{
	if (sock->sendNext)
		SendMessageNext (sock);

	return sock->canSend;
}

qboolean Datagram_CanSendUnreliableMessage (qsocket_t *sock)
{
	return true;
}

int Datagram_SendUnreliableMessage (qsocket_t *sock, sizebuf_t *data)
{
	int 	packetLen;

#if 0	// joe: removed DEBUG
	if (data->cursize == 0)
		Host_Error ("Datagram_SendUnreliableMessage: zero length message");

	if (data->cursize > MAX_DATAGRAM)
		Host_Error ("Datagram_SendUnreliableMessage: message too big %u", data->cursize);
#endif

	packetLen = NET_HEADERSIZE + data->cursize;

	packetBuffer.length = BigLong (packetLen | NETFLAG_UNRELIABLE);
	packetBuffer.sequence = BigLong (sock->unreliableSendSequence++);
	memcpy (packetBuffer.data, data->data, data->cursize);

	if (sfunc.Write(sock->socket, (byte *)&packetBuffer, packetLen, &sock->addr) == -1)
		return -1;

	packetsSent++;
	return 1;
}

int Datagram_GetMessage (qsocket_t *sock)
{
	unsigned int	length, flags;
	int		ret = 0;
	struct qsockaddr readaddr;
	unsigned int	sequence, count;

	if (!sock->canSend)
		if ((net_time - sock->lastSendTime) > 1.0)
			ReSendMessage (sock);

	while(1)
	{	
		length = sfunc.Read(sock->socket, (byte *)&packetBuffer, NET_DATAGRAMSIZE, &readaddr);

//	if ((rand() & 255) > 220)
//		continue;

		if (length == 0)
			break;

		if (length == -1)
		{
			Con_Printf ("Read error\n");
			return -1;
		}

		// joe: added NAT fix from ProQuake
		if (!sock->net_wait && sfunc.AddrCompare(&readaddr, &sock->addr) != 0)
		{
#if 0	// joe: removed DEBUG
			Con_DPrintf ("Forged packet received\n");
			Con_DPrintf ("Expected: %s\n", StrAddr(&sock->addr));
			Con_DPrintf ("Received: %s\n", StrAddr(&readaddr));
#endif
			continue;
		}

		if (length < NET_HEADERSIZE)
		{
			shortPacketCount++;
			continue;
		}

		length = BigLong (packetBuffer.length);
		flags = length & (~NETFLAG_LENGTH_MASK);
		length &= NETFLAG_LENGTH_MASK;

		// joe, from ProQuake: fix for attack that crashes server
		if (length > NET_DATAGRAMSIZE)
		{
			Con_Printf ("Invalid length\n");
			return -1;
		}

		if (flags & NETFLAG_CTL)
			continue;

		sequence = BigLong (packetBuffer.sequence);
		packetsReceived++;

		// joe: NAT fix from ProQuake
		if (sock->net_wait)
		{
			sock->addr = readaddr;
			Q_strcpy (sock->address, sfunc.AddrToString(&readaddr));
			sock->net_wait = false;
		}

		if (flags & NETFLAG_UNRELIABLE)
		{
			if (sequence < sock->unreliableReceiveSequence)
			{
				Con_DPrintf ("Got a stale datagram\n");
				ret = 0;
				break;
			}
			if (sequence != sock->unreliableReceiveSequence)
			{
				count = sequence - sock->unreliableReceiveSequence;
				droppedDatagrams += count;
				Con_DPrintf ("Dropped %u datagram(s)\n", count);
			}
			sock->unreliableReceiveSequence = sequence + 1;

			length -= NET_HEADERSIZE;

			SZ_Clear (&net_message);
			SZ_Write (&net_message, packetBuffer.data, length);

			ret = 2;
			break;
		}

		if (flags & NETFLAG_ACK)
		{
			if (sequence != (sock->sendSequence - 1))
			{
				Con_DPrintf ("Stale ACK received\n");
				continue;
			}
			if (sequence == sock->ackSequence)
			{
				sock->ackSequence++;
				if (sock->ackSequence != sock->sendSequence)
					Con_DPrintf ("ack sequencing error\n");
			}
			else
			{
				Con_DPrintf ("Duplicate ACK received\n");
				continue;
			}
			sock->sendMessageLength -= MAX_DATAGRAM;
			if (sock->sendMessageLength > 0)
			{
				memmove (sock->sendMessage, sock->sendMessage + MAX_DATAGRAM, sock->sendMessageLength);
				sock->sendNext = true;
			}
			else
			{
				sock->sendMessageLength = 0;
				sock->canSend = true;
			}
			continue;
		}

		if (flags & NETFLAG_DATA)
		{
			packetBuffer.length = BigLong (NET_HEADERSIZE | NETFLAG_ACK);
			packetBuffer.sequence = BigLong (sequence);
			sfunc.Write (sock->socket, (byte *)&packetBuffer, NET_HEADERSIZE, &readaddr);

			if (sequence != sock->receiveSequence)
			{
				receivedDuplicateCount++;
				continue;
			}
			sock->receiveSequence++;

			length -= NET_HEADERSIZE;

			if (flags & NETFLAG_EOM)
			{
				SZ_Clear (&net_message);
				SZ_Write (&net_message, sock->receiveMessage, sock->receiveMessageLength);
				SZ_Write (&net_message, packetBuffer.data, length);
				sock->receiveMessageLength = 0;

				ret = 1;
				break;
			}

			memcpy (sock->receiveMessage + sock->receiveMessageLength, packetBuffer.data, length);
			sock->receiveMessageLength += length;
			continue;
		}
	}

	if (sock->sendNext)
		SendMessageNext (sock);

	return ret;
}

void PrintStats (qsocket_t *s)
{
	Con_Printf ("canSend = %4u   \n", s->canSend);
	Con_Printf ("sendSeq = %4u   ", s->sendSequence);
	Con_Printf ("recvSeq = %4u   \n", s->receiveSequence);
	Con_Printf ("\n");
}

void NET_Stats_f (void)
{
	qsocket_t	*s;

	if (Cmd_Argc () == 1)
	{
		Con_Printf ("unreliable messages sent   = %i\n", unreliableMessagesSent);
		Con_Printf ("unreliable messages recv   = %i\n", unreliableMessagesReceived);
		Con_Printf ("reliable messages sent     = %i\n", messagesSent);
		Con_Printf ("reliable messages received = %i\n", messagesReceived);
		Con_Printf ("packetsSent                = %i\n", packetsSent);
		Con_Printf ("packetsReSent              = %i\n", packetsReSent);
		Con_Printf ("packetsReceived            = %i\n", packetsReceived);
		Con_Printf ("receivedDuplicateCount     = %i\n", receivedDuplicateCount);
		Con_Printf ("shortPacketCount           = %i\n", shortPacketCount);
		Con_Printf ("droppedDatagrams           = %i\n", droppedDatagrams);
	}
	else if (!strcmp(Cmd_Argv(1), "*"))
	{
		for (s = net_activeSockets ; s ; s = s->next)
			PrintStats (s);
		for (s = net_freeSockets ; s ; s = s->next)
			PrintStats (s);
	}
	else
	{
		for (s = net_activeSockets ; s ; s = s->next)
			if (!Q_strcasecmp(Cmd_Argv(1), s->address))
				break;
		if (!s)
			for (s = net_freeSockets ; s ; s = s->next)
				if (!Q_strcasecmp(Cmd_Argv(1), s->address))
					break;
		if (!s)
			return;
		PrintStats (s);
	}
}

// joe, from ProQuake: recognize ip:port
void Strip_Port (char *ch)
{
	if ((ch = strchr(ch, ':')))
	{
		int	old_port = net_hostport;

		sscanf (ch+1, "%d", &net_hostport);
		for ( ; ch[-1] == ' ' ; ch--)
			;
		*ch = 0;
		if (net_hostport != old_port)
			Con_Printf ("Setting port to %d\n", net_hostport);
	}
}

static qboolean testInProgress = false;
static	int	testPollCount;
static	int	testDriver;
static	int	testSocket;

static void Test_Poll (void);
PollProcedure	testPollProcedure = {NULL, 0.0, Test_Poll};

static void Test_Poll (void)
{
	struct qsockaddr clientaddr;
	int	control, len, colors, frags, connectTime;
	char	name[32], address[64];
	byte	playerNumber;

	net_landriverlevel = testDriver;

	while (1)
	{
		len = dfunc.Read(testSocket, net_message.data, net_message.maxsize, &clientaddr);
		if (len < sizeof(int))
			break;

		net_message.cursize = len;

		MSG_BeginReading ();
		control = BigLong (*((int *)net_message.data));
		MSG_ReadLong ();
		if (control == -1)
			break;
		if ((control & (~NETFLAG_LENGTH_MASK)) !=  NETFLAG_CTL)
			break;
		if ((control & NETFLAG_LENGTH_MASK) != len)
			break;

		if (MSG_ReadByte() != CCREP_PLAYER_INFO)
		{	// joe: changed from Sys_Error to Con_Printf as in ProQuake
			Con_Printf ("Unexpected response to Player Info request\n");
			break;
		}

		playerNumber = MSG_ReadByte ();
		Q_strcpy (name, MSG_ReadString());
		colors = MSG_ReadLong ();
		frags = MSG_ReadLong ();
		connectTime = MSG_ReadLong ();
		Q_strcpy (address, MSG_ReadString());

		Con_Printf ("%s\n  frags:%3i  colors:%u %u  time:%u\n  %s\n", name, frags, colors >> 4, colors & 0x0f, connectTime / 60, address);
	}

	testPollCount--;
	if (testPollCount)
	{
		SchedulePollProcedure (&testPollProcedure, 0.1);
	}
	else
	{
		dfunc.CloseSocket (testSocket);
		testInProgress = false;
	}
}

static void Test_f (void)
{
	char	*host;
	int	n, max = MAX_SCOREBOARD;
	struct qsockaddr sendaddr;

	if (testInProgress)
	{	// joe: added error message from ProQuake
		Con_Printf ("There is already a test/rcon in progress\n");
		return;
	}

	if (Cmd_Argc() < 2)
	{
		Con_Printf ("Usage: test <host>\n");
		return;
	}

	host = Cmd_Argv (1);
	Strip_Port (host);

	if (host && hostCacheCount)
	{
		for (n=0 ; n<hostCacheCount ; n++)
			if (!Q_strcasecmp(host, hostcache[n].name))
			{
				if (hostcache[n].driver != myDriverLevel)
					continue;
				net_landriverlevel = hostcache[n].ldriver;
				max = hostcache[n].maxusers;
				memcpy (&sendaddr, &hostcache[n].addr, sizeof(struct qsockaddr));
				break;
			}
		if (n < hostCacheCount)
			goto JustDoIt;
	}

	for (net_landriverlevel = 0 ; net_landriverlevel < net_numlandrivers ; net_landriverlevel++)
	{
		if (!net_landrivers[net_landriverlevel].initialized)
			continue;

		// see if we can resolve the host name
		if (dfunc.GetAddrFromName(host, &sendaddr) != -1)
			break;
	}
	if (net_landriverlevel == net_numlandrivers)
	{	// joe: added error message from ProQuake
		Con_Printf ("Could not resolve %s\n", host);
		return;
	}

JustDoIt:
	testSocket = dfunc.OpenSocket(0);
	if (testSocket == -1)
	{	// joe: added error message from ProQuake
		Con_Printf ("Could not open socket\n");
		return;
	}

	testInProgress = true;
	testPollCount = 20;
	testDriver = net_landriverlevel;

	for (n=0 ; n<max ; n++)
	{
		SZ_Clear (&net_message);
		// save space for the header, filled in later
		MSG_WriteLong (&net_message, 0);
		MSG_WriteByte (&net_message, CCREQ_PLAYER_INFO);
		MSG_WriteByte (&net_message, n);
		*((int *)net_message.data) = BigLong (NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		dfunc.Write (testSocket, net_message.data, net_message.cursize, &sendaddr);
	}
	SZ_Clear (&net_message);
	SchedulePollProcedure (&testPollProcedure, 0.1);
}

/*
static qboolean test2InProgress = false;
static int		test2Driver;
static int		test2Socket;
*/

static void Test2_Poll (void);
PollProcedure	test2PollProcedure = {NULL, 0.0, Test2_Poll};

static void Test2_Poll (void)
{
	struct qsockaddr clientaddr;
	int		control, len;
	char	name[256], value[256];

	net_landriverlevel = testDriver;
	name[0] = 0;

	len = dfunc.Read(testSocket, net_message.data, net_message.maxsize, &clientaddr);
	if (len < sizeof(int))
		goto Reschedule;

	net_message.cursize = len;

	MSG_BeginReading ();
	control = BigLong (*((int *)net_message.data));
	MSG_ReadLong();
	if (control == -1)
		goto Error;
	if ((control & (~NETFLAG_LENGTH_MASK)) !=  NETFLAG_CTL)
		goto Error;
	if ((control & NETFLAG_LENGTH_MASK) != len)
		goto Error;

	if (MSG_ReadByte() != CCREP_RULE_INFO)
		goto Error;

	Q_strcpy (name, MSG_ReadString());
	if (name[0] == 0)
		goto Done;
	Q_strcpy (value, MSG_ReadString());

	Con_Printf ("%-16.16s  %-16.16s\n", name, value);

	SZ_Clear (&net_message);
	// save space for the header, filled in later
	MSG_WriteLong (&net_message, 0);
	MSG_WriteByte (&net_message, CCREQ_RULE_INFO);
	MSG_WriteString (&net_message, name);
	*((int *)net_message.data) = BigLong (NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
	dfunc.Write(testSocket, net_message.data, net_message.cursize, &clientaddr);
	SZ_Clear (&net_message);

Reschedule:
	// joe: added poll counter from ProQuake
	testPollCount--;
	if (testPollCount)
	{
		SchedulePollProcedure (&test2PollProcedure, 0.05);
		return;
	}
	goto Done;

Error:
	Con_Printf ("Unexpected response to Rule Info request\n");

Done:
	dfunc.CloseSocket (testSocket);
	testInProgress = false;

	return;
}

static void Test2_f (void)
{
	char	*host;
	int	n;
	struct qsockaddr sendaddr;

	if (testInProgress)
	{	// joe: added error message from ProQuake
		Con_Printf ("There is already a test/rcon in progress\n");
		return;
	}

	if (Cmd_Argc() < 2)
	{
		Con_Printf ("Usage: test2 <host>\n");
		return;
	}

	host = Cmd_Argv (1);
	Strip_Port (host);

	if (host && hostCacheCount)
	{
		for (n=0 ; n<hostCacheCount ; n++)
			if (!Q_strcasecmp(host, hostcache[n].name))
			{
				if (hostcache[n].driver != myDriverLevel)
					continue;
				net_landriverlevel = hostcache[n].ldriver;
				memcpy (&sendaddr, &hostcache[n].addr, sizeof(struct qsockaddr));
				break;
			}
		if (n < hostCacheCount)
			goto JustDoIt;
	}

	for (net_landriverlevel = 0 ; net_landriverlevel < net_numlandrivers ; net_landriverlevel++)
	{
		if (!net_landrivers[net_landriverlevel].initialized)
			continue;

		// see if we can resolve the host name
		if (dfunc.GetAddrFromName(host, &sendaddr) != -1)
			break;
	}
	if (net_landriverlevel == net_numlandrivers)
	{	// joe: added error message from ProQuake
		Con_Printf ("Could not resolve %s\n", host);
		return;
	}

JustDoIt:
	testSocket = dfunc.OpenSocket(0);
	if (testSocket == -1)
	{	// joe: added error message from ProQuake
		Con_Printf ("Could not open socket\n");
		return;
	}

	testInProgress = true;
	testPollCount = 20;			// joe: added this from ProQuake
	testDriver = net_landriverlevel;

	SZ_Clear (&net_message);
	// save space for the header, filled in later
	MSG_WriteLong (&net_message, 0);
	MSG_WriteByte (&net_message, CCREQ_RULE_INFO);
	MSG_WriteString (&net_message, "");
	*((int *)net_message.data) = BigLong (NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
	dfunc.Write (testSocket, net_message.data, net_message.cursize, &sendaddr);
	SZ_Clear (&net_message);
	SchedulePollProcedure (&test2PollProcedure, 0.05);
}

static void Rcon_Poll (void);
PollProcedure	rconPollProcedure = {NULL, 0.0, Rcon_Poll};

static void Rcon_Poll (void)
{
	struct qsockaddr clientaddr;
	int	control, len;

	net_landriverlevel = testDriver;

	len = dfunc.Read(testSocket, net_message.data, net_message.maxsize, &clientaddr);
	if (len < sizeof(int))
	{
		testPollCount--;
		if (testPollCount)
		{
			SchedulePollProcedure (&rconPollProcedure, 0.25);
			return;
		}
		Con_Printf ("rcon: no response\n");
		goto Done;
	}

	net_message.cursize = len;

	MSG_BeginReading ();
	control = BigLong(*((int *)net_message.data));
	MSG_ReadLong ();
	if (control == -1)
		goto Error;
	if ((control & (~NETFLAG_LENGTH_MASK)) !=  NETFLAG_CTL)
		goto Error;
	if ((control & NETFLAG_LENGTH_MASK) != len)
		goto Error;

	if (MSG_ReadByte() != CCREP_RCON)
		goto Error;

	Con_Printf ("%s\n", MSG_ReadString());

	goto Done;

Error:
	Con_Printf ("Unexpected repsonse to rcon command\n");

Done:
	dfunc.CloseSocket (testSocket);
	testInProgress = false;	
	return;
}

// joe: rcon from ProQuake
extern	cvar_t	rcon_password;
extern	cvar_t	rcon_server;
extern	char	server_name[MAX_QPATH];

static void Rcon_f (void)
{
	char	*host;
	int	n;
	struct qsockaddr sendaddr;

	if (testInProgress)
	{
		Con_Printf ("There is already a test/rcon in progress\n");
		return;
	}

	if (Cmd_Argc() < 2)
	{
		Con_Printf ("Usage: rcon <host>\n");
		return;
	}

	if (!*rcon_password.string)
	{
		Con_Printf ("rcon_password has not been set\n");
		return;
	}

	host = rcon_server.string;

	if (!*rcon_server.string)
	{
		if (cls.state == ca_connected)
		{
			host = server_name;
		}
		else
		{
			Con_Printf ("rcon_server has not been set\n");
			return;
		}
	}

	Strip_Port (host);

	if (host && hostCacheCount)
	{
		for (n=0 ; n<hostCacheCount ; n++)
			if (!Q_strcasecmp(host, hostcache[n].name))
			{
				if (hostcache[n].driver != myDriverLevel)
					continue;
				net_landriverlevel = hostcache[n].ldriver;
				memcpy (&sendaddr, &hostcache[n].addr, sizeof(struct qsockaddr));
				break;
			}
		if (n < hostCacheCount)
			goto JustDoIt;
	}

	for (net_landriverlevel = 0 ; net_landriverlevel < net_numlandrivers ; net_landriverlevel++)
	{
		if (!net_landrivers[net_landriverlevel].initialized)
			continue;

		// see if we can resolve the host name
		if (dfunc.GetAddrFromName(host, &sendaddr) != -1)
			break;
	}
	if (net_landriverlevel == net_numlandrivers)
	{
		Con_Printf ("Could not resolve %s\n", host);
		return;
	}

JustDoIt:
	testSocket = dfunc.OpenSocket(0);
	if (testSocket == -1)			
	{
		Con_Printf ("Could not open socket\n");
		return;
	}

	testInProgress = true;
	testPollCount = 20;
	testDriver = net_landriverlevel;

	SZ_Clear (&net_message);
	// save space for the header, filled in later
	MSG_WriteLong (&net_message, 0);
	MSG_WriteByte (&net_message, CCREQ_RCON);
	MSG_WriteString (&net_message, rcon_password.string);
	MSG_WriteString (&net_message, Cmd_Args());
	*((int *)net_message.data) = BigLong (NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
	dfunc.Write(testSocket, net_message.data, net_message.cursize, &sendaddr);
	SZ_Clear (&net_message);
	SchedulePollProcedure (&rconPollProcedure, 0.05);
}

int Datagram_Init (void)
{
	int	i, csock;

	myDriverLevel = net_driverlevel;
	Cmd_AddCommand ("net_stats", NET_Stats_f);

	if (COM_CheckParm("-nolan"))
		return -1;

	for (i=0 ; i<net_numlandrivers ; i++)
	{
		csock = net_landrivers[i].Init();
		if (csock == -1)
			continue;
		net_landrivers[i].initialized = true;
		net_landrivers[i].controlSock = csock;
	}

#ifdef BAN_TEST
	Cmd_AddCommand ("ban", NET_Ban_f);
#endif
	Cmd_AddCommand ("test", Test_f);
	Cmd_AddCommand ("test2", Test2_f);
	Cmd_AddCommand ("rcon", Rcon_f);	// joe: rcon from ProQuake

	return 0;
}

void Datagram_Shutdown (void)
{
	int	i;

// shutdown the lan drivers
	for (i=0 ; i<net_numlandrivers ; i++)
	{
		if (net_landrivers[i].initialized)
		{
			net_landrivers[i].Shutdown ();
			net_landrivers[i].initialized = false;
		}
	}
}

void Datagram_Close (qsocket_t *sock)
{
	sfunc.CloseSocket(sock->socket);
}

void Datagram_Listen (qboolean state)
{
	int	i;

	for (i=0 ; i<net_numlandrivers ; i++)
		if (net_landrivers[i].initialized)
			net_landrivers[i].Listen (state);
}

qsocket_t *Datagram_Reject (char *message, int acceptsock, struct qsockaddr *addr)
{
	SZ_Clear (&net_message);
	// save space for the header, filled in later
	MSG_WriteLong (&net_message, 0);
	MSG_WriteByte (&net_message, CCREP_REJECT);
	MSG_WriteString (&net_message, message);
	*((int *)net_message.data) = BigLong (NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
	dfunc.Write (acceptsock, net_message.data, net_message.cursize, addr);
	SZ_Clear (&net_message);

	return NULL;
}

extern	cvar_t		cl_password;	// joe: password protection from ProQuake
extern	unsigned long	qsmackAddr;	// joe: allow qsmack bots to connect to server

static qsocket_t *_Datagram_CheckNewConnections (void)
{
	int		newsock, acceptsock, len, command, control, ret;
	byte		mod, mod_version, mod_flags;
	qsocket_t	*sock, *s;
	struct qsockaddr clientaddr;
	struct qsockaddr newaddr;

	acceptsock = dfunc.CheckNewConnections();
	if (acceptsock == -1)
		return NULL;

	SZ_Clear (&net_message);

	len = dfunc.Read (acceptsock, net_message.data, net_message.maxsize, &clientaddr);
	if (len < sizeof(int))
		return NULL;
	net_message.cursize = len;

	MSG_BeginReading ();
	control = BigLong(*((int *)net_message.data));
	MSG_ReadLong ();
	if (control == -1)
		return NULL;
	if ((control & (~NETFLAG_LENGTH_MASK)) != NETFLAG_CTL)
		return NULL;
	if ((control & NETFLAG_LENGTH_MASK) != len)
		return NULL;

	command = MSG_ReadByte ();
	if (command == CCREQ_SERVER_INFO)
	{
		if (strcmp(MSG_ReadString(), "QUAKE"))
			return NULL;

		SZ_Clear (&net_message);
		// save space for the header, filled in later
		MSG_WriteLong (&net_message, 0);
		MSG_WriteByte (&net_message, CCREP_SERVER_INFO);
		dfunc.GetSocketAddr(acceptsock, &newaddr);
		MSG_WriteString (&net_message, dfunc.AddrToString(&newaddr));
		MSG_WriteString (&net_message, hostname.string);
		MSG_WriteString (&net_message, sv.name);
		MSG_WriteByte (&net_message, net_activeconnections);
		MSG_WriteByte (&net_message, svs.maxclients);
		MSG_WriteByte (&net_message, NET_PROTOCOL_VERSION);
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		dfunc.Write (acceptsock, net_message.data, net_message.cursize, &clientaddr);
		SZ_Clear (&net_message);

		return NULL;
	}

	if (command == CCREQ_PLAYER_INFO)
	{
		int		playerNumber, activeNumber, clientNumber;
		client_t	*client;
		
		playerNumber = MSG_ReadByte ();
		activeNumber = -1;
		for (clientNumber = 0, client = svs.clients ; clientNumber < svs.maxclients ; clientNumber++, client++)
		{
			if (client->active)
			{
				activeNumber++;
				if (activeNumber == playerNumber)
					break;
			}
		}
		if (clientNumber == svs.maxclients)
			return NULL;

		SZ_Clear (&net_message);
		// save space for the header, filled in later
		MSG_WriteLong (&net_message, 0);
		MSG_WriteByte (&net_message, CCREP_PLAYER_INFO);
		MSG_WriteByte (&net_message, playerNumber);
		MSG_WriteString (&net_message, client->name);
		MSG_WriteLong (&net_message, client->colors);
		MSG_WriteLong (&net_message, (int)client->edict->v.frags);
		MSG_WriteLong (&net_message, (int)(net_time - client->netconnection->connecttime));
		MSG_WriteString (&net_message, client->netconnection->address);
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		dfunc.Write (acceptsock, net_message.data, net_message.cursize, &clientaddr);
		SZ_Clear (&net_message);

		return NULL;
	}

	if (command == CCREQ_RULE_INFO)
	{
		char	*prevCvarName;
		cvar_t	*var;

		// find the search start location
		prevCvarName = MSG_ReadString ();
		if (*prevCvarName)
		{
			if (!(var = Cvar_FindVar(prevCvarName)))
				return NULL;
			var = var->next;
		}
		else
			var = cvar_vars;

		// search for the next server cvar
		while (var)
		{
			if (var->flags & CVAR_SERVER)
				break;
			var = var->next;
		}

		// send the response

		SZ_Clear (&net_message);
		// save space for the header, filled in later
		MSG_WriteLong (&net_message, 0);
		MSG_WriteByte (&net_message, CCREP_RULE_INFO);
		if (var)
		{
			MSG_WriteString (&net_message, var->name);
			MSG_WriteString (&net_message, var->string);
		}
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		dfunc.Write (acceptsock, net_message.data, net_message.cursize, &clientaddr);
		SZ_Clear (&net_message);

		return NULL;
	}

	// joe: rcon from ProQuake
	if (command == CCREQ_RCON)
	{
		// 2048 = largest possible return from MSG_ReadString
		char	pass[2048], cmd[2048];

		strcpy (pass, MSG_ReadString());
		strcpy (cmd, MSG_ReadString());

		SZ_Clear (&rcon_message);
		// save space for the header, filled in later
		MSG_WriteLong (&rcon_message, 0);
		MSG_WriteByte (&rcon_message, CCREP_RCON);

		if (!*rcon_password.string)
		{
			MSG_WriteString (&rcon_message, "rcon is disabled on this server");
		}
		else if (strcmp(pass, rcon_password.string))
		{
			MSG_WriteString (&rcon_message, "incorrect password");
		}
		else
		{
			MSG_WriteString (&rcon_message, "");
			rcon_active = true;
			Cmd_ExecuteString (cmd, src_command);
			rcon_active = false;
		}

		*((int *)rcon_message.data) = BigLong (NETFLAG_CTL | (rcon_message.cursize & NETFLAG_LENGTH_MASK));
		dfunc.Write(acceptsock, rcon_message.data, rcon_message.cursize, &clientaddr);
		SZ_Clear (&rcon_message);

		return NULL;
	}

	if (command != CCREQ_CONNECT)
		return NULL;

	if (strcmp(MSG_ReadString(), "QUAKE"))
		return NULL;

	if (MSG_ReadByte() != NET_PROTOCOL_VERSION)
		return Datagram_Reject ("Incompatible version.\n", acceptsock, &clientaddr);

#ifdef BAN_TEST
	// check for a ban
	if (clientaddr.sa_family == AF_INET)
	{
		unsigned long	testAddr;

		testAddr = ((struct sockaddr_in *)&clientaddr)->sin_addr.s_addr;
		if ((testAddr & banMask) == banAddr)
			return Datagram_Reject ("You have been banned.\n", acceptsock, &clientaddr);
	}
#endif

	// see if this guy is already connected
	for (s = net_activeSockets ; s ; s = s->next)
	{
		if (s->driver != net_driverlevel)
			continue;
		ret = dfunc.AddrCompare(&clientaddr, &s->addr);
		if (ret >= 0)
		{
			// is this a duplicate connection request?
			if (ret == 0 && net_time - s->connecttime < 2.0)
			{
				// yes, so send a duplicate reply
				SZ_Clear (&net_message);
				// save space for the header, filled in later
				MSG_WriteLong (&net_message, 0);
				MSG_WriteByte (&net_message, CCREP_ACCEPT);
				dfunc.GetSocketAddr(s->socket, &newaddr);
				MSG_WriteLong (&net_message, dfunc.GetSocketPort(&newaddr));
				*((int *)net_message.data) = BigLong (NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
				dfunc.Write (acceptsock, net_message.data, net_message.cursize, &clientaddr);
				SZ_Clear (&net_message);

				return NULL;
			}
			// it's somebody coming back in from a crash/disconnect
			// so close the old qsocket and let their retry get them back in
			//NET_Close (s);	// joe: removed as in ProQuake
			//return NULL;
		}
	}

	if (len > 12)
		mod = MSG_ReadByte ();
	else if (!qsmackActive && clientaddr.sa_family == AF_INET && ((struct sockaddr_in *)&clientaddr)->sin_addr.s_addr == qsmackAddr)
		mod = MOD_QSMACK;
	else
		mod = MOD_NONE;
	if (len > 13)
		mod_version = MSG_ReadByte ();
	else
		mod_version = 0;
	if (len > 14)
		mod_flags = MSG_ReadByte ();
	else
		mod_flags = 0;
	if (mod != MOD_QSMACK)
	{
		if (cl_password.value && (len <= 18 || cl_password.value != MSG_ReadLong()))
			return Datagram_Reject (va("Óåòöåò éó ðáóó÷ïòä ðòïôåãôåäŸ\nYou must use JoeQuake v%s (build %i) or above\nand set cl_password to the server password\n", JOEQUAKE_VERSION, build_number()), acceptsock, &clientaddr);

		if (cl_cheatfree && (mod != MOD_JOEQUAKE || mod_version < 32))
			return Datagram_Reject (va("Ôèéó éó á ãèåáô­æòåå óåòöåòŸ\nYou must use JoeQuake v%s (build %i) or above\n", JOEQUAKE_VERSION, build_number()), acceptsock, &clientaddr);
	}

	// allocate a QSocket
	if (!(sock = NET_NewQSocket()))
		return Datagram_Reject ("Server is full.\n", acceptsock, &clientaddr);

	// allocate a network socket
	newsock = dfunc.OpenSocket(0);
	if (newsock == -1)
	{
		NET_FreeQSocket (sock);
		return NULL;
	}

	// connect to the client
	if (dfunc.Connect(newsock, &clientaddr) == -1)
	{
		dfunc.CloseSocket(newsock);
		NET_FreeQSocket (sock);
		return NULL;
	}

	sock->mod = mod;
	sock->mod_version = mod_version;
	sock->mod_flags = mod_flags;
	if (mod == MOD_JOEQUAKE && mod_version >= 34)
		sock->net_wait = true;		// joe: NAT fix from ProQuake
	sock->encrypt = 2;

	// everything is allocated, just fill in the details	
	sock->socket = newsock;
	sock->landriver = net_landriverlevel;
	sock->addr = clientaddr;
	Q_strcpy (sock->address, dfunc.AddrToString(&clientaddr));

	// send him back the info about the server connection he has been allocated
	SZ_Clear (&net_message);
	// save space for the header, filled in later
	MSG_WriteLong (&net_message, 0);
	MSG_WriteByte (&net_message, CCREP_ACCEPT);
	dfunc.GetSocketAddr (newsock, &newaddr);
	sock->client_port = dfunc.GetSocketPort(&newaddr);
	MSG_WriteLong (&net_message, sock->client_port);
	MSG_WriteByte (&net_message, MOD_JOEQUAKE);
	MSG_WriteByte (&net_message, 10 * MOD_PROQUAKE_VERSION);
	if (cl_cheatfree)
	{
		MSG_WriteByte (&net_message, JQF_CHEATFREE);
		MSG_WriteLong (&net_message, _lrotl(net_seed, 17));
	}
	else
	{
		MSG_WriteByte (&net_message, 0);
	}
	*((int *)net_message.data) = BigLong (NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
	dfunc.Write (acceptsock, net_message.data, net_message.cursize, &clientaddr);
	SZ_Clear (&net_message);

	return sock;
}

qsocket_t *Datagram_CheckNewConnections (void)
{
	qsocket_t	*ret = NULL;

	for (net_landriverlevel = 0 ; net_landriverlevel < net_numlandrivers ; net_landriverlevel++)
		if (net_landrivers[net_landriverlevel].initialized)
			if ((ret = _Datagram_CheckNewConnections ()))
				break;

	return ret;
}

static void _Datagram_SearchForHosts (qboolean xmit)
{
	int	i, n, ret, control;
	struct qsockaddr readaddr;
	struct qsockaddr myaddr;

	dfunc.GetSocketAddr (dfunc.controlSock, &myaddr);
	if (xmit)
	{
		SZ_Clear (&net_message);
		// save space for the header, filled in later
		MSG_WriteLong (&net_message, 0);
		MSG_WriteByte (&net_message, CCREQ_SERVER_INFO);
		MSG_WriteString (&net_message, "QUAKE");
		MSG_WriteByte (&net_message, NET_PROTOCOL_VERSION);
		*((int *)net_message.data) = BigLong (NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		dfunc.Broadcast(dfunc.controlSock, net_message.data, net_message.cursize);
		SZ_Clear (&net_message);
	}

	while ((ret = dfunc.Read(dfunc.controlSock, net_message.data, net_message.maxsize, &readaddr)) > 0)
	{
		if (ret < sizeof(int))
			continue;
		net_message.cursize = ret;

		// don't answer our own query
		if (dfunc.AddrCompare(&readaddr, &myaddr) >= 0)
			continue;

		// is the cache full?
		if (hostCacheCount == HOSTCACHESIZE)
			continue;

		MSG_BeginReading ();
		control = BigLong (*((int *)net_message.data));
		MSG_ReadLong ();
		if (control == -1)
			continue;
		if ((control & (~NETFLAG_LENGTH_MASK)) !=  NETFLAG_CTL)
			continue;
		if ((control & NETFLAG_LENGTH_MASK) != ret)
			continue;

		if (MSG_ReadByte() != CCREP_SERVER_INFO)
			continue;

		dfunc.GetAddrFromName(MSG_ReadString(), &readaddr);

		// search the cache for this server
		for (n=0 ; n<hostCacheCount ; n++)
			if (dfunc.AddrCompare(&readaddr, &hostcache[n].addr) == 0)
				break;

		// is it already there?
		if (n < hostCacheCount)
			continue;

		// add it
		hostCacheCount++;
		Q_strcpy (hostcache[n].name, MSG_ReadString());
		Q_strcpy (hostcache[n].map, MSG_ReadString());
		hostcache[n].users = MSG_ReadByte ();
		hostcache[n].maxusers = MSG_ReadByte ();
		if (MSG_ReadByte() != NET_PROTOCOL_VERSION)
		{
			Q_strcpy (hostcache[n].cname, hostcache[n].name);
			hostcache[n].cname[14] = 0;
			Q_strcpy (hostcache[n].name, "*");
			strcat (hostcache[n].name, hostcache[n].cname);
		}
		memcpy (&hostcache[n].addr, &readaddr, sizeof(struct qsockaddr));
		hostcache[n].driver = net_driverlevel;
		hostcache[n].ldriver = net_landriverlevel;
		Q_strcpy (hostcache[n].cname, dfunc.AddrToString(&readaddr));

		// check for a name conflict
		for (i=0 ; i<hostCacheCount ; i++)
		{
			if (i == n)
				continue;
			if (!Q_strcasecmp(hostcache[n].name, hostcache[i].name))
			{
				i = strlen (hostcache[n].name);
				if (i < 15 && hostcache[n].name[i-1] > '8')
				{
					hostcache[n].name[i] = '0';
					hostcache[n].name[i+1] = 0;
				}
				else
				{
					hostcache[n].name[i-1]++;
				}
				i = -1;
			}
		}
	}
}

void Datagram_SearchForHosts (qboolean xmit)
{
	for (net_landriverlevel = 0 ; net_landriverlevel < net_numlandrivers ; net_landriverlevel++)
	{
		if (hostCacheCount == HOSTCACHESIZE)
			break;
		if (net_landrivers[net_landriverlevel].initialized)
			_Datagram_SearchForHosts (xmit);
	}
}

static qsocket_t *_Datagram_Connect (char *host)
{
	struct qsockaddr sendaddr;
	struct qsockaddr readaddr;
	qsocket_t	*sock;
	int		newsock, clientsock, ret, len, reps;
	double		start_time;
	int		control;
	char		*reason;

	// see if we can resolve the host name
	if (dfunc.GetAddrFromName(host, &sendaddr) == -1)
	{	// joe: added error message from ProQuake
		Con_Printf ("Could not resolve %s\n", host);
		return NULL;
	}

	newsock = dfunc.OpenSocket(0);
	if (newsock == -1)
		return NULL;

	if (!(sock = NET_NewQSocket()))
		goto ErrorReturn2;

	sock->socket = newsock;
	sock->landriver = net_landriverlevel;

	// connect to the host
	if (dfunc.Connect(newsock, &sendaddr) == -1)
		goto ErrorReturn;

	// send the connection request
	Con_Printf ("trying...\n");
	SCR_UpdateScreen ();
	start_time = net_time;

	for (reps=0 ; reps<3 ; reps++)
	{
		SZ_Clear (&net_message);
		// save space for the header, filled in later
		MSG_WriteLong (&net_message, 0);
		MSG_WriteByte (&net_message, CCREQ_CONNECT);
		MSG_WriteString (&net_message, "QUAKE");
		MSG_WriteByte (&net_message, NET_PROTOCOL_VERSION);
		MSG_WriteByte (&net_message, MOD_JOEQUAKE);
		MSG_WriteByte (&net_message, MOD_PROQUAKE_VERSION * 10);
		MSG_WriteByte (&net_message, 0);
		MSG_WriteLong (&net_message, cl_password.value);	// joe: password protected servers from ProQuake
		*((int *)net_message.data) = BigLong (NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		dfunc.Write (newsock, net_message.data, net_message.cursize, &sendaddr);
		SZ_Clear (&net_message);

		do {
			ret = dfunc.Read (newsock, net_message.data, net_message.maxsize, &readaddr);
			// if we got something, validate it
			if (ret > 0)
			{
				// is it from the right place?
				if (sfunc.AddrCompare(&readaddr, &sendaddr) != 0)
				{
#if 0	// joe: removed DEBUG
					Con_Printf ("wrong reply address\n");
					Con_Printf ("Expected: %s\n", StrAddr(&sendaddr));
					Con_Printf ("Received: %s\n", StrAddr(&readaddr));
					SCR_UpdateScreen ();
#endif
					ret = 0;
					continue;
				}

				if (ret < sizeof(int))
				{
					ret = 0;
					continue;
				}

				net_message.cursize = ret;
				MSG_BeginReading ();

				control = BigLong(*((int *)net_message.data));
				MSG_ReadLong ();
				if (control == -1)
				{
					ret = 0;
					continue;
				}
				if ((control & (~NETFLAG_LENGTH_MASK)) !=  NETFLAG_CTL)
				{
					ret = 0;
					continue;
				}
				if ((control & NETFLAG_LENGTH_MASK) != ret)
				{
					ret = 0;
					continue;
				}
			}
		} while (ret == 0 && (SetNetTime() - start_time) < 2.5);

		if (ret)
			break;
		Con_Printf ("still trying...\n");
		SCR_UpdateScreen ();
		start_time = SetNetTime ();
	}

	if (ret == 0)
	{
		reason = "No Response";
		Con_Printf ("%s\n", reason);
		Q_strcpy (m_return_reason, reason);

		goto ErrorReturn;
	}

	if (ret == -1)
	{
		reason = "Network Error";
		Con_Printf ("%s\n", reason);
		Q_strcpy (m_return_reason, reason);

		goto ErrorReturn;
	}

	len = ret;
	ret = MSG_ReadByte ();
	if (ret == CCREP_REJECT)
	{
		reason = MSG_ReadString ();
		Con_Printf (reason);
		Q_strncpy (m_return_reason, reason, 31);

		goto ErrorReturn;
	}

	if (ret == CCREP_ACCEPT)
	{
		sock->client_port = MSG_ReadLong ();
		memcpy (&sock->addr, &sendaddr, sizeof(struct qsockaddr));
		dfunc.SetSocketPort (&sock->addr, sock->client_port);

		if (len > 9)
			sock->mod = MSG_ReadByte ();
		else
			sock->mod = MOD_NONE;
		if (len > 10)
			sock->mod_version = MSG_ReadByte ();
		else
			sock->mod_version = 0;
		if (len > 11)
			sock->mod_flags = MSG_ReadByte ();
		else
			sock->mod_flags = 0;
		if (sock->mod == MOD_JOEQUAKE && (sock->mod_flags & JQF_CHEATFREE))
		{
			if (security_loaded)
			{
				cl_cheatfree = true;
				net_seed = MSG_ReadLong ();
				Security_SetSeed (net_seed, argv0);
			}
			else
			{
				reason = "Security module not initialized";
				Con_Printf ("%s\n", reason);
				Q_strcpy (m_return_reason, reason);

				goto ErrorReturn;
			}
		}
		else
		{
			cl_cheatfree = false;
		}
	}
	else
	{
		reason = "Bad Response";
		Con_Printf ("%s\n", reason);
		Q_strcpy (m_return_reason, reason);

		goto ErrorReturn;
	}

	dfunc.GetNameFromAddr(&sendaddr, sock->address);

	Con_Printf ("Connection accepted\n");
	sock->lastMessageTime = SetNetTime ();

	// joe, from ProQuake: make NAT work by opening a new socket
	if (sock->mod == MOD_JOEQUAKE && sock->mod_version >= 34)
	{
		clientsock = dfunc.OpenSocket(0);
		if (clientsock == -1)
			goto ErrorReturn;
		dfunc.CloseSocket(newsock);
		newsock = clientsock;
		sock->socket = newsock;
	}

	sock->encrypt = 2;

	// switch the connection to the specified address
	if (dfunc.Connect(newsock, &sock->addr) == -1)
	{
		reason = "Connect to Game failed";
		Con_Printf ("%s\n", reason);
		Q_strcpy (m_return_reason, reason);

		goto ErrorReturn;
	}

	m_return_onerror = false;
	return sock;

ErrorReturn:
	NET_FreeQSocket (sock);

ErrorReturn2:
	dfunc.CloseSocket(newsock);
	if (m_return_onerror)
	{
		key_dest = key_menu;
		m_state = m_return_state;
		m_return_onerror = false;
	}

	return NULL;
}

qsocket_t *Datagram_Connect (char *host)
{
	qsocket_t *ret = NULL;

	Strip_Port (host);
	for (net_landriverlevel = 0 ; net_landriverlevel < net_numlandrivers ; net_landriverlevel++)
		if (net_landrivers[net_landriverlevel].initialized)
			if ((ret = _Datagram_Connect(host)))
				break;

	return ret;
}
