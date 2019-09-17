/* 
** NetXMS subagent for FreeBSD
** Copyright (C) 2004 Alex Kirhenshtein
** Copyright (C) 2008 Mark Ibell
** Copyright (C) 2016 Raden Solutions
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
**/

#include <nms_common.h>
#include <nms_agent.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/route.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <kvm.h>
#include <nlist.h>

#if HAVE_NET_ISO88025_H
#include <net/iso88025.h>
#endif

#include "freebsd_subagent.h"

typedef struct t_IfList
{
	char *name;
	struct ether_addr *mac;
	InetAddress *addr;
	int addrCount;
	int index;
	int type;
	int mtu;
} IFLIST;

/**
 * Handler for Net.IP.Forwarding parameter
 */
LONG H_NetIpForwarding(const TCHAR *pszParam, const TCHAR *pArg, TCHAR *value, AbstractCommSession *session)
{
	int nVer = CAST_FROM_POINTER(pArg, int);
	int nRet = SYSINFO_RC_ERROR;
	int mib[4];
	size_t nSize = sizeof(mib), nValSize;
	int nVal;

	if (nVer == 6)
	{
		return ERR_NOT_IMPLEMENTED;
	}

	if (sysctlnametomib("net.inet.ip.forwarding", mib, &nSize) != 0)
	{
		return SYSINFO_RC_ERROR;
	}

	nValSize = sizeof(nVal);
	if (sysctl(mib, nSize, &nVal, &nValSize, NULL, 0) == 0)
	{
		if (nVal == 0 || nVal == 1)
		{
			ret_int(value, nVal);
			nRet = SYSINFO_RC_SUCCESS;
		}
	}

	return nRet;
}

/**
 * Handler for Net.Interface.AdminStatus parameter
 */
LONG H_NetIfAdminStatus(const TCHAR *pszParam, const TCHAR *pArg, TCHAR *value, AbstractCommSession *session)
{
	int nRet = SYSINFO_RC_SUCCESS;
	char szArg[512];

	AgentGetParameterArgA(pszParam, 1, szArg, sizeof(szArg));

	if (szArg[0] != 0)
	{
		if (szArg[0] >= '0' && szArg[0] <= '9')
		{
			// index
			if (if_indextoname(atoi(szArg), szArg) != szArg)
			{
				// not found
				nRet = SYSINFO_RC_ERROR;
			}
		}

		if (nRet == SYSINFO_RC_SUCCESS)
		{
			int nSocket;

			nRet = SYSINFO_RC_ERROR;

			nSocket = socket(AF_INET, SOCK_DGRAM, 0);
			if (nSocket > 0)
			{
				struct ifreq ifr;
				int flags;

				memset(&ifr, 0, sizeof(ifr));
				strncpy(ifr.ifr_name, szArg, sizeof(ifr.ifr_name));
				if (ioctl(nSocket, SIOCGIFFLAGS, (caddr_t)&ifr) >= 0)
				{
					flags = (ifr.ifr_flags & 0xffff) | (ifr.ifr_flagshigh << 16);
					if ((flags & IFF_UP) == IFF_UP)
					{
						// enabled
						ret_int(value, 1);
						nRet = SYSINFO_RC_SUCCESS;
					}
					else
					{
						ret_int(value, 2);
						nRet = SYSINFO_RC_SUCCESS;
					}
				}
				close(nSocket);
			}
		}
	}

	return nRet;
}

/**
 * Handler for Net.Interface.OperStatus parameter
 */
LONG H_NetIfOperStatus(const TCHAR *pszParam, const TCHAR *pArg, TCHAR *value, AbstractCommSession *session)
{
	int nRet = SYSINFO_RC_SUCCESS;
	char szArg[512];

	AgentGetParameterArgA(pszParam, 1, szArg, sizeof(szArg));

	if (szArg[0] != 0)
	{
		if (szArg[0] >= '0' && szArg[0] <= '9')
		{
			// index
			if (if_indextoname(atoi(szArg), szArg) != szArg)
			{
				// not found
				nRet = SYSINFO_RC_ERROR;
			}
		}

		if (nRet == SYSINFO_RC_SUCCESS)
		{
			int nSocket;

			nRet = SYSINFO_RC_ERROR;

			nSocket = socket(AF_INET, SOCK_DGRAM, 0);
			if (nSocket > 0)
			{
				struct ifmediareq ifmr;

				memset(&ifmr, 0, sizeof(ifmr));
				strncpy(ifmr.ifm_name, szArg, sizeof(ifmr.ifm_name));
				if (ioctl(nSocket, SIOCGIFMEDIA, (caddr_t)&ifmr) >= 0)
				{
					if ((ifmr.ifm_status & IFM_AVALID) == IFM_AVALID &&
							(ifmr.ifm_status & IFM_ACTIVE) == IFM_ACTIVE)
					{
						ret_int(value, 1);
						nRet = SYSINFO_RC_SUCCESS;
					}
					else
					{
						ret_int(value, 0);
						nRet = SYSINFO_RC_SUCCESS;
					}
				}
            else if (errno == EINVAL || errno == ENOTTY)
            {
               // ifmedia not supported, assume the status is NORMAL
               ret_int(value, 1);
               nRet = SYSINFO_RC_SUCCESS;
            }
				close(nSocket);
			}
		}
	}

	return nRet;
}

