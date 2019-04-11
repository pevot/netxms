/*
** NetXMS - Network Management System
** Driver for other Cisco devices
** Copyright (C) 2003-2019 Victor Kirhenshtein
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License as published by
** the Free Software Foundation; either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** File: generic.cpp
**/

#include "cisco.h"

/**
 * Get driver name
 */
const TCHAR *GenericCiscoDriver::getName()
{
   return _T("CISCO-GENERIC");
}

/**
 * Check if given device can be potentially supported by driver
 *
 * @param oid Device OID
 */
int GenericCiscoDriver::isPotentialDevice(const TCHAR *oid)
{
   return (_tcsncmp(oid, _T(".1.3.6.1.4.1.9.1."), 17) == 0) ? 127 : 0;
}

/**
 * Check if given device is supported by driver
 *
 * @param snmp SNMP transport
 * @param oid Device OID
 */
bool GenericCiscoDriver::isDeviceSupported(SNMP_Transport *snmp, const TCHAR *oid)
{
   return SnmpWalkCount(snmp, _T(".1.3.6.1.4.1.9.9.46.1.3.1.1.4")) > 0;
}