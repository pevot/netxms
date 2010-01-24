/* 
** NetXMS multiplatform core agent
** Copyright (C) 2003-2009 Victor Kirhenshtein
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
** File: session.cpp
**
**/

#include "nxagentd.h"

#ifdef _WIN32
#define write	_write
#define close	_close
#endif


//
// Externals
//

void UnregisterSession(DWORD dwIndex);
void ProxySNMPRequest(CSCPMessage *pRequest, CSCPMessage *pResponse);
DWORD DeployPolicy(DWORD session, CSCPMessage *request);
DWORD UninstallPolicy(DWORD session, CSCPMessage *request);


//
// Constants
//

#define RAW_MSG_SIZE    262144


//
// Client communication read thread
//

THREAD_RESULT THREAD_CALL CommSession::readThreadStarter(void *pArg)
{
   ((CommSession *)pArg)->readThread();

   // When CommSession::ReadThread exits, all other session
   // threads are already stopped, so we can safely destroy
   // session object
   UnregisterSession(((CommSession *)pArg)->getIndex());
   delete (CommSession *)pArg;
   return THREAD_OK;
}


//
// Client communication write thread
//

THREAD_RESULT THREAD_CALL CommSession::writeThreadStarter(void *pArg)
{
   ((CommSession *)pArg)->writeThread();
   return THREAD_OK;
}


//
// Received message processing thread
//

THREAD_RESULT THREAD_CALL CommSession::processingThreadStarter(void *pArg)
{
   ((CommSession *)pArg)->processingThread();
   return THREAD_OK;
}


//
// Client communication write thread
//

THREAD_RESULT THREAD_CALL CommSession::proxyReadThreadStarter(void *pArg)
{
   ((CommSession *)pArg)->proxyReadThread();
   return THREAD_OK;
}


//
// Client session class constructor
//

CommSession::CommSession(SOCKET hSocket, DWORD dwHostAddr,
                         BOOL bMasterServer, BOOL bControlServer)
{
   m_pSendQueue = new Queue;
   m_pMessageQueue = new Queue;
   m_hSocket = hSocket;
   m_hProxySocket = -1;
   m_dwIndex = INVALID_INDEX;
   m_pMsgBuffer = (CSCP_BUFFER *)malloc(sizeof(CSCP_BUFFER));
   m_hWriteThread = INVALID_THREAD_HANDLE;
   m_hProcessingThread = INVALID_THREAD_HANDLE;
   m_dwHostAddr = dwHostAddr;
   m_bIsAuthenticated = (g_dwFlags & AF_REQUIRE_AUTH) ? FALSE : TRUE;
   m_bMasterServer = bMasterServer;
   m_bControlServer = bControlServer;
   m_bProxyConnection = FALSE;
   m_bAcceptTraps = FALSE;
   m_hCurrFile = -1;
   m_pCtx = NULL;
   m_ts = time(NULL);
}


//
// Destructor
//

CommSession::~CommSession()
{
   shutdown(m_hSocket, SHUT_RDWR);
   closesocket(m_hSocket);
   if (m_hProxySocket != -1)
      closesocket(m_hProxySocket);
   delete m_pSendQueue;
   delete m_pMessageQueue;
   safe_free(m_pMsgBuffer);
   if (m_hCurrFile != -1)
      close(m_hCurrFile);
   DestroyEncryptionContext(m_pCtx);
}


//
// Start all threads
//

void CommSession::run()
{
   m_hWriteThread = ThreadCreateEx(writeThreadStarter, 0, this);
   m_hProcessingThread = ThreadCreateEx(processingThreadStarter, 0, this);
   ThreadCreate(readThreadStarter, 0, this);
}


//
// Disconnect session
//

void CommSession::disconnect()
{
	DebugPrintf(m_dwIndex, 5, "CommSession::disconnect()");
   shutdown(m_hSocket, SHUT_RDWR);
   if (m_hProxySocket != -1)
      shutdown(m_hProxySocket, SHUT_RDWR);
}


//
// Reading thread
//