/**
 * Handler for Net.ArpCache list
 */
LONG H_NetArpCache(const TCHAR *pszParam, const TCHAR *pArg, StringList *value, AbstractCommSession *session)
{
	int nRet = SYSINFO_RC_ERROR;
	FILE *hFile;
	int mib[6] = { CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_FLAGS, RTF_LLINFO };
	size_t nNeeded;
	char *pNext, *pBuff;
	struct rt_msghdr *pRtm = NULL;
	struct sockaddr_inarp *pSin;
	struct sockaddr_dl *pSdl;
	char *pLim;

	if (sysctl(mib, 6, NULL, &nNeeded, NULL, 0) != 0)
	{
		return SYSINFO_RC_ERROR;
	}
	if ((pBuff = (char *)malloc(nNeeded)) == NULL)
	{
		return SYSINFO_RC_ERROR;
	}
	if (sysctl(mib, 6, pBuff, &nNeeded, NULL, 0) != 0)
	{
		return SYSINFO_RC_ERROR;
	}

	nRet = SYSINFO_RC_SUCCESS;
	pLim = pBuff + nNeeded;
	for (pNext = pBuff; pNext < pLim; pNext += pRtm->rtm_msglen)
	{
		char szBuff[256];
		struct ether_addr *pEa;

		pRtm = (struct rt_msghdr *)pNext;
		pSin = (struct sockaddr_inarp *)(pRtm + 1);

		pSdl = (struct sockaddr_dl *)((char *)pSin + (pSin->sin_len > 0
					?
						(1 + ((pSin->sin_len - 1) | (sizeof(long) - 1)))
					:
						sizeof(long))
				);

		if (pSdl->sdl_alen != 6)
		{
			continue;
		}

		pEa = (struct ether_addr *)LLADDR(pSdl);

		if (memcmp(pEa->octet, "\377\377\377\377\377\377", 6) == 0)
		{
			// broadcast
			continue;
		}

		snprintf(szBuff, sizeof(szBuff),
				"%02X%02X%02X%02X%02X%02X %s %d",
				pEa->octet[0], pEa->octet[1],
				pEa->octet[2], pEa->octet[3],
				pEa->octet[4], pEa->octet[5],
				inet_ntoa(pSin->sin_addr),
				pSdl->sdl_index);
		value->addMBString(szBuff);
	}

	return nRet;
}

/**
 * Handler for Net.IP.RoutingTable list
 */
