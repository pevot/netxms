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
** $module: objects.cpp
**
**/

#include "nms_core.h"


//
// Global data
//

DWORD g_dwFreeItemId = 1;
DWORD g_dwMgmtNode = 0;
INDEX *g_pIndexById = NULL;
DWORD g_dwIdIndexSize = 0;
INDEX *g_pSubnetIndexByAddr = NULL;
DWORD g_dwSubnetAddrIndexSize = 0;
INDEX *g_pNodeIndexByAddr = NULL;
DWORD g_dwNodeAddrIndexSize = 0;
INDEX *g_pInterfaceIndexByAddr = NULL;
DWORD g_dwInterfaceAddrIndexSize = 0;
Network *g_pEntireNet = NULL;

MUTEX g_hMutexObjectAccess;
MUTEX g_hMutexIdIndex;
MUTEX g_hMutexNodeIndex;
MUTEX g_hMutexSubnetIndex;
MUTEX g_hMutexInterfaceIndex;


//
// Static data
//

static DWORD m_dwFreeObjectId = 2;


//
// Initialize objects infrastructure
//

void ObjectsInit(void)
{
   g_hMutexObjectAccess = MutexCreate();
   g_hMutexIdIndex = MutexCreate();
   g_hMutexNodeIndex = MutexCreate();
   g_hMutexSubnetIndex = MutexCreate();
   g_hMutexInterfaceIndex = MutexCreate();

   // Create "Entire Network" object
   g_pEntireNet = new Network;
   NetObjInsert(g_pEntireNet, FALSE);
}


//
// Lock write access to all objects
//

void ObjectsGlobalLock(void)
{
   MutexLock(g_hMutexObjectAccess, INFINITE);
}


//
// Unlock write access to all objects
//

void ObjectsGlobalUnlock(void)
{
   MutexUnlock(g_hMutexObjectAccess);
}


//
// Add new object to index
//

static void AddObjectToIndex(INDEX **ppIndex, DWORD *pdwIndexSize, DWORD dwKey, NetObj *pObject)
{
   DWORD dwFirst, dwLast, dwPos;
   INDEX *pIndex = *ppIndex;

   if (pIndex == NULL)
   {
      dwPos = 0;
   }
   else
   {
      dwFirst = 0;
      dwLast = *pdwIndexSize - 1;

      if (dwKey < pIndex[0].dwKey)
         dwPos = 0;
      else if (dwKey > pIndex[dwLast].dwKey)
         dwPos = dwLast + 1;
      else
         while(dwFirst < dwLast)
         {
            dwPos = (dwFirst + dwLast) / 2;
            if (dwPos == 0)
               dwPos++;
            if ((pIndex[dwPos - 1].dwKey < dwKey) && (pIndex[dwPos].dwKey > dwKey))
               break;
            if ((pIndex[dwPos].dwKey < dwKey) && (pIndex[dwPos + 1].dwKey > dwKey))
            {
               dwPos++;
               break;
            }
            if (dwKey < pIndex[dwPos].dwKey)
               dwLast = dwPos;
            else if (dwKey > pIndex[dwPos].dwKey)
               dwFirst = dwPos;
            else
            {
               WriteLog(MSG_DUPLICATE_KEY, EVENTLOG_ERROR_TYPE, "x", dwKey);
               return;
            }
         }
   }

   (*pdwIndexSize)++;
   *ppIndex = (INDEX *)realloc(*ppIndex, sizeof(INDEX) * (*pdwIndexSize));
   pIndex = *ppIndex;
   if (dwPos < (*pdwIndexSize - 1))
      memmove(&pIndex[dwPos + 1], &pIndex[dwPos], sizeof(INDEX) * (*pdwIndexSize - dwPos - 1));
   pIndex[dwPos].dwKey = dwKey;
   pIndex[dwPos].pObject = pObject;
}


//
// Perform binary search on index
// Returns INVALID_INDEX if key not found or position of appropriate network object
// We assume that pIndex == NULL will not be passed
//

static DWORD SearchIndex(INDEX *pIndex, DWORD dwIndexSize, DWORD dwKey)
{
   DWORD dwFirst, dwLast, dwMid;

   dwFirst = 0;
   dwLast = dwIndexSize - 1;

   if ((dwKey < pIndex[0].dwKey) || (dwKey > pIndex[dwLast].dwKey))
      return INVALID_INDEX;

   while(dwFirst < dwLast)
   {
      dwMid = (dwFirst + dwLast) / 2;
      if (dwKey == pIndex[dwMid].dwKey)
         return dwMid;
      if (dwKey < pIndex[dwMid].dwKey)
         dwLast = dwMid - 1;
      else
         dwFirst = dwMid + 1;
   }

   if (dwKey == pIndex[dwLast].dwKey)
      return dwLast;

   return INVALID_INDEX;
}


//
// Delete object from index
//