void CommSession::readThread()
{
   CSCP_MESSAGE *pRawMsg;
   CSCPMessage *pMsg;
   BYTE *pDecryptionBuffer = NULL;
   int iErr;
   char szBuffer[256];
   WORD wFlags;

   // Initialize raw message receiving function
   RecvNXCPMessage(0, NULL, m_pMsgBuffer, 0, NULL, NULL, 0);

   pRawMsg = (CSCP_MESSAGE *)malloc(RAW_MSG_SIZE);
#ifdef _WITH_ENCRYPTION
   pDecryptionBuffer = (BYTE *)malloc(RAW_MSG_SIZE);
#endif
   while(1)
   {
      if ((iErr = RecvNXCPMessage(m_hSocket, pRawMsg, m_pMsgBuffer, RAW_MSG_SIZE,
                                  &m_pCtx, pDecryptionBuffer, INFINITE)) <= 0)
      {
         break;
      }

      // Check if message is too large
      if (iErr == 1)
         continue;

      // Check for decryption failure
      if (iErr == 2)
      {
         nxlog_write(MSG_DECRYPTION_FAILURE, EVENTLOG_WARNING_TYPE, NULL);
         continue;
      }

      // Check that actual received packet size is equal to encoded in packet
      if ((int)ntohl(pRawMsg->dwSize) != iErr)
      {
         DebugPrintf(m_dwIndex, 5, "Actual message size doesn't match wSize value (%d,%d)", iErr, ntohl(pRawMsg->dwSize));
         continue;   // Bad packet, wait for next
      }

      // Update activity timestamp
      m_ts = time(NULL);

      if (m_bProxyConnection)
      {
         // Forward received message to remote peer
         SendEx(m_hProxySocket, (char *)pRawMsg, iErr, 0);
      }
      else
      {
         wFlags = ntohs(pRawMsg->wFlags);
         if (wFlags & MF_BINARY)
         {
            // Convert message header to host format
            pRawMsg->dwId = ntohl(pRawMsg->dwId);
            pRawMsg->wCode = ntohs(pRawMsg->wCode);
            pRawMsg->dwNumVars = ntohl(pRawMsg->dwNumVars);
            DebugPrintf(m_dwIndex, 6, "Received raw message %s", NXCPMessageCodeName(pRawMsg->wCode, szBuffer));

            if (pRawMsg->wCode == CMD_FILE_DATA)
            {
               if ((m_hCurrFile != -1) && (m_dwFileRqId == pRawMsg->dwId))
               {
                  if (write(m_hCurrFile, pRawMsg->df, pRawMsg->dwNumVars) == (int)pRawMsg->dwNumVars)
                  {
                     if (wFlags & MF_END_OF_FILE)
                     {
                        CSCPMessage msg;

                        close(m_hCurrFile);
                        m_hCurrFile = -1;
                  
                        msg.SetCode(CMD_REQUEST_COMPLETED);
                        msg.SetId(pRawMsg->dwId);
                        msg.SetVariable(VID_RCC, ERR_SUCCESS);
                        sendMessage(&msg);
                     }
                  }
                  else
                  {
                     // I/O error
                     CSCPMessage msg;

                     close(m_hCurrFile);
                     m_hCurrFile = -1;
               
                     msg.SetCode(CMD_REQUEST_COMPLETED);
                     msg.SetId(pRawMsg->dwId);
                     msg.SetVariable(VID_RCC, ERR_IO_FAILURE);
                     sendMessage(&msg);
                  }
               }
            }
         }
         else if (wFlags & MF_CONTROL)
         {
            // Convert message header to host format
            pRawMsg->dwId = ntohl(pRawMsg->dwId);
            pRawMsg->wCode = ntohs(pRawMsg->wCode);
            pRawMsg->dwNumVars = ntohl(pRawMsg->dwNumVars);
            DebugPrintf(m_dwIndex, 6, "Received control message %s", NXCPMessageCodeName(pRawMsg->wCode, szBuffer));

            if (pRawMsg->wCode == CMD_GET_NXCP_CAPS)
            {
               CSCP_MESSAGE *pMsg;

               pMsg = (CSCP_MESSAGE *)malloc(CSCP_HEADER_SIZE);
               pMsg->dwId = htonl(pRawMsg->dwId);
               pMsg->wCode = htons((WORD)CMD_NXCP_CAPS);
               pMsg->wFlags = htons(MF_CONTROL);
               pMsg->dwNumVars = htonl(NXCP_VERSION << 24);
               pMsg->dwSize = htonl(CSCP_HEADER_SIZE);
               sendRawMessage(pMsg, m_pCtx);
            }
         }
         else
         {
            // Create message object from raw message
            pMsg = new CSCPMessage(pRawMsg);
            if (pMsg->GetCode() == CMD_REQUEST_SESSION_KEY)
            {
               DebugPrintf(m_dwIndex, 6, "Received message %s", NXCPMessageCodeName(pMsg->GetCode(), szBuffer));
               if (m_pCtx == NULL)
               {
                  CSCPMessage *pResponse;

                  SetupEncryptionContext(pMsg, &m_pCtx, &pResponse, NULL, NXCP_VERSION);
                  sendMessage(pResponse);
                  delete pResponse;
               }
               delete pMsg;
            }
            else
            {
               m_pMessageQueue->Put(pMsg);
            }
         }
      }
   }
   if (iErr < 0)
      nxlog_write(MSG_SESSION_BROKEN, EVENTLOG_WARNING_TYPE, "e", WSAGetLastError());
   free(pRawMsg);
#ifdef _WITH_ENCRYPTION
   free(pDecryptionBuffer);
#endif

   // Notify other threads to exit
   m_pSendQueue->Put(INVALID_POINTER_VALUE);
   m_pMessageQueue->Put(INVALID_POINTER_VALUE);
   if (m_hProxySocket != -1)
      shutdown(m_hProxySocket, SHUT_RDWR);

   // Wait for other threads to finish
   ThreadJoin(m_hWriteThread);
   ThreadJoin(m_hProcessingThread);
   if (m_bProxyConnection)
      ThreadJoin(m_hProxyReadThread);

   DebugPrintf(m_dwIndex, 5, "Session with %s closed", IpToStr(m_dwHostAddr, szBuffer));
}


