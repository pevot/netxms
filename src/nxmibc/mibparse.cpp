/* 
** NetXMS - Network Management System
** NetXMS MIB compiler
** Copyright (C) 2005 Victor Kirhenshtein
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
** $module: mibparse.cpp
**
**/

#include "nxmibc.h"
#include "mibparse.h"


//
// Do actual builtin object creation
//

static MP_OBJECT *CreateObject(char *pszName, DWORD dwId)
{
   MP_OBJECT *pObject;
   MP_SUBID *pSubId;

   pObject = CREATE(MP_OBJECT);
   pObject->pszName = strdup(pszName);
   pObject->iType = MIBC_OBJECT;
   pObject->pOID = da_create();
   pSubId = CREATE(MP_SUBID);
   pSubId->bResolved = TRUE;
   pSubId->dwValue = dwId;
   pSubId->pszName = strdup(pszName);
   da_add(pObject->pOID, pSubId);
   return pObject;
}


//
// Create builtin object
//

static MP_OBJECT *CreateBuiltinObject(char *pszName)
{
   MP_OBJECT *pObject = NULL;

   if (!strcmp(pszName, "iso"))
   {
      pObject = CreateObject("iso", 1);
   }
   else if (!strcmp(pszName, "ccitt"))
   {
      pObject = CreateObject("ccitt", 0);
   }
   return pObject;
}


//
// Find module by name
//

static MP_MODULE *FindModuleByName(DynArray *pModuleList, char *pszName)
{
   int i, iNumModules;

   iNumModules = da_size(pModuleList);
   for(i = 0; i < iNumModules; i++)
      if (!strcmp(((MP_MODULE *)da_get(pModuleList, i))->pszName, pszName))
         return (MP_MODULE *)da_get(pModuleList, i);
   return NULL;
}


//
// Find object in module
//

static MP_OBJECT *FindObjectByName(MP_MODULE *pModule, char *pszName)
{
   int i, iNumObjects;

   iNumObjects = da_size(pModule->pObjectList);
   for(i = 0; i < iNumObjects; i++)
   {
      if (!strcmp(((MP_OBJECT *)da_get(pModule->pObjectList, i))->pszName, pszName))
         return (MP_OBJECT *)da_get(pModule->pObjectList, i);
   }
   return NULL;
}


//
// Find imported object in module
//

static MP_OBJECT *FindImportedObjectByName(MP_MODULE *pModule, char *pszName,
                                           MP_MODULE **ppImportModule)
{
   int i, j, iNumImports, iNumSymbols;
   MP_IMPORT_MODULE *pImport;

   iNumImports = da_size(pModule->pImportList);
   for(i = 0; i < iNumImports; i++)
   {
      pImport = (MP_IMPORT_MODULE *)da_get(pModule->pImportList, i);
      iNumSymbols = da_size(pImport->pSymbols);
      for(j = 0; j < iNumSymbols; j++)
         if (!strcmp((char *)da_get(pImport->pSymbols, j), pszName))
         {
            *ppImportModule = pImport->pModule;
            return (MP_OBJECT *)da_get(pImport->pObjects, j);
         }
   }
   return NULL;
}


//
// Find next module in chain, if symbol is imported and then re-exported
//

static MP_MODULE *FindNextImportModule(DynArray *pModuleList, MP_MODULE *pModule,
                                       char *pszSymbol)
{
   int i, j, iNumImports, iNumSymbols;
   MP_IMPORT_MODULE *pImport;

   iNumImports = da_size(pModule->pImportList);
   for(i = 0; i < iNumImports; i++)
   {
      pImport = (MP_IMPORT_MODULE *)da_get(pModule->pImportList, i);
      iNumSymbols = da_size(pImport->pSymbols);
      for(j = 0; j < iNumSymbols; j++)
         if (!strcmp((char *)da_get(pImport->pSymbols, j), pszSymbol))
            return FindModuleByName(pModuleList, pImport->pszName);
   }
   return NULL;
}


//
// Resolve imports
//