static void DeleteObjectFromIndex(INDEX **ppIndex, DWORD *pdwIndexSize, DWORD dwKey)
{
   DWORD dwPos;
   INDEX *pIndex = *ppIndex;

   dwPos = SearchIndex(pIndex, *pdwIndexSize, dwKey);
   if (dwPos != INVALID_INDEX)
   {
      (*pdwIndexSize)--;
      memmove(&pIndex[dwPos], &pIndex[dwPos + 1], sizeof(INDEX) * (*pdwIndexSize - dwPos));
   }
}


//
// Insert new object into network
//

void NetObjInsert(NetObj *pObject, BOOL bNewObject)
{
   ObjectsGlobalLock();
   if (bNewObject)   
   {
      // Assign unique ID to new object
      pObject->SetId(m_dwFreeObjectId++);

      // Create table for storing data collection values
      if (pObject->Type() == OBJECT_NODE)
      {
         char szQuery[256], szQueryTemplate[256];

         ConfigReadStr("IDataTableCreationCommand", szQueryTemplate, 255, "");
         sprintf(szQuery, szQueryTemplate, pObject->Id());
         DBQuery(g_hCoreDB, szQuery);
      }
   }
   MutexLock(g_hMutexIdIndex, INFINITE);
   AddObjectToIndex(&g_pIndexById, &g_dwIdIndexSize, pObject->Id(), pObject);
   MutexUnlock(g_hMutexIdIndex);
   if (pObject->IpAddr() != 0)
      switch(pObject->Type())
      {
         case OBJECT_GENERIC:
         case OBJECT_NETWORK:
            break;
         case OBJECT_SUBNET:
            MutexLock(g_hMutexSubnetIndex, INFINITE);
            AddObjectToIndex(&g_pSubnetIndexByAddr, &g_dwSubnetAddrIndexSize, pObject->IpAddr(), pObject);
            MutexUnlock(g_hMutexSubnetIndex);
            if (bNewObject)
               PostEvent(EVENT_SUBNET_ADDED, pObject->Id(), NULL);
            break;
         case OBJECT_NODE:
            MutexLock(g_hMutexNodeIndex, INFINITE);
            AddObjectToIndex(&g_pNodeIndexByAddr, &g_dwNodeAddrIndexSize, pObject->IpAddr(), pObject);
            MutexUnlock(g_hMutexNodeIndex);
            if (bNewObject)
               PostEvent(EVENT_NODE_ADDED, pObject->Id(), NULL);
            break;
         case OBJECT_INTERFACE:
            MutexLock(g_hMutexInterfaceIndex, INFINITE);
            AddObjectToIndex(&g_pInterfaceIndexByAddr, &g_dwInterfaceAddrIndexSize, pObject->IpAddr(), pObject);
            MutexUnlock(g_hMutexInterfaceIndex);
            break;
         default:
            WriteLog(MSG_BAD_NETOBJ_TYPE, EVENTLOG_ERROR_TYPE, "d", pObject->Type());
            break;
      }
   ObjectsGlobalUnlock();
}


//
// Delete object
// This function should be called only by syncer thread when access to objects are locked
//

void NetObjDelete(NetObj *pObject)
{
   MutexLock(g_hMutexIdIndex, INFINITE);
   DeleteObjectFromIndex(&g_pIndexById, &g_dwIdIndexSize, pObject->Id());
   MutexUnlock(g_hMutexIdIndex);
   if (pObject->IpAddr() != 0)
      switch(pObject->Type())
      {
         case OBJECT_GENERIC:
         case OBJECT_NETWORK:
            break;
         case OBJECT_SUBNET:
            MutexLock(g_hMutexSubnetIndex, INFINITE);
            DeleteObjectFromIndex(&g_pSubnetIndexByAddr, &g_dwSubnetAddrIndexSize, pObject->IpAddr());
            MutexUnlock(g_hMutexSubnetIndex);
            break;
         case OBJECT_NODE:
            MutexLock(g_hMutexNodeIndex, INFINITE);
            DeleteObjectFromIndex(&g_pNodeIndexByAddr, &g_dwNodeAddrIndexSize, pObject->IpAddr());
            MutexUnlock(g_hMutexNodeIndex);
            break;
         case OBJECT_INTERFACE:
            MutexLock(g_hMutexInterfaceIndex, INFINITE);
            DeleteObjectFromIndex(&g_pInterfaceIndexByAddr, &g_dwInterfaceAddrIndexSize, pObject->IpAddr());
            MutexUnlock(g_hMutexInterfaceIndex);
            break;
         default:
            WriteLog(MSG_BAD_NETOBJ_TYPE, EVENTLOG_ERROR_TYPE, "d", pObject->Type());
            break;
      }
}


//
// Find node by IP address
//

Node *FindNodeByIP(DWORD dwAddr)
{
   DWORD dwPos;
   Node *pNode;

   if ((g_pInterfaceIndexByAddr == NULL) || (dwAddr == 0))
      return NULL;

   MutexLock(g_hMutexInterfaceIndex, INFINITE);
   dwPos = SearchIndex(g_pInterfaceIndexByAddr, g_dwInterfaceAddrIndexSize, dwAddr);
   pNode = (dwPos == INVALID_INDEX) ? NULL : (Node *)g_pInterfaceIndexByAddr[dwPos].pObject->GetParent();
   MutexUnlock(g_hMutexInterfaceIndex);
   return pNode;
}


