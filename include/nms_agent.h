/* 
** Project X - Network Management System
** Copyright (C) 2003 Victor Kirhenshtein
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
** $module: nms_agent.h
**
**/

#ifndef _nms_agent_h_
#define _nms_agent_h_

#include <stdio.h>

//
// Some constants
//

#define AGENT_LISTEN_PORT        4700
#define AGENT_PROTOCOL_VERSION   1
#define MAX_SECRET_LENGTH        64
#define MAX_PARAM_NAME           256
#define MAX_RESULT_LENGTH        256


//
// Error codes
//

#define ERR_SUCCESS                 0
#define ERR_UNKNOWN_COMMAND         400
#define ERR_AUTH_REQUIRED           401
#define ERR_UNKNOWN_PARAMETER       404
#define ERR_REQUEST_TIMEOUT         408
#define ERR_AUTH_FAILED             440
#define ERR_ALREADY_AUTHENTICATED   441
#define ERR_AUTH_NOT_REQUIRED       442
#define ERR_INTERNAL_ERROR          500
#define ERR_NOT_IMPLEMENTED         501
#define ERR_OUT_OF_RESOURCES        503


//
// Parameter handler return codes
//

#define SYSINFO_RC_SUCCESS       0
#define SYSINFO_RC_UNSUPPORTED   1
#define SYSINFO_RC_ERROR         2


//
// Inline functions for returning parameters
//

#ifdef __cplusplus

inline void ret_string(char *rbuf, char *value)
{
   memset(rbuf, 0, MAX_RESULT_LENGTH);
   strncpy(rbuf, value, MAX_RESULT_LENGTH - 3);
   strcat(rbuf, "\r\n");
}

inline void ret_int(char *rbuf, long value)
{
   sprintf(rbuf, "%ld\r\n", value);
}

inline void ret_uint(char *rbuf, unsigned long value)
{
   sprintf(rbuf, "%lu\r\n", value);
}

inline void ret_double(char *rbuf, double value)
{
   sprintf(rbuf, "%f\r\n", value);
}

inline void ret_int64(char *rbuf, __int64 value)
{
#ifdef _WIN32
   sprintf(rbuf, "%I64d\r\n", value);
#else    /* _WIN32 */
   sprintf(rbuf, "%lld\r\n", value);
#endif   /* _WIN32 */
}

inline void ret_uint64(char *rbuf, unsigned __int64 value)
{
#ifdef _WIN32
   sprintf(rbuf, "%I64u\r\n", value);
#else    /* _WIN32 */
   sprintf(rbuf, "%llu\r\n", value);
#endif   /* _WIN32 */
}

#endif   /* __cplusplus */

#endif   /* _nms_agent_h_ */
