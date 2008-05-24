/* $Id$ */

/* 
** NetXMS subagent for GNU/Linux
** Copyright (C) 2008 Alex Kirhenshtein
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

#ifndef __IOSTAT_H__
#define __IOSTAT_H__

LONG H_TransferRate(const char *, const char *, char *);
LONG H_BlockReadRate(const char *, const char *, char *);
LONG H_BlockWriteRate(const char *, const char *, char *);
LONG H_BytesReadRate(const char *, const char *, char *);
LONG H_BytesWriteRate(const char *, const char *, char *);
LONG H_DiskQueue(const char *, const char *, char *);
LONG H_DiskTime(const char *, const char *, char *);

#endif // __DISK_H__

///////////////////////////////////////////////////////////////////////////////
/*

$Log: not supported by cvs2svn $

*/