LONG H_NetRoutingTable(const TCHAR *pszParam, const TCHAR *pArg, StringList *value, AbstractCommSession *session)
{
#define sa2sin(x) ((struct sockaddr_in *)x)
#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
	int nRet = SYSINFO_RC_ERROR;
	int mib[6] = { CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_DUMP, 0 };
	char *pRT = NULL, *pNext = NULL;
	size_t nReqSize = 0;
	struct rt_msghdr *rtm;
	struct sockaddr *sa;
	struct sockaddr *rti_info[RTAX_MAX];

	if (sysctl(mib, 6, NULL, &nReqSize, NULL, 0) == 0)
	{
		if (nReqSize > 0)
		{
			pRT = (char *)malloc(nReqSize);
			if (pRT != NULL)
			{
				if (sysctl(mib, 6, pRT, &nReqSize, NULL, 0) < 0)
				{
					free(pRT);
					pRT = NULL;
				}
			}
		}
	}

	if (pRT != NULL)
	{
		nRet = SYSINFO_RC_SUCCESS;

		for (pNext = pRT; pNext < pRT + nReqSize; pNext += rtm->rtm_msglen)
		{
			rtm = (struct rt_msghdr *)pNext;
			sa = (struct sockaddr *)(rtm + 1);

			if (sa->sa_family != AF_INET)
			{
				continue;
			}

			for (int i = 0; i < RTAX_MAX; i++)
			{
				if (rtm->rtm_addrs & (1 << i))
				{
					rti_info[i] = sa;
					sa = (struct sockaddr *)((char *)(sa) + ROUNDUP(sa->sa_len));
				}
				else
				{
					rti_info[i] = NULL;
				}
			}

			if (rti_info[RTAX_DST] != NULL
#if HAVE_DECL_RTF_WASCLONED
			    && !(rtm->rtm_flags & RTF_WASCLONED))
#else
                            )
#endif
			{
				char szOut[1024];
				char szTmp[64];

				if (sa2sin(rti_info[RTAX_DST])->sin_addr.s_addr == INADDR_ANY)
				{
					strcpy(szOut, "0.0.0.0/0 ");
				}
				else
				{
					if ((rtm->rtm_flags & RTF_HOST) || rti_info[RTAX_NETMASK]==NULL)
					{
						// host
						strcpy(szOut,
								inet_ntoa(sa2sin(rti_info[RTAX_DST])->sin_addr));
						strcat(szOut, "/32 ");
					}
					else
					{
						// net
						int nMask =
							rti_info[RTAX_NETMASK] ?
								ntohl(sa2sin(rti_info[RTAX_NETMASK])->sin_addr.s_addr)
								: 0;

						sprintf(szOut, "%s/%d ",
								inet_ntoa(sa2sin(rti_info[RTAX_DST])->sin_addr),
								nMask ? 33 - ffs(nMask) : 0);
					}
				}

				if (rti_info[RTAX_GATEWAY]->sa_family != AF_INET)
				{
					strcat(szOut, "0.0.0.0 ");
				}
				else
				{
					strcat(szOut,
							inet_ntoa(sa2sin(rti_info[RTAX_GATEWAY])->sin_addr));
					strcat(szOut, " ");
				}
				snprintf(szTmp, sizeof(szTmp), "%d %d",
						rtm->rtm_index,
						(rtm->rtm_flags & RTF_GATEWAY) == 0 ? 3 : 4);
				strcat(szOut, szTmp);

				value->addMBString(szOut);
			}
		}

		free(pRT);
	}

#undef ROUNDUP
#undef sa2sin

	return nRet;
}

/**
 * Dump full interface info to string list
 */
static void DumpInterfaceInfo(IFLIST *pList, int nIfCount, StringList *value)
{
   char szOut[1024], macAddr[32], ipAddrText[64];

   for(int i = 0; i < nIfCount; i++)
   {
      if (pList[i].addrCount == 0)
      {
         snprintf(szOut, sizeof(szOut), "%d 0.0.0.0/0 %d(%d) %s %s",
               pList[i].index, pList[i].type, pList[i].mtu,
               BinToStrA((BYTE *)pList[i].mac, 6, macAddr),
               pList[i].name);
         value->addMBString(szOut);
      }
      else
      {
         for(int j = 0; j < pList[i].addrCount; j++)
         {
            snprintf(szOut, sizeof(szOut), "%d %s/%d %d(%d) %s %s",
                  pList[i].index, pList[i].addr[j].toStringA(ipAddrText),
                  pList[i].addr[j].getMaskBits(), pList[i].type, pList[i].mtu,
                  BinToStrA((BYTE *)pList[i].mac, 6, macAddr),
                  pList[i].name);
            value->addMBString(szOut);
         }
      }
   }
}

/**
 * Dump only interface names to string list
 */
static void DumpInterfaceNames(IFLIST *pList, int nIfCount, StringList *value)
{
   for(int i = 0; i < nIfCount; i++)
   {
      value->addMBString(pList[i].name);
   }
}

/**
 * Common handler for Net.InterfaceList and Net.InterfaceNames lists
 */