//
// Send prepared raw message over the network and destroy it
//

BOOL CommSession::sendRawMessage(CSCP_MESSAGE *pMsg, CSCP_ENCRYPTION_CONTEXT *pCtx)
{
   BOOL bResult = TRUE;
   char szBuffer[128];

   DebugPrintf(m_dwIndex, 6, "Sending message %s (size %d)", NXCPMessageCodeName(ntohs(pMsg->wCode), szBuffer), ntohl(pMsg->dwSize));
   if ((pCtx != NULL) && (pCtx != PROXY_ENCRYPTION_CTX))
   {
      CSCP_ENCRYPTED_MESSAGE *pEnMsg;

      pEnMsg = CSCPEncryptMessage(pCtx, pMsg);
      free(pMsg);
      if (pEnMsg != NULL)
      {
         if (SendEx(m_hSocket, (const char *)pEnMsg, ntohl(pEnMsg->dwSize), 0) <= 0)
         {
            bResult = FALSE;
         }
         free(pEnMsg);
      }
   }
   else
   {
      if (SendEx(m_hSocket, (const char *)pMsg, ntohl(pMsg->dwSize), 0) <= 0)
      {
         bResult = FALSE;
      }
      free(pMsg);
   }
	if (!bResult)
	   DebugPrintf(m_dwIndex, 6, "CommSession::SendRawMessage() for %s (size %d) failed", NXCPMessageCodeName(ntohs(pMsg->wCode), szBuffer), ntohl(pMsg->dwSize));
   return bResult;
}


//
// Writing thread
//

void CommSession::writeThread(void)
{
   CSCP_MESSAGE *pMsg;

   while(1)
   {
      pMsg = (CSCP_MESSAGE *)m_pSendQueue->GetOrBlock();
      if (pMsg == INVALID_POINTER_VALUE)    // Session termination indicator
         break;

      if (!sendRawMessage(pMsg, m_pCtx))
         break;
   }
   m_pSendQueue->Clear();
}


//
// Message processing thread
//

