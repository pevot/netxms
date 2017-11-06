/*
** NetXMS - Network Management System
** Log Parsing Library
** Copyright (C) 2003-2017 Victor Kirhenshtein
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
** File: wevt.cpp
**
**/

#define _WIN32_WINNT 0x0600
#include "libnxlp.h"
#include <vss.h>
#include <vswriter.h>
#include <vsbackup.h>
#include <comdef.h>

/**
* Helper function to display error and return failure in FileSnapshot::create
*/
inline FileSnapshot *CreateFailure(HRESULT hr, IVssBackupComponents *bc, const TCHAR *format)
{
   _com_error err(hr);
   TCHAR buffer[8192];
   _sntprintf(buffer, 8192, format, err.ErrorMessage(), hr);
   nxlog_debug(3, _T("LogWatch: %s"), buffer);
   if (bc != NULL)
      bc->Release();
   return NULL;
}

/**
* Create file snapshot using VSS
*/
FileSnapshot *FileSnapshot::create(const TCHAR *path)
{
   IVssBackupComponents *bc;
   HRESULT hr = CreateVssBackupComponents(&bc);
   if (FAILED(hr))
      return CreateFailure(hr, NULL, _T("Call to CreateVssBackupComponents failed (%s) HRESULT=0x%08X"));

   hr = bc->InitializeForBackup();
   if (FAILED(hr))
      return CreateFailure(hr, bc, _T("Call to IVssBackupComponents::InitializeForBackup failed (%s) HRESULT=0x%08X"));

   hr = bc->SetBackupState(false, false, VSS_BT_COPY);
   if (FAILED(hr))
      return CreateFailure(hr, bc, _T("Call to IVssBackupComponents::SetBackupState failed (%s) HRESULT=0x%08X"));

   hr = bc->SetContext(VSS_CTX_FILE_SHARE_BACKUP);
   if (FAILED(hr))
      return CreateFailure(hr, bc, _T("Call to IVssBackupComponents::SetContext failed (%s) HRESULT=0x%08X"));

   VSS_ID snapshotSetId;
   hr = bc->StartSnapshotSet(&snapshotSetId);
   if (FAILED(hr))
      return CreateFailure(hr, bc, _T("Call to IVssBackupComponents::StartSnapshotSet failed (%s) HRESULT=0x%08X"));

   const TCHAR *s = _tcschr(path, _T(':'));
   if (s == NULL)
      return CreateFailure(S_OK, bc, _T("Unsupported file path format"));
   s++;

   size_t len = s - path;
   TCHAR device[64];
   _tcslcpy(device, path, std::min(static_cast<size_t>(64), len + 1));
   _tcslcat(device, _T("\\"), 64);
   nxlog_debug(7, _T("LogWatch: Adding device %s to VSS snapshot"), device);

   VSS_ID snapshotId;
   hr = bc->AddToSnapshotSet(device, GUID_NULL, &snapshotId);
   if (FAILED(hr))
      return CreateFailure(hr, bc, _T("Call to IVssBackupComponents::AddToSnapshotSet failed (%s) HRESULT=0x%08X"));

   IVssAsync *async;
   hr = bc->DoSnapshotSet(&async);
   if (FAILED(hr))
      return CreateFailure(hr, bc, _T("Call to IVssBackupComponents::DoSnapshotSet failed (%s) HRESULT=0x%08X"));

   hr = async->Wait();
   async->Release();
   if (FAILED(hr))
      return CreateFailure(hr, bc, _T("Call to IVssAsync::Wait failed (%s) HRESULT=0x%08X"));

   VSS_SNAPSHOT_PROP prop;
   hr = bc->GetSnapshotProperties(snapshotId, &prop);
   if (FAILED(hr))
      return CreateFailure(hr, bc, _T("Call to IVssBackupComponents::GetSnapshotProperties failed (%s) HRESULT=0x%08X"));

   nxlog_debug(7, _T("LogWatch: Created VSS snapshot %s"), prop.m_pwszSnapshotDeviceObject);
   String sname(prop.m_pwszSnapshotDeviceObject);
   sname.append(s);

   FileSnapshot *object = new FileSnapshot();
   object->m_handle = bc;
   object->m_name = _tcsdup(sname);
   return object;
}

/**
 * File snapshot object constructor
 */
FileSnapshot::FileSnapshot()
{
   m_handle = NULL;
   m_name = NULL;
}

/**
 * File snapshot destructor
 */
FileSnapshot::~FileSnapshot()
{
   m_handle->Release();
   free(m_name);
}