//
// Find subnet by IP address
//

Subnet *FindSubnetByIP(DWORD dwAddr)
{
   DWORD dwPos;
   Subnet *pSubnet;

   if ((g_pSubnetIndexByAddr == NULL) || (dwAddr == 0))
      return NULL;

   MutexLock(g_hMutexSubnetIndex, INFINITE);
   dwPos = SearchIndex(g_pSubnetIndexByAddr, g_dwSubnetAddrIndexSize, dwAddr);
   pSubnet = (dwPos == INVALID_INDEX) ? NULL : (Subnet *)g_pSubnetIndexByAddr[dwPos].pObject;
   MutexUnlock(g_hMutexSubnetIndex);
   return pSubnet;
}


//
// Find object by ID
//

NetObj *FindObjectById(DWORD dwId)
{
   DWORD dwPos;
   NetObj *pObject;

   if (g_pIndexById == NULL)
      return NULL;

   MutexLock(g_hMutexIdIndex, INFINITE);
   dwPos = SearchIndex(g_pIndexById, g_dwIdIndexSize, dwId);
   pObject = (dwPos == INVALID_INDEX) ? NULL : g_pIndexById[dwPos].pObject;
   MutexUnlock(g_hMutexIdIndex);
   return pObject;
}


//
// Find local management node ID
//

DWORD FindLocalMgmtNode(void)
{
   DWORD i, dwId = 0;

   if (g_pNodeIndexByAddr == NULL)
      return 0;

   MutexLock(g_hMutexNodeIndex, INFINITE);
   for(i = 0; i < g_dwNodeAddrIndexSize; i++)
      if (((Node *)g_pNodeIndexByAddr[i].pObject)->Flags() & NF_IS_LOCAL_MGMT)
      {
         dwId = g_pNodeIndexByAddr[i].pObject->Id();
         break;
      }
   MutexUnlock(g_hMutexNodeIndex);
   return dwId;
}


//
// Load objects from database at stratup
//

BOOL LoadObjects(void)
{
   DB_RESULT hResult;
   int i, iNumRows;
   DWORD dwId;

   // Load subnets
   hResult = DBSelect(g_hCoreDB, "SELECT id FROM subnets WHERE is_deleted=0");
   if (hResult != 0)
   {
      Subnet *pSubnet;

      iNumRows = DBGetNumRows(hResult);
      for(i = 0; i < iNumRows; i++)
      {
         dwId = DBGetFieldULong(hResult, i, 0);
         pSubnet = new Subnet;
         if (pSubnet->CreateFromDB(dwId))
         {
            g_pEntireNet->AddSubnet(pSubnet);
            NetObjInsert(pSubnet, FALSE);  // Insert into indexes
         }
         else     // Object load failed
         {
            delete pSubnet;
            WriteLog(MSG_SUBNET_LOAD_FAILED, EVENTLOG_ERROR_TYPE, "d", dwId);
         }
      }
      DBFreeResult(hResult);
   }

   // Load nodes
   hResult = DBSelect(g_hCoreDB, "SELECT id FROM nodes WHERE is_deleted=0");
   if (hResult != 0)
   {
      Node *pNode;

      iNumRows = DBGetNumRows(hResult);
      for(i = 0; i < iNumRows; i++)
      {
         dwId = DBGetFieldULong(hResult, i, 0);
         pNode = new Node;
         if (pNode->CreateFromDB(dwId))
         {
            NetObjInsert(pNode, FALSE);  // Insert into indexes
         }
         else     // Object load failed
         {
            delete pNode;
            WriteLog(MSG_NODE_LOAD_FAILED, EVENTLOG_ERROR_TYPE, "d", dwId);
         }
      }
      DBFreeResult(hResult);
   }

   // Load interfaces
   hResult = DBSelect(g_hCoreDB, "SELECT id FROM interfaces WHERE is_deleted=0");
   if (hResult != 0)
   {
      Interface *pInterface;

      iNumRows = DBGetNumRows(hResult);
      for(i = 0; i < iNumRows; i++)
      {
         dwId = DBGetFieldULong(hResult, i, 0);
         pInterface = new Interface;
         if (pInterface->CreateFromDB(dwId))
         {
            NetObjInsert(pInterface, FALSE);  // Insert into indexes
         }
         else     // Object load failed
         {
            delete pInterface;
            WriteLog(MSG_INTERFACE_LOAD_FAILED, EVENTLOG_ERROR_TYPE, "d", dwId);
         }
      }
      DBFreeResult(hResult);
   }

   // Recalculate status for "Entire Net" object
   g_pEntireNet->CalculateCompoundStatus();

   // Set first available node ID
   m_dwFreeObjectId = g_pIndexById[g_dwIdIndexSize - 1].dwKey + 1;

   return TRUE;
}
