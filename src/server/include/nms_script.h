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
** File: nms_script.h
**
**/

#ifndef _nms_script_h_
#define _nms_script_h_


//
// "NetXMS object" class
//

class NXSL_NetObjClass : public NXSL_Class
{
public:
   NXSL_NetObjClass();

   virtual NXSL_Value *getAttr(NXSL_Object *pObject, const TCHAR *pszAttr);
};


//
// "NetXMS node" class
//

class NXSL_NodeClass : public NXSL_Class
{
public:
   NXSL_NodeClass();

   virtual NXSL_Value *getAttr(NXSL_Object *pObject, const TCHAR *pszAttr);
};


//
// "NetXMS interface" class
//

class NXSL_InterfaceClass : public NXSL_Class
{
public:
   NXSL_InterfaceClass();

   virtual NXSL_Value *getAttr(NXSL_Object *pObject, const TCHAR *pszAttr);
};

/**
 * NXSL "Event" class
 */
class NXSL_EventClass : public NXSL_Class
{
public:
   NXSL_EventClass();

   virtual NXSL_Value *getAttr(NXSL_Object *pObject, const TCHAR *pszAttr);
};

/**
 * NXSL "DCI" class
 */
class NXSL_DciClass : public NXSL_Class
{
public:
   NXSL_DciClass();

   virtual NXSL_Value *getAttr(NXSL_Object *object, const TCHAR *attr);
};

/**
 * NXSL "SNMP_Transport" class
 */
class NXSL_SNMPTransportClass : public NXSL_Class
{
public:
	NXSL_SNMPTransportClass();

	virtual NXSL_Value *getAttr(NXSL_Object *object, const TCHAR *attr);
	virtual void onObjectDelete(NXSL_Object *object);
};


//
// "SNMP_VarBind" class
//

class NXSL_SNMPVarBindClass : public NXSL_Class
{
public:
	NXSL_SNMPVarBindClass();

	virtual NXSL_Value *getAttr(NXSL_Object *object, const TCHAR *attr);
	virtual void onObjectDelete(NXSL_Object *object);
};


//
// Server's default script environment
//

class NXSL_ServerEnv : public NXSL_Environment
{
protected:
	CONSOLE_CTX m_console;

public:
	NXSL_ServerEnv();

	virtual void print(NXSL_Value *value);
	virtual void trace(int level, const TCHAR *text);

	void setConsole(CONSOLE_CTX console) { m_console = console; }
};


//
// Functions
//

void LoadScripts();
void ReloadScript(UINT32 dwScriptId);
BOOL IsValidScriptId(UINT32 dwId);


//
// Global variables
//

extern NXSL_Library *g_pScriptLibrary;
extern NXSL_NetObjClass g_nxslNetObjClass;
extern NXSL_NodeClass g_nxslNodeClass;
extern NXSL_InterfaceClass g_nxslInterfaceClass;
extern NXSL_EventClass g_nxslEventClass;
extern NXSL_DciClass g_nxslDciClass;
extern NXSL_SNMPVarBindClass g_nxslSnmpVarBindClass;
extern NXSL_SNMPTransportClass g_nxslSnmpTransportClass;

#endif