static LONG GetInterfaceList(StringList *value, bool namesOnly)
{
	int nRet = SYSINFO_RC_ERROR;
	struct ifaddrs *pIfAddr, *pNext;
	if (getifaddrs(&pIfAddr) == 0)
	{
		char *pName = NULL;
		int nIndex, nMask, i;
		int nIfCount = 0;
		IFLIST *pList = NULL;

		nRet = SYSINFO_RC_SUCCESS;

		pNext = pIfAddr;
		while (pNext != NULL)
		{
			if (pName != pNext->ifa_name)
			{
				IFLIST *pTmp;

				nIfCount++;
				pTmp = (IFLIST *)MemRealloc(pList, nIfCount * sizeof(IFLIST));
				if (pTmp == NULL)
				{
					// out of memoty
					nIfCount--;
					nRet = SYSINFO_RC_ERROR;
					break;
				}
				pList = pTmp;

				memset(&(pList[nIfCount - 1]), 0, sizeof(IFLIST));
				pList[nIfCount - 1].name = pNext->ifa_name;
				pName = pNext->ifa_name;
			}

			switch(pNext->ifa_addr->sa_family)
			{
			case AF_INET:
			case AF_INET6:
				{
					InetAddress *pTmp;
					pList[nIfCount - 1].addrCount++;
					pTmp = (InetAddress *)MemRealloc(pList[nIfCount - 1].addr,
							pList[nIfCount - 1].addrCount * sizeof(InetAddress));
					if (pTmp == NULL)
					{
						pList[nIfCount-1].addrCount--;
						nRet = SYSINFO_RC_ERROR;
						break;
					}
					pList[nIfCount - 1].addr = pTmp;
               if (pNext->ifa_addr->sa_family == AF_INET)
               {
                  UINT32 addr = htonl(reinterpret_cast<struct sockaddr_in*>(pNext->ifa_addr)->sin_addr.s_addr);
                  UINT32 mask = htonl(reinterpret_cast<struct sockaddr_in*>(pNext->ifa_netmask)->sin_addr.s_addr);
					   pList[nIfCount - 1].addr[pList[nIfCount - 1].addrCount - 1] = InetAddress(addr, mask);
               }
               else
               {
                  pList[nIfCount - 1].addr[pList[nIfCount - 1].addrCount - 1] =
                        InetAddress(reinterpret_cast<struct sockaddr_in6*>(pNext->ifa_addr)->sin6_addr.s6_addr,
                              BitsInMask(reinterpret_cast<struct sockaddr_in6*>(pNext->ifa_netmask)->sin6_addr.s6_addr, 16));
               }
				}
				break;
			case AF_LINK:
				{
					struct sockaddr_dl *pSdl;

					pSdl = (struct sockaddr_dl *)pNext->ifa_addr;
					pList[nIfCount - 1].mac = (struct ether_addr *)LLADDR(pSdl);
					pList[nIfCount - 1].index = pSdl->sdl_index;
					pList[nIfCount - 1].type = static_cast<struct if_data*>(pNext->ifa_data)->ifi_type;
					pList[nIfCount - 1].mtu = static_cast<struct if_data*>(pNext->ifa_data)->ifi_mtu;
				}
				break;
			}

			if (nRet == SYSINFO_RC_ERROR)
			{
				break;
			}
			pNext = pNext->ifa_next;
		}

		if (nRet == SYSINFO_RC_SUCCESS)
		{
         if (namesOnly)
            DumpInterfaceNames(pList, nIfCount, value);
         else
            DumpInterfaceInfo(pList, nIfCount, value);
		}

		for (i = 0; i < nIfCount; i++)
		{
			if (pList[i].addr != NULL)
			{
				MemFree(pList[i].addr);
				pList[i].addr = NULL;
				pList[i].addrCount = 0;
			}
		}
		if (pList != NULL)
		{
			MemFree(pList);
			pList = NULL;
		}

		freeifaddrs(pIfAddr);
	}
	else
	{
		nxlog_debug_tag(SUBAGENT_DEBUG_TAG, 5, _T("Call to getifaddrs() failed (%s)"), _tcserror(errno));
	}
	return nRet;
}

/**
 * Handler for Net.InterfaceList list
 */
LONG H_NetIfList(const TCHAR *pszParam, const TCHAR *pArg, StringList *value, AbstractCommSession *session)
{
   return GetInterfaceList(value, false);
}

/**
 * Handler for Net.InterfaceNames list
 */
LONG H_NetIfNames(const TCHAR *pszParam, const TCHAR *pArg, StringList *value, AbstractCommSession *session)
{
   return GetInterfaceList(value, true);
}

/**
 * KVM name list
 */
struct nlist s_nl[] = 
{
	{ (char *)"_ifnet" },
	{ NULL }
};

/**
 * KVM handle
 */