static void ResolveImports(DynArray *pModuleList, MP_MODULE *pModule)
{
   int i, j, iNumImports, iNumSymbols;
   MP_MODULE *pImportModule;
   MP_IMPORT_MODULE *pImport;
   MP_OBJECT *pObject;
   char *pszSymbol;

   iNumImports = da_size(pModule->pImportList);
   for(i = 0; i < iNumImports; i++)
   {
      pImport = (MP_IMPORT_MODULE *)da_get(pModule->pImportList, i);
      pImportModule = FindModuleByName(pModuleList, pImport->pszName);
      if (pImportModule != NULL)
      {
         pImport->pModule = pImportModule;
         iNumSymbols = da_size(pImport->pSymbols);
         pImport->pObjects = da_create();
         for(j = 0; j < iNumSymbols; j++)
         {
            pszSymbol = (char *)da_get(pImport->pSymbols, j);
            do
            {
               pObject = FindObjectByName(pImportModule, pszSymbol);
               if (pObject != NULL)
               {
                  da_add(pImport->pObjects, pObject);
               }
               else
               {
                  pImportModule = FindNextImportModule(pModuleList, pImportModule, pszSymbol);
                  if (pImportModule == NULL)
                  {
                     Error(ERR_UNRESOLVED_IMPORT, pModule->pszName, pszSymbol);
                     da_add(pImport->pObjects, NULL);
                     break;
                  }
               }
            } while(pObject == NULL);
         }
      }
      else
      {
         Error(ERR_UNRESOLVED_MODULE, pModule->pszName, pImport->pszName);
      }
   }
}


//
// Build full OID for object
//

static void BuildFullOID(MP_MODULE *pModule, MP_OBJECT *pObject)
{
   int iLen;
   MP_SUBID *pSubId;
   MP_OBJECT *pParent;
   DynArray *pOID;

   iLen = da_size(pObject->pOID);
   while(iLen > 0)
   {
      pSubId = (MP_SUBID *)da_get(pObject->pOID, iLen - 1);
      if (!pSubId->bResolved)
      {
         pParent = FindObjectByName(pModule, pSubId->pszName);
         if (pParent != NULL)
         {
            BuildFullOID(pModule, pParent);
         }
         else
         {
            MP_MODULE *pImportModule;

            pParent = FindImportedObjectByName(pModule, pSubId->pszName, &pImportModule);
            if (pParent != NULL)
            {
               BuildFullOID(pImportModule, pParent);
            }
            else
            {
               pParent = CreateBuiltinObject(pSubId->pszName);
            }
         }
         if (pParent != NULL)
         {
            pOID = da_create();
            pOID->nSize = pParent->pOID->nSize + pObject->pOID->nSize - iLen;
            pOID->ppData = (void **)malloc(sizeof(void *) * pOID->nSize);
            memcpy(pOID->ppData, pParent->pOID->ppData, sizeof(void *) * pParent->pOID->nSize);
            memcpy(&pOID->ppData[pParent->pOID->nSize], &pObject->pOID->ppData[iLen],
                   sizeof(void *) * (pObject->pOID->nSize - iLen));
            da_destroy(pObject->pOID);
            pObject->pOID = pOID;
            break;
         }
         else
         {
            Error(ERR_UNRESOLVED_SYMBOL, pModule->pszName, pSubId->pszName);
         }
      }
      else
      {
         iLen--;
      }
   }
}


//
// Resolve syntax for object
//

static void ResolveSyntax(MP_MODULE *pModule, MP_OBJECT *pObject)
{
   MP_OBJECT *pType;
   MP_MODULE *pCurrModule = pModule;
   char *pszType;

   if ((pObject->iSyntax != -1) || (pObject->pszDataType == NULL))
      return;

   pszType = pObject->pszDataType;
   do
   {
      pType = FindObjectByName(pCurrModule, CHECK_NULL(pszType));
      if (pType == NULL)
         pType = FindImportedObjectByName(pCurrModule,
                                          CHECK_NULL(pszType),
                                          &pCurrModule);
      if (pType == NULL)
         break;
      pszType = pType->pszDataType;
   } while(pType->iSyntax == -1);

   if (pType != NULL)
   {
      pObject->iSyntax = pType->iSyntax;
   }
   else
   {
      Error(ERR_UNRESOLVED_SYNTAX, pModule->pszName, pObject->pszDataType, pObject->pszName);
   }
}