void CommSession::processingThread(void)
{
   CSCPMessage *pMsg;
   char szBuffer[128];
   CSCPMessage msg;
   DWORD dwCommand, dwRet;

   while(1)
   {
      pMsg = (CSCPMessage *)m_pMessageQueue->GetOrBlock();
      if (pMsg == INVALID_POINTER_VALUE)    // Session termination indicator
         break;
      dwCommand = pMsg->GetCode();
      DebugPrintf(m_dwIndex, 6, "Received message %s", NXCPMessageCodeName((WORD)dwCommand, szBuffer));

      // Prepare response message
      msg.SetCode(CMD_REQUEST_COMPLETED);
      msg.SetId(pMsg->GetId());

      // Check if authentication required
      if ((!m_bIsAuthenticated) && (dwCommand != CMD_AUTHENTICATE))
      {
         msg.SetVariable(VID_RCC, ERR_AUTH_REQUIRED);
      }
      else if ((g_dwFlags & AF_REQUIRE_ENCRYPTION) && (m_pCtx == NULL))
      {
         msg.SetVariable(VID_RCC, ERR_ENCRYPTION_REQUIRED);
      }
      else
      {
         switch(dwCommand)
         {
            case CMD_AUTHENTICATE:
               authenticate(pMsg, &msg);
               break;
            case CMD_GET_PARAMETER:
               getParameter(pMsg, &msg);
               break;
            case CMD_GET_LIST:
               getList(pMsg, &msg);
               break;
            case CMD_KEEPALIVE:
               msg.SetVariable(VID_RCC, ERR_SUCCESS);
               break;
            case CMD_ACTION:
               action(pMsg, &msg);
               break;
            case CMD_TRANSFER_FILE:
               recvFile(pMsg, &msg);
               break;
            case CMD_UPGRADE_AGENT:
               msg.SetVariable(VID_RCC, upgrade(pMsg));
               break;
            case CMD_GET_PARAMETER_LIST:
               msg.SetVariable(VID_RCC, ERR_SUCCESS);
               GetParameterList(&msg);
               break;
            case CMD_GET_AGENT_CONFIG:
               getConfig(&msg);
               break;
            case CMD_UPDATE_AGENT_CONFIG:
               updateConfig(pMsg, &msg);
               break;
            case CMD_SETUP_PROXY_CONNECTION:
               dwRet = setupProxyConnection(pMsg);
               // Proxy session established, incoming messages will
               // not be processed locally. Acknowledgement message sent
               // by SetupProxyConnection() in case of success.
               if (dwRet == ERR_SUCCESS)
                  goto stop_processing;
               msg.SetVariable(VID_RCC, dwRet);
               break;
            case CMD_ENABLE_AGENT_TRAPS:
               if (m_bMasterServer)
               {
                  m_bAcceptTraps = TRUE;
                  msg.SetVariable(VID_RCC, ERR_SUCCESS);
               }
               else
               {
                  msg.SetVariable(VID_RCC, ERR_ACCESS_DENIED);
               }
               break;
				case CMD_SNMP_REQUEST:
					if (m_bMasterServer && (g_dwFlags & AF_ENABLE_SNMP_PROXY))
					{
						ProxySNMPRequest(pMsg, &msg);
					}
					else
					{
                  msg.SetVariable(VID_RCC, ERR_ACCESS_DENIED);
					}
					break;
				case CMD_DEPLOY_AGENT_POLICY:
					if (m_bMasterServer)
					{
						msg.SetVariable(VID_RCC, DeployPolicy(m_dwIndex, pMsg));
					}
					else
					{
                  msg.SetVariable(VID_RCC, ERR_ACCESS_DENIED);
					}
					break;
				case CMD_UNINSTALL_AGENT_POLICY:
					if (m_bMasterServer)
					{
						msg.SetVariable(VID_RCC, UninstallPolicy(m_dwIndex, pMsg));
					}
					else
					{
                  msg.SetVariable(VID_RCC, ERR_ACCESS_DENIED);
					}
					break;
            default:
               // Attempt to process unknown command by subagents
               if (!ProcessCmdBySubAgent(dwCommand, pMsg, &msg, this))
                  msg.SetVariable(VID_RCC, ERR_UNKNOWN_COMMAND);
               break;
         }
      }
      delete pMsg;

      // Send response
      sendMessage(&msg);
      msg.DeleteAllVariables();
   }

stop_processing:
   ;
}


//
// Authenticate peer
//

