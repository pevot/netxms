/* $Id$ */

/* 
** NetXMS subagent for GNU/Linux
** Copyright (C) 2004 Alex Kirhenshtein
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
**/

#ifndef __NET_H__
#define __NET_H__

#define IF_INFO_ADMIN_STATUS     0
#define IF_INFO_OPER_STATUS      1
#define IF_INFO_BYTES_IN         2
#define IF_INFO_BYTES_OUT        3
#define IF_INFO_DESCRIPTION      4
#define IF_INFO_IN_ERRORS        5
#define IF_INFO_OUT_ERRORS       6
#define IF_INFO_PACKETS_IN       7
#define IF_INFO_PACKETS_OUT      8
#define IF_INFO_SPEED            9


LONG H_NetIfInfoFromIOCTL(const char *, const char *, char *);
LONG H_NetIfInfoFromProc(const char *, const char *, char *);
LONG H_NetIpForwarding(const char *, const char *, char *);
LONG H_NetArpCache(const char *, const char *, NETXMS_VALUES_LIST *);
LONG H_NetRoutingTable(const char *, const char *, NETXMS_VALUES_LIST *);
LONG H_NetIfList(const char *, const char *, NETXMS_VALUES_LIST *);

#endif // __NET_H__

///////////////////////////////////////////////////////////////////////////////
/*

$Log: not supported by cvs2svn $
Revision 1.3  2005/06/11 16:28:24  victor
Implemented all Net.Interface.* parameters except Net.Interface.Speed

Revision 1.2  2005/06/09 12:15:43  victor
Added support for Net.Interface.AdminStatus and Net.Interface.Link parameters

Revision 1.1  2004/10/22 22:08:34  alk
source restructured;
implemented:
	Net.IP.Forwarding
	Net.IP6.Forwarding
	Process.Count(*)
	Net.ArpCache
	Net.InterfaceList (if-type not implemented yet)
	System.ProcessList


*/
