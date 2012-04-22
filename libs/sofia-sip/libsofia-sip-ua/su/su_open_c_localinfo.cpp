/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

/**@ingroup su_open_c_localinfo.cpp
 *
 * @CFILE su_open_c_localinfo.cpp
 * Functionality for choosing an access point for sockets on Symbian.
 *
 * @author Martti Mela <Martti.Mela@nokia.com>
 * @date Created: Fri May 18 14:31:41 2007 mela
 *
 */

#include "config.h"

#include <unistd.h>
#include <in_sock.h>
#include <es_sock.h>
#include <e32base.h>
#include <s32mem.h>
#include <s32strm.h>
#include <commdbconnpref.h>

#include <sofia-sip/su.h>


su_sockaddr_t sa_global[1];

/* Copy IP address for the sockaddr structure.
 *
 * @param su pointer to allocated su_sockaddr_t structure
 *
 * @return 0 if successful.
 */
extern "C" int su_get_local_ip_addr(su_sockaddr_t *su)
{
	su->su_sin.sin_addr.s_addr = sa_global->su_sin.sin_addr.s_addr;
	su->su_family = sa_global->su_family;
	su->su_len = sa_global->su_len;

	return 0;
}


/* Set up the access point for the stack. Code adapted from Forum Nokia,
 * http://wiki.forum.nokia.com/index.php/LocalDeviceIpAddress.
 *
 * @param su su_sockaddr_t structure
 * @param ifindex pointer to interface index
 *
 * @return Connection object
 */
extern "C" void *su_localinfo_ap_set(su_sockaddr_t *su, int *ifindex)
{
  TCommDbConnPref iPref;
  RSocketServ aSocketServ;
  RSocket sock;

  /* Get the IAP id of the underlying interface of this RConnection */
  TUint32 iapId;

  iPref.SetDirection(ECommDbConnectionDirectionOutgoing);
  iPref.SetDialogPreference(ECommDbDialogPrefPrompt);
  iPref.SetBearerSet(KCommDbBearerUnknown /*PSD*/);

  aSocketServ = RSocketServ();
  aSocketServ.Connect();
  RConnection *aConnection = new RConnection();
  aConnection->Open(aSocketServ);
  aConnection->Start(iPref);

  User::LeaveIfError(sock.Open(aSocketServ, KAfInet, KSockStream,
			       KProtocolInetTcp));

  User::LeaveIfError(aConnection->GetIntSetting(_L("IAP\\Id"), iapId));

  /* Get IP information from the socket */
  TSoInetInterfaceInfo ifinfo;
  TPckg<TSoInetInterfaceInfo> ifinfopkg(ifinfo);

  TSoInetIfQuery ifquery;
  TPckg<TSoInetIfQuery> ifquerypkg(ifquery);

  /* To find out which interfaces are using our current IAP, we
   * must enumerate and go through all of them and make a query
   * by name for each. */
  User::LeaveIfError(sock.SetOpt(KSoInetEnumInterfaces, KSolInetIfCtrl));
  while(sock.GetOpt(KSoInetNextInterface, KSolInetIfCtrl, ifinfopkg) == KErrNone) {
    ifquery.iName = ifinfo.iName;
    TInt err = sock.GetOpt(KSoInetIfQueryByName, KSolInetIfQuery, ifquerypkg);

    /* IAP ID is index 1 of iZone */
    if(err == KErrNone && ifquery.iZone[1] == iapId) {
      /* We have found an interface using the IAP we are interested in. */
      if(ifinfo.iAddress.Address() > 0) {
	/* found a IPv4 address */
	su->su_sin.sin_addr.s_addr = htonl(ifinfo.iAddress.Address());
	sa_global->su_sin.sin_addr.s_addr = su->su_sin.sin_addr.s_addr;
	sa_global->su_family = su->su_family = AF_INET;
	sa_global->su_len = su->su_len = 28;
	*ifindex = iapId;
	return (void *) aConnection;
      }
    }
    else if(err != KErrNone)
      break;
  }

  sock.Close();
  return (void *) aConnection;
}


/* Deinitialize the access point in use.
 *
 * @param aconn Pointer to connection object
 *
 * @return 0 if successful.
 */
extern "C" int su_localinfo_ap_deinit(void *aconn)
{
  RConnection *aConnection = (RConnection *) aconn;
  aConnection->Stop();
  aConnection->Close();
  delete aConnection;
  return 0;
}