void CommSession::authenticate(CSCPMessage *pRequest, CSCPMessage *pMsg)
{
   if (m_bIsAuthenticated)
   {
      // Already authenticated
      pMsg->SetVariable(VID_RCC, (g_dwFlags & AF_REQUIRE_AUTH) ? ERR_ALREADY_AUTHENTICATED : ERR_AUTH_NOT_REQUIRED);
   }
   else
   {
      char szSecret[MAX_SECRET_LENGTH];
      BYTE hash[32];
      WORD wAuthMethod;

      wAuthMethod = pRequest->GetVariableShort(VID_AUTH_METHOD);
      switch(wAuthMethod)
      {
         case AUTH_PLAINTEXT:
            pRequest->GetVariableStr(VID_SHARED_SECRET, szSecret, MAX_SECRET_LENGTH);
            if (!strcmp(szSecret, g_szSharedSecret))
            {
               m_bIsAuthenticated = TRUE;
               pMsg->SetVariable(VID_RCC, ERR_SUCCESS);
            }
            else
            {
               nxlog_write(MSG_AUTH_FAILED, EVENTLOG_WARNING_TYPE, "as", m_dwHostAddr, "PLAIN");
               pMsg->SetVariable(VID_RCC, ERR_AUTH_FAILED);
            }
            break;
         case AUTH_MD5_HASH:
            pRequest->GetVariableBinary(VID_SHARED_SECRET, (BYTE *)szSecret, MD5_DIGEST_SIZE);
            CalculateMD5Hash((BYTE *)g_szSharedSecret, strlen(g_szSharedSecret), hash);
            if (!memcmp(szSecret, hash, MD5_DIGEST_SIZE))
            {
               m_bIsAuthenticated = TRUE;
               pMsg->SetVariable(VID_RCC, ERR_SUCCESS);
            }
            else
            {
               nxlog_write(MSG_AUTH_FAILED, EVENTLOG_WARNING_TYPE, "as", m_dwHostAddr, "MD5");
               pMsg->SetVariable(VID_RCC, ERR_AUTH_FAILED);
            }
            break;
         case AUTH_SHA1_HASH:
            pRequest->GetVariableBinary(VID_SHARED_SECRET, (BYTE *)szSecret, SHA1_DIGEST_SIZE);
            CalculateSHA1Hash((BYTE *)g_szSharedSecret, strlen(g_szSharedSecret), hash);
            if (!memcmp(szSecret, hash, SHA1_DIGEST_SIZE))
            {
               m_bIsAuthenticated = TRUE;
               pMsg->SetVariable(VID_RCC, ERR_SUCCESS);
            }
            else
            {
               nxlog_write(MSG_AUTH_FAILED, EVENTLOG_WARNING_TYPE, "as", m_dwHostAddr, "SHA1");
               pMsg->SetVariable(VID_RCC, ERR_AUTH_FAILED);
            }
            break;
         default:
            pMsg->SetVariable(VID_RCC, ERR_NOT_IMPLEMENTED);
            break;
      }
   }
}


//
// Get parameter's value
//

void CommSession::getParameter(CSCPMessage *pRequest, CSCPMessage *pMsg)
{
   char szParameter[MAX_PARAM_NAME], szValue[MAX_RESULT_LENGTH];
   DWORD dwErrorCode;

   pRequest->GetVariableStr(VID_PARAMETER, szParameter, MAX_PARAM_NAME);
   dwErrorCode = GetParameterValue(m_dwIndex, szParameter, szValue);
   pMsg->SetVariable(VID_RCC, dwErrorCode);
   if (dwErrorCode == ERR_SUCCESS)
      pMsg->SetVariable(VID_VALUE, szValue);
}


//
// Get list of values
//

void CommSession::getList(CSCPMessage *pRequest, CSCPMessage *pMsg)
{
   char szParameter[MAX_PARAM_NAME];

   pRequest->GetVariableStr(VID_PARAMETER, szParameter, MAX_PARAM_NAME);

   StringList value;
   DWORD dwErrorCode = GetEnumValue(m_dwIndex, szParameter, &value);
   pMsg->SetVariable(VID_RCC, dwErrorCode);
   if (dwErrorCode == ERR_SUCCESS)
   {
		pMsg->SetVariable(VID_NUM_STRINGS, (DWORD)value.getSize());
		for(int i = 0; i < value.getSize(); i++)
			pMsg->SetVariable(VID_ENUM_VALUE_BASE + i, value.getValue(i));
   }
}


//
// Perform action on request
//

void CommSession::action(CSCPMessage *pRequest, CSCPMessage *pMsg)
{
   char szAction[MAX_PARAM_NAME];
   StringList args;
   DWORD i, dwRetCode;

   if ((g_dwFlags & AF_ENABLE_ACTIONS) && m_bControlServer)
   {
      // Get action name and arguments
      pRequest->GetVariableStr(VID_ACTION_NAME, szAction, MAX_PARAM_NAME);
      DWORD numArgs = pRequest->GetVariableLong(VID_NUM_ARGS);
      for(i = 0; i < numArgs; i++)
			args.addPreallocated(pRequest->GetVariableStr(VID_ACTION_ARG_BASE + i));

      // Execute action
      dwRetCode = ExecAction(szAction, &args);
      pMsg->SetVariable(VID_RCC, dwRetCode);
   }
   else
   {
      pMsg->SetVariable(VID_RCC, ERR_ACCESS_DENIED);
   }
}


