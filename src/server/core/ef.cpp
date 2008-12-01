/* 
** NetXMS - Network Management System
** Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Victor Kirhenshtein
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
** File: ef.cpp
**
**/

#include "nxcore.h"


//
// Setup event forwarding session
//

BOOL EF_SetupSession(ISCSession *, CSCPMessage *request)
{
	return TRUE;
}


//
// Close event forwarding session
//

void EF_CloseSession(ISCSession *)
{
}


//
// Process event forwarding session message
//

BOOL EF_ProcessMessage(ISCSession *session, CSCPMessage *request, CSCPMessage *response)
{
	int i, numArgs;
	DWORD code, id;
	TCHAR userTag[MAX_USERTAG_LENGTH], *argList[32];
   char format[] = "ssssssssssssssssssssssssssssssss";
	NetObj *object;

	if (request->GetCode() == CMD_FORWARD_EVENT)
	{
		DbgPrintf(4, _T("Event forwarding request from %s"), IpToStr(session->GetPeerAddress(), userTag));
		
		id = request->GetVariableLong(VID_OBJECT_ID);
		if (id != 0)
			object = FindObjectById(id);  // Object is specified explicitely
		else
			object = FindNodeByIP(request->GetVariableLong(VID_IP_ADDRESS));	// Object is specified by IP address
		
		if (object != NULL)
		{
			code = request->GetVariableLong(VID_EVENT_CODE);
			request->GetVariableStr(VID_USER_TAG, userTag, MAX_USERTAG_LENGTH);
			numArgs = request->GetVariableShort(VID_NUM_ARGS);
			if (numArgs > 32)
				numArgs = 32;
			for(i = 0; i < numArgs; i++)
				argList[i] = request->GetVariableStr(VID_EVENT_ARG_BASE + i);

			format[numArgs] = 0;
			if (PostEventWithTag(code, object->Id(), userTag, (numArgs > 0) ? format : NULL,
			                     argList[0], argList[1], argList[2], argList[3],
										argList[4], argList[5], argList[6], argList[7],
										argList[8], argList[9], argList[10], argList[11],
										argList[12], argList[13], argList[14], argList[15],
										argList[16], argList[17], argList[18], argList[19],
										argList[20], argList[21], argList[22], argList[23],
										argList[24], argList[25], argList[26], argList[27],
										argList[28], argList[29], argList[30], argList[31]))
			{
				response->SetVariable(VID_RCC, ISC_ERR_SUCCESS);
			}
			else
			{
				response->SetVariable(VID_RCC, ISC_ERR_OBJECT_NOT_FOUND);
			}
      
			// Cleanup
			for(i = 0; i < numArgs; i++)
				safe_free(argList[i]);
		}
		else
		{
			response->SetVariable(VID_RCC, ISC_ERR_OBJECT_NOT_FOUND);
		}
	}
	else
	{
		response->SetVariable(VID_RCC, ISC_ERR_NOT_IMPLEMENTED);
	}
	return FALSE;	// Don't close session
}