static kvm_t *s_kvmd = NULL;

/**
 * KVM lock
 */
 static Mutex s_kvmLock;

#if __FreeBSD__ >= 10

/**
 * Read kernel counter
 */
inline UINT64 ReadKernelCounter64(counter_u64_t cnt)
{
	UINT64 value;
	if (kvm_read(s_kvmd, (u_long)cnt, &value, sizeof(UINT64)) != sizeof(UINT64))
	{
		nxlog_debug(7, _T("ReadKernelCounter64: kvm_read failed (%hs)"), kvm_geterr(s_kvmd));
	   return 0;
	}
   return value;
}

#endif

/**
 * Handler for interface statistics parameters
 */
LONG H_NetIfInfoFromKVM(const TCHAR *param, const TCHAR *arg, TCHAR *value, AbstractCommSession *session)
{
	char ifName[256];
	if (!AgentGetParameterArgA(param, 1, ifName, sizeof(ifName)))
		return SYSINFO_RC_UNSUPPORTED;

	if (ifName[0] == 0)		
		return SYSINFO_RC_UNSUPPORTED;

	if ((ifName[0] >= '0') && (ifName[0] <= '9'))
	{
		int ifIndex = atoi(ifName);
		if (if_indextoname(ifIndex, ifName) != ifName)
		{
			nxlog_debug(7, _T("H_NetIfInfoFromKVM: cannot find interface name for index %d"), ifIndex);
			return SYSINFO_RC_UNSUPPORTED;
		}
	}

	s_kvmLock.lock();
	if (s_kvmd == NULL)
	{
		char errmsg[_POSIX2_LINE_MAX];
		s_kvmd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, errmsg);
		if (s_kvmd == NULL)
		{
			nxlog_debug(7, _T("H_NetIfInfoFromKVM: kvm_openfiles failed (%hs)"), errmsg);
			s_kvmLock.unlock();
			return SYSINFO_RC_ERROR;
		}
		if (kvm_nlist(s_kvmd, s_nl) < 0)
		{
			nxlog_debug(7, _T("H_NetIfInfoFromKVM: kvm_nlist failed (%hs)"), kvm_geterr(s_kvmd));
			kvm_close(s_kvmd);
			s_kvmd = NULL;
			s_kvmLock.unlock();
			return SYSINFO_RC_UNSUPPORTED;
		}
		if (s_nl[0].n_type == 0)
		{
			nxlog_debug(7, _T("H_NetIfInfoFromKVM: symbol %hs not found in kernel symbol table"), s_nl[0].n_name);
			kvm_close(s_kvmd);
			s_kvmd = NULL;
			s_kvmLock.unlock();
			return SYSINFO_RC_UNSUPPORTED;
		}
	}

	int rc = SYSINFO_RC_UNSUPPORTED;

	u_long curr = s_nl[0].n_value;
	struct ifnethead head;
	if (kvm_read(s_kvmd, curr, &head, sizeof(head)) != sizeof(head))
	{
		nxlog_debug(7, _T("H_NetIfInfoFromKVM: kvm_read failed (%hs)"), kvm_geterr(s_kvmd));
		s_kvmLock.unlock();
		return SYSINFO_RC_ERROR;
	}
#if __FreeBSD__ >= 12
	curr = (u_long)STAILQ_FIRST(&head);
#else
	curr = (u_long)TAILQ_FIRST(&head);