//
// Prepare for receiving file
//

void CommSession::recvFile(CSCPMessage *pRequest, CSCPMessage *pMsg)
{
   TCHAR szFileName[MAX_PATH], szFullPath[MAX_PATH];

   if (m_bMasterServer)
   {
      szFileName[0] = 0;
      pRequest->GetVariableStr(VID_FILE_NAME, szFileName, MAX_PATH);
		DebugPrintf(m_dwIndex, 5, "CommSession::recvFile(): Preparing for receiving file \"%s\"", szFileName);
      BuildFullPath(szFileName, szFullPath);

      // Check if for some reason we have already opened file
      if (m_hCurrFile != -1)
      {
         pMsg->SetVariable(VID_RCC, ERR_RESOURCE_BUSY);
      }
      else
      {
			DebugPrintf(m_dwIndex, 5, "CommSession::recvFile(): Writing to local file \"%s\"", szFullPath);
         m_hCurrFile = _topen(szFullPath, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, 0600);
         if (m_hCurrFile == -1)
         {
				DebugPrintf(m_dwIndex, 2, "CommSession::recvFile(): Error opening file \"%s\" for writing (%s)", szFullPath, strerror(errno));
            pMsg->SetVariable(VID_RCC, ERR_IO_FAILURE);
         }
         else
         {
            m_dwFileRqId = pRequest->GetId();
            pMsg->SetVariable(VID_RCC, ERR_SUCCESS);
         }
      }
   }
   else
   {
      pMsg->SetVariable(VID_RCC, ERR_ACCESS_DENIED);
   }
}


//
// Send file to server
//

static void SendFileProgressCallback(INT64 bytesTransferred, void *cbArg)
{
	((CommSession *)cbArg)->updateTimeStamp();
}

bool CommSession::sendFile(DWORD requestId, const TCHAR *file, long offset)
{
	return SendFileOverNXCP(m_hSocket, requestId, file, m_pCtx, offset, SendFileProgressCallback, this) ? true : false;
}


//
// Upgrade agent from package in the file store
//

DWORD CommSession::upgrade(CSCPMessage *pRequest)
{
   if (m_bMasterServer)
   {
      TCHAR szPkgName[MAX_PATH], szFullPath[MAX_PATH];

      szPkgName[0] = 0;
      pRequest->GetVariableStr(VID_FILE_NAME, szPkgName, MAX_PATH);
      BuildFullPath(szPkgName, szFullPath);
      return UpgradeAgent(szFullPath);
   }
   else
   {
      return ERR_ACCESS_DENIED;
   }
}


//
// Get agent's configuration file
//

void CommSession::getConfig(CSCPMessage *pMsg)
{
   if (m_bMasterServer)
   {
      pMsg->SetVariable(VID_RCC, 
         pMsg->SetVariableFromFile(VID_CONFIG_FILE, g_szConfigFile) ? ERR_SUCCESS : ERR_IO_FAILURE);
   }
   else
   {
      pMsg->SetVariable(VID_RCC, ERR_ACCESS_DENIED);
   }
}


//
// Update agent's configuration file
//

