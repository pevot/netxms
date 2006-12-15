/* $Id: nxmp_parser.h,v 1.1 2006-12-15 11:38:14 victor Exp $ */
/* 
** NetXMS - Network Management System
** NetXMS Server
** Copyright (C) 2003, 2004, 2005, 2006 Victor Kirhenshtein
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
** File: nxmp_parser.h
**
**/

#ifndef _nxmp_parser_h
#define _nxmp_parser_h

#include <FlexLexer.h>
#include "nxmp_parser.tab.hpp"


//
// Constants
//

#define MAX_STRING_SIZE    4096


//
// Management pack's data
//

class NXMP_Data
{
private:
   EVENT_TEMPLATE *m_pEventList;
   EVENT_TEMPLATE *m_pCurrEvent;
   DWORD m_dwNumEvents;

public:
   NXMP_Data();
   ~NXMP_Data();

   void NewEvent(char *pszName);
   void SetEventText(char *pszText);
   void SetEventDescription(char *pszText);
   void SetEventCode(DWORD dwCode) { if (m_pCurrEvent != NULL) m_pCurrEvent->dwCode = dwCode; }
   void SetEventSeverity(DWORD dwSeverity) { if (m_pCurrEvent != NULL) m_pCurrEvent->dwSeverity = dwSeverity; }
   void SetEventFlags(DWORD dwFlags) { if (m_pCurrEvent != NULL) m_pCurrEvent->dwFlags = dwFlags; }
   void CloseEvent(void) { m_pCurrEvent = NULL; }
};


//
// Modified lexer class
//

class NXMP_Parser;

class NXMP_Lexer : public yyFlexLexer
{
protected:
   int m_nSourceSize;
   int m_nSourcePos;
   char *m_pszSource;
   NXMP_Parser *m_pParser;

   int m_nCurrLine;
   int m_nCommentLevel;
   int m_nStrSize;
   char m_szStr[MAX_STRING_SIZE];
   YYSTYPE *m_plval;
   BOOL m_bErrorState;

	virtual int LexerInput(char *pBuffer, int nMaxSize);
   virtual void LexerError(const char *pszMsg);

public:
	NXMP_Lexer(NXMP_Parser *pParser, TCHAR *pszSource);
	virtual ~NXMP_Lexer();

   virtual int yylex(void);
   void SetLvalPtr(YYSTYPE *plval) { m_plval = plval; }

   int GetCurrLine(void) { return m_nCurrLine; }

   void SetErrorState(void) { m_bErrorState = TRUE; }
   BOOL IsErrorState(void) { return m_bErrorState; }
};


//
// Parser class
//

class NXMP_Parser
{
protected:
   TCHAR *m_pszErrorText;
   NXMP_Lexer *m_pLexer;

public:
   NXMP_Parser(void);
   ~NXMP_Parser();

   NXMP_Data *Parse(TCHAR *pszSource);
   void Error(const char *pszMsg);

   TCHAR *GetErrorText(void) { return CHECK_NULL(m_pszErrorText); }
};

#endif