//
// Resolve object identifiers
//

static void ResolveObjects(DynArray *pModuleList, MP_MODULE *pModule)
{
   int i, iNumObjects;
   MP_OBJECT *pObject;

   iNumObjects = da_size(pModule->pObjectList);
   for(i = 0; i < iNumObjects; i++)
   {
      pObject = (MP_OBJECT *)da_get(pModule->pObjectList, i);
      if (pObject->iType == MIBC_OBJECT)
      {
         BuildFullOID(pModule, pObject);
         ResolveSyntax(pModule, pObject);
      }
   }
}


//
// Build MIB tree from object list
//

static void BuildMIBTree(SNMP_MIBObject *pRoot, MP_MODULE *pModule)
{
   int i, j, iLen, iNumObjects;
   MP_OBJECT *pObject;
   MP_SUBID *pSubId;
   SNMP_MIBObject *pCurrObj, *pNewObj;

   iNumObjects = da_size(pModule->pObjectList);
   for(i = 0; i < iNumObjects; i++)
   {
      pObject = (MP_OBJECT *)da_get(pModule->pObjectList, i);
      if (pObject->iType == MIBC_OBJECT)
      {
         iLen = da_size(pObject->pOID);
         for(j = 0, pCurrObj = pRoot; j < iLen; j++)
         {
            pSubId = (MP_SUBID *)da_get(pObject->pOID, j);
            pNewObj = pCurrObj->FindChildByID(pSubId->dwValue);
            if (pNewObj == NULL)
            {
               if (j == iLen - 1)
                  pNewObj = new SNMP_MIBObject(pSubId->dwValue, pObject->pszName,
                                               pObject->iSyntax,
                                               pObject->iStatus,
                                               pObject->iAccess,
                                               pObject->pszDescription);
               else
                  pNewObj = new SNMP_MIBObject(pSubId->dwValue, pSubId->pszName);
               pCurrObj->AddChild(pNewObj);
            }
            else
            {
               if (j == iLen - 1)
               {
                  // Last OID in chain, update object information
                  pNewObj->SetInfo(pObject->iSyntax, pObject->iStatus,
                                   pObject->iAccess, pObject->pszDescription);
                  if (pNewObj->Name() == NULL)
                     pNewObj->SetName(pObject->pszName);
               }
            }
            pCurrObj = pNewObj;
         }
      }
   }
}


//
// Interface to parser
//

int ParseMIBFiles(int nNumFiles, char **ppszFileList, SNMP_MIBObject **ppRoot)
{
   int i, iNumModules, nRet;
   DynArray *pModuleList;
   MP_MODULE *pModule;
   SNMP_MIBObject *pRoot;

   printf("Parsing source files:\n");
   pModuleList = da_create();
   for(i = 0; i < nNumFiles; i++)
   {
      printf("   %s\n", ppszFileList[i]);
      pModule = ParseMIB(ppszFileList[i]);
      if (pModule == NULL)
      {
         nRet = SNMP_MPE_PARSE_ERROR;
         goto parse_error;
      }
      da_add(pModuleList, pModule);
   }

   iNumModules = da_size(pModuleList);
   printf("Resolving imports:\n");
   for(i = 0; i < iNumModules; i++)
   {
      pModule = (MP_MODULE *)da_get(pModuleList, i);
      printf("   %s\n", pModule->pszName);
      ResolveImports(pModuleList, pModule);
   }

   printf("Resolving object identifiers:\n");
   for(i = 0; i < iNumModules; i++)
   {
      pModule = (MP_MODULE *)da_get(pModuleList, i);
      printf("   %s\n", pModule->pszName);
      ResolveObjects(pModuleList, pModule);
   }

   printf("Creating MIB tree:\n");
   pRoot = new SNMP_MIBObject;
   for(i = 0; i < iNumModules; i++)
   {
      pModule = (MP_MODULE *)da_get(pModuleList, i);
      printf("   %s\n", pModule->pszName);
      BuildMIBTree(pRoot, pModule);
   }

   *ppRoot = pRoot;
   return SNMP_MPE_SUCCESS;

parse_error:
   *ppRoot = NULL;
   return nRet;
}
