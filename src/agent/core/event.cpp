/* 
** NetXMS multiplatform core agent
** Copyright (C) 2003-2019 Victor Kirhenshtein
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
** File: event.cpp
**
**/

#include "nxagentd.h"
#include <stdarg.h>

/**
 * Static data
 */
static UINT64 s_generatedEventsCount = 0;
static UINT64 s_sentEventsCount = 0;
static UINT64 s_eventIdBase = static_cast<UINT64>(time(NULL)) << 32;
static VolatileCounter s_eventIdCounter = 0;
static time_t s_lastEventTimestamp = 0;

/**
 * Send event to server
 */
void PostEvent(UINT32 eventCode, const TCHAR *eventName, time_t timestamp, int pcount, const TCHAR **parameters)
{
   if (nxlog_get_debug_level() >= 5)
   {
      StringBuffer argsText;
      for(int i = 0; i < pcount; i++)
      {
         argsText.append(_T(", arg["));
         argsText.append(i);
         argsText.append(_T("]=\""));
         argsText.append(CHECK_NULL(parameters[i]));
         argsText.append(_T('"'));
      }
      nxlog_debug(5, _T("PostEvent(): event_code=%d, event_name=%s, timestamp=") INT64_FMT _T(", num_args=%d%s"),
                  eventCode, CHECK_NULL(eventName), static_cast<INT64>(timestamp), pcount, (const TCHAR *)argsText);
   }

   NXCPMessage *msg = new NXCPMessage(CMD_TRAP, 0, 4); // Use version 4
	msg->setField(VID_TRAP_ID, s_eventIdBase | static_cast<UINT64>(InterlockedIncrement(&s_eventIdCounter)));
   msg->setField(VID_EVENT_CODE, eventCode);
	if (eventName != NULL)
		msg->setField(VID_EVENT_NAME, eventName);
	msg->setFieldFromTime(VID_TIMESTAMP, (timestamp != 0) ? timestamp : time(NULL));
   msg->setField(VID_NUM_ARGS, (WORD)pcount);
   for(int i = 0; i < pcount; i++)
      msg->setField(VID_EVENT_ARG_BASE + i, parameters[i]);
	s_generatedEventsCount++;
	s_lastEventTimestamp = time(NULL);
	g_notificationProcessorQueue.put(msg);
}

/**
 * Send event - variant 2
 * Arguments:
 * eventCode - Event code
 * eventName   - event name; to send event by name, eventCode must be set to 0
 * format   - Parameter format string, each parameter represented by one character.
 *    The following format characters can be used:
 *        s - String
 *        m - Multibyte (non-UNICODE) string
 *        d - Decimal integer
 *        x - Hex integer
 *        a - IP address
 *        i - Object ID
 *        D - 64-bit decimal integer
 *        X - 64-bit hex integer
 */
void PostEvent(UINT32 eventCode, const TCHAR *eventName, time_t timestamp, const char *format, va_list args)
{
   int i, iNumArgs;
   TCHAR *ppArgList[64];
   static TCHAR badFormat[] = _T("BAD FORMAT");

   iNumArgs = (format == NULL) ? 0 : (int)strlen(format);
   for(i = 0; i < iNumArgs; i++)
   {
      switch(format[i])
      {
         case 's':
            ppArgList[i] = va_arg(args, TCHAR *);
            break;
         case 'm':
#ifdef UNICODE
            ppArgList[i] = WideStringFromMBString(va_arg(args, char *));
#else
            ppArgList[i] = va_arg(args, char *);
#endif
            break;
         case 'd':
            ppArgList[i] = (TCHAR *)malloc(16 * sizeof(TCHAR));   //
            _sntprintf(ppArgList[i], 16, _T("%d"), va_arg(args, LONG)); //
            break;
         case 'D':
            ppArgList[i] = (TCHAR *)malloc(32 * sizeof(TCHAR));
            _sntprintf(ppArgList[i], 32, INT64_FMT, va_arg(args, INT64));
            break;
         case 'x':
         case 'i':
            ppArgList[i] = (TCHAR *)malloc(16 * sizeof(TCHAR));
            _sntprintf(ppArgList[i], 16, _T("0x%08X"), va_arg(args, UINT32));
            break;
         case 'X':
            ppArgList[i] = (TCHAR *)malloc(32 * sizeof(TCHAR));
            _sntprintf(ppArgList[i], 32, UINT64X_FMT(_T("016")), va_arg(args, QWORD));
            break;
         case 'a':
            ppArgList[i] = (TCHAR *)malloc(16 * sizeof(TCHAR));
            IpToStr(va_arg(args, UINT32), ppArgList[i]);
            break;
         default:
            ppArgList[i] = badFormat;
            break;
      }
   }

   PostEvent(eventCode, eventName, timestamp, iNumArgs, const_cast<const TCHAR**>(ppArgList));

   for(i = 0; i < iNumArgs; i++)
      if ((format[i] == 'd') || (format[i] == 'x') ||
         (format[i] == 'D') || (format[i] == 'X') ||
         (format[i] == 'i') || (format[i] == 'a')
#ifdef UNICODE
         || (format[i] == 'm')
#endif
         )
      {
         free(ppArgList[i]);
      }
}

/**
 * Send event - variant 3
 * Same as variant 2, but uses argument list instead of va_list
 */
void PostEvent(UINT32 eventCode, const TCHAR *eventName, time_t timestamp, const char *format, ...)
{
   va_list args;
   va_start(args, format);
   PostEvent(eventCode, eventName, timestamp, format, args);
   va_end(args);
}

/**
 * Forward event from external subagent to server
 */
void ForwardEvent(NXCPMessage *msg)
{
	msg->setField(VID_TRAP_ID, s_eventIdBase | (UINT64)InterlockedIncrement(&s_eventIdCounter));
	s_generatedEventsCount++;
	s_lastEventTimestamp = time(NULL);
	g_notificationProcessorQueue.put(msg);
}

/**
 * Handler for event statistic DCIs
 */
LONG H_AgentEventSender(const TCHAR *cmd, const TCHAR *arg, TCHAR *value, AbstractCommSession *session)
{
	switch(arg[0])
	{
		case 'G':
			ret_uint64(value, s_generatedEventsCount);
			break;
		case 'S':
			ret_uint64(value, s_sentEventsCount);
			break;
		case 'T':
			ret_uint64(value, static_cast<UINT64>(s_lastEventTimestamp));
			break;
		default:
			return SYSINFO_RC_UNSUPPORTED;
	}
	return SYSINFO_RC_SUCCESS;
}