void CommSession::updateConfig(CSCPMessage *pRequest, CSCPMessage *pMsg)
{
   if (m_bMasterServer)
   {
      BYTE *pConfig;
      int hFile;
      DWORD dwSize;

      if (pRequest->IsVariableExist(VID_CONFIG_FILE))
      {
         dwSize = pRequest->GetVariableBinary(VID_CONFIG_FILE, NULL, 0);
         pConfig = (BYTE *)malloc(dwSize);
         pRequest->GetVariableBinary(VID_CONFIG_FILE, pConfig, dwSize);
         hFile = _topen(g_szConfigFile, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, 0644);
         if (hFile != -1)
         {
#if !defined(_WIN32) && !defined(_NETWARE)
            if (dwSize > 0)
            {
               for(DWORD i = 0; i < dwSize - 1; i++)
                  if ((pConfig[i] == 0x0D) && (pConfig[i + 1] == 0x0A))
                  {
                     dwSize--;
                     memmove(&pConfig[i], &pConfig[i + 1], dwSize - i);
                  }
            }
#endif
            write(hFile, pConfig, dwSize);
            close(hFile);
            pMsg->SetVariable(VID_RCC, ERR_SUCCESS);
         }
         else
         {
				DebugPrintf(m_dwIndex, 2, "CommSession::updateConfig(): Error opening file \"%s\" for writing (%s)",
                        g_szConfigFile, strerror(errno));
            pMsg->SetVariable(VID_RCC, ERR_FILE_OPEN_ERROR);
         }
         free(pConfig);
      }
      else
      {
         pMsg->SetVariable(VID_RCC, ERR_MALFORMED_COMMAND);
      }
   }
   else
   {
      pMsg->SetVariable(VID_RCC, ERR_ACCESS_DENIED);
   }
}


//
// Setup proxy connection
//

DWORD CommSession::setupProxyConnection(CSCPMessage *pRequest)
{
   DWORD dwResult, dwAddr;
   WORD wPort;
   struct sockaddr_in sa;
   CSCP_ENCRYPTION_CONTEXT *pSavedCtx;
   TCHAR szBuffer[32];

   if (m_bMasterServer && (g_dwFlags & AF_ENABLE_PROXY))
   {
      dwAddr = pRequest->GetVariableLong(VID_IP_ADDRESS);
      wPort = pRequest->GetVariableShort(VID_AGENT_PORT);
      m_hProxySocket = socket(AF_INET, SOCK_STREAM, 0);
      if (m_hProxySocket != -1)
      {
         // Fill in address structure
         memset(&sa, 0, sizeof(sa));
         sa.sin_addr.s_addr = htonl(dwAddr);
         sa.sin_family = AF_INET;
         sa.sin_port = htons(wPort);
         if (connect(m_hProxySocket, (struct sockaddr *)&sa, sizeof(sa)) != -1)
         {
            CSCPMessage msg;
            CSCP_MESSAGE *pRawMsg;

            // Stop writing thread
            m_pSendQueue->Put(INVALID_POINTER_VALUE);

            // Wait while all queued messages will be sent
            while(m_pSendQueue->Size() > 0)
               ThreadSleepMs(100);

            // Finish proxy connection setup
            pSavedCtx = m_pCtx;
            m_pCtx = PROXY_ENCRYPTION_CTX;
            m_bProxyConnection = TRUE;
            dwResult = ERR_SUCCESS;
            m_hProxyReadThread = ThreadCreateEx(proxyReadThreadStarter, 0, this);

            // Send confirmation message
            // We cannot use sendMessage() and writing thread, because
            // encryption context already overriden, and writing thread
            // already stopped
            msg.SetCode(CMD_REQUEST_COMPLETED);
            msg.SetId(pRequest->GetId());
            msg.SetVariable(VID_RCC, RCC_SUCCESS);
            pRawMsg = msg.CreateMessage();
            sendRawMessage(pRawMsg, pSavedCtx);
            DestroyEncryptionContext(pSavedCtx);

            DebugPrintf(m_dwIndex, 5, "Established proxy connection to %s:%d", IpToStr(dwAddr, szBuffer), wPort);
         }
         else
         {
            dwResult = ERR_CONNECT_FAILED;
         }
      }
      else
      {
         dwResult = ERR_SOCKET_ERROR;
      }
   }
   else
   {
      dwResult = ERR_ACCESS_DENIED;
   }
   return dwResult;
}


//
// Proxy reading thread
//

void CommSession::proxyReadThread(void)
{
   fd_set rdfs;
   struct timeval tv;
   char pBuffer[8192];
   int nRet;

   while(1)
   {
      FD_ZERO(&rdfs);
      FD_SET(m_hProxySocket, &rdfs);
      tv.tv_sec = 0;
      tv.tv_usec = 5000000;   // Half-second timeout
      nRet = select(SELECT_NFDS(m_hProxySocket + 1), &rdfs, NULL, NULL, &tv);
      if (nRet < 0)
         break;
      if (nRet > 0)
      {
         nRet = recv(m_hProxySocket, pBuffer, 8192, 0);
         if (nRet <= 0)
            break;
         SendEx(m_hSocket, pBuffer, nRet, 0);
      }
   }
   disconnect();
}