#endif
	while(curr != 0)
	{
		struct ifnet ifnet;
		if (kvm_read(s_kvmd, curr, &ifnet, sizeof(ifnet)) != sizeof(ifnet))
		{
			nxlog_debug(7, _T("H_NetIfInfoFromKVM: kvm_read failed (%hs)"), kvm_geterr(s_kvmd));
			rc = SYSINFO_RC_ERROR;
			break;
		}
#if __FreeBSD__ >= 12
		curr = (u_long)STAILQ_NEXT(&ifnet, if_link);
#else
		curr = (u_long)TAILQ_NEXT(&ifnet, if_link);
#endif

#if __FreeBSD__ >= 5
		const char *currName = ifnet.if_xname;
#else
		char currName[IFNAMSIZ];
		if (kvm_read(s_kvmd, ifnet.if_name, currName, sizeof(currName)) != sizeof(currName))
		{
			nxlog_debug(7, _T("H_NetIfInfoFromKVM: kvm_read failed (%hs)"), kvm_geterr(s_kvmd));
			rc = SYSINFO_RC_ERROR;
			break;
		}
		currName[sizeof(currName) - 2] = 0;
		size_t len = strlen(currName); 
		snprintf(&currName[len], sizeof(currName) - len, "%d", ifnet.if_unit);
#endif
		if (!strcmp(currName, ifName))
		{
			rc = SYSINFO_RC_SUCCESS;
#if __FreeBSD__ >= 11
			switch(CAST_FROM_POINTER(arg, int))
			{
				case IF_INFO_BYTES_IN:
					ret_uint(value, (UINT32)(ReadKernelCounter64(ifnet.if_counters[IFCOUNTER_IBYTES]) & _ULL(0xFFFFFFFF)));
					break;
				case IF_INFO_BYTES_IN_64:
					ret_uint64(value, ReadKernelCounter64(ifnet.if_counters[IFCOUNTER_IBYTES]));
					break;
				case IF_INFO_BYTES_OUT:
					ret_uint(value, (UINT32)(ReadKernelCounter64(ifnet.if_counters[IFCOUNTER_OBYTES]) & _ULL(0xFFFFFFFF)));
					break;
				case IF_INFO_BYTES_OUT_64:
					ret_uint64(value, ReadKernelCounter64(ifnet.if_counters[IFCOUNTER_OBYTES]));
					break;
				case IF_INFO_IN_ERRORS:
					ret_uint(value, (UINT32)(ReadKernelCounter64(ifnet.if_counters[IFCOUNTER_IERRORS]) & _ULL(0xFFFFFFFF)));
					break;
				case IF_INFO_IN_ERRORS_64:
					ret_uint64(value, ReadKernelCounter64(ifnet.if_counters[IFCOUNTER_IERRORS]));
					break;
				case IF_INFO_OUT_ERRORS:
					ret_uint(value, (UINT32)(ReadKernelCounter64(ifnet.if_counters[IFCOUNTER_OERRORS]) & _ULL(0xFFFFFFFF)));
					break;
				case IF_INFO_OUT_ERRORS_64:
					ret_uint64(value, ReadKernelCounter64(ifnet.if_counters[IFCOUNTER_OERRORS]));
					break;
				case IF_INFO_PACKETS_IN:
					ret_uint(value, (UINT32)(ReadKernelCounter64(ifnet.if_counters[IFCOUNTER_IPACKETS]) & _ULL(0xFFFFFFFF)));
					break;
				case IF_INFO_PACKETS_IN_64:
					ret_uint64(value, ReadKernelCounter64(ifnet.if_counters[IFCOUNTER_IPACKETS]));
					break;
				case IF_INFO_PACKETS_OUT:
					ret_uint(value, (UINT32)(ReadKernelCounter64(ifnet.if_counters[IFCOUNTER_OPACKETS]) & _ULL(0xFFFFFFFF)));
					break;
				case IF_INFO_PACKETS_OUT_64:
					ret_uint64(value, ReadKernelCounter64(ifnet.if_counters[IFCOUNTER_OPACKETS]));
					break;
				default:
					rc = SYSINFO_RC_UNSUPPORTED;
					break;
			}
#else
			switch(CAST_FROM_POINTER(arg, int))
			{
				case IF_INFO_BYTES_IN:
					ret_uint(value, ifnet.if_ibytes);
					break;
				case IF_INFO_BYTES_OUT:
					ret_uint(value, ifnet.if_obytes);
					break;
				case IF_INFO_IN_ERRORS:
					ret_uint(value, ifnet.if_ierrors);
					break;
				case IF_INFO_OUT_ERRORS:
					ret_uint(value, ifnet.if_oerrors);
					break;
				case IF_INFO_PACKETS_IN:
					ret_uint(value, ifnet.if_ipackets);
					break;
				case IF_INFO_PACKETS_OUT:
					ret_uint(value, ifnet.if_opackets);
					break;
				default:
					rc = SYSINFO_RC_UNSUPPORTED;
					break;
			}
#endif
			break;
		}
	}
	s_kvmLock.unlock();
	return rc;
}

/**
 * Handler for Net.Interface.64BitCounters
 */
LONG H_NetInterface64bitSupport(const TCHAR *param, const TCHAR *arg, TCHAR *value, AbstractCommSession *session)
{
#if __FreeBSD__ >= 10
	ret_int(value, 1);
#else
	ret_int(value, 0);
#endif	
	return SYSINFO_RC_SUCCESS;
} 
