// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.

#ifndef PROPERTYMAP_H_INCLUDED
#define	PROPERTYMAP_H_INCLUDED

namespace Telnyx {

struct PropertyMap
{
  #define PROP_MAP_STRINGS \
  { \
    "source-address", \
    "source-port", \
    "source-transport", \
    "target-address", \
    "target-port", \
    "target-transport", \
    "target-host", \
    "target-identity", \
    "local-address", \
    "transport-id", \
    "session-id", \
    "xor", \
    "peer-xor", \
    "auth-method", \
    "auth-action", \
    "auth-response", \
    "route-action", \
    "reject-code", \
    "reject-reason", \
    "interface-address", \
    "interface-port", \
    "transaction-timeout", \
    "reliable-transport-transaction-timeout", \
    "rtp-resizer-samples", \
    "max-channel", \
    "response-target", \
    "response-interface", \
    "response-sdp", \
    "no-rtp-proxy", \
    "require-rtp-proxy", \
    "enable-verbose-rtp", \
    "leg1-contact", \
    "leg2-contact", \
    "leg1-rr", \
    "leg2-rr", \
    "leg-identifier", \
    "leg-index", \
    "invoke-local-handler", \
    "disable-nat-compensation", \
    "respond-to-packet-source", \
    "sdp-answer-route", \
    "reinvite", \
    "generate-local-response", \
    "inbound-contact", \
    "is-early-dialog-persisted", \
    "is-out-of-dialog-refer", \
    "subnets", \
    "x-local-reg", \
    "is-locally-authenticated", \
    "endpoint-name", \
    "transport-alias", \
    "reg-id", \
    "require-persistent-connection", \
    "disallow-forward-response", \
    "property-undefined" \
  };

  enum Enum
  {
    PROP_SourceAddress,
    PROP_SourcePort,
    PROP_SourceTransport,
    PROP_TargetAddress,
    PROP_TargetPort,
    PROP_TargetTransport,
    PROP_TargetHost,
    PROP_TargetIdentity,
    PROP_LocalAddress,
    PROP_TransportId,
    PROP_SessionId,
    PROP_XOR,
    PROP_PeerXOR,
    PROP_AuthMethod,
    PROP_AuthAction,
    PROP_AuthResponse,
    PROP_RouteAction,
    PROP_RejectCode,
    PROP_RejectReason,
    PROP_InterfaceAddress,
    PROP_InterfacePort,
    PROP_TransactionTimeout,
    PROP_ReliableTransportTransactionTimeout,
    PROP_RTPResizerSamples,
    PROP_MaxChannel,
    PROP_ResponseTarget,
    PROP_ResponseInterface,
    PROP_ResponseSDP,
    PROP_NoRTPProxy,
    PROP_RequireRTPProxy,
    PROP_EnableVerboseRTP,
    PROP_Leg1Contact,
    PROP_Leg2Contact,
    PROP_Leg1RR,
    PROP_Leg2RR,
    PROP_LegIdentifier,
    PROP_LegIndex,
    PROP_InvokeLocalHandler,
    PROP_DisableNATCompensation,
    PROP_RespondToPacketSource,
    PROP_SDPAnswerRoute,
    PROP_IsReinvite,
    PROP_GenerateLocalResponse,
    PROP_InboundContact,
    PROP_IsEarlyDialogPersisted,
    PROP_IsOutOfDialogRefer,
    PROP_Subnets,
    PROP_LocalReg,
    PROP_IsLocallyAuthenticated,
    PROP_EndpointName,
    PROP_TransportAlias,
    PROP_RegId,
    PROP_RequirePersistentConnection,
    PROP_DisallowForwardResponse,
    PROP_Max
  };
  
  static const char* propertyString(PropertyMap::Enum prop)
    /// returns the string representation of a custom property
  {
    char* ret = 0;
    
    if (prop < PROP_Max)
    {
      static const char* prop_map[] = PROP_MAP_STRINGS;

      
      ret = (char*)prop_map[prop];
    }
    
    return ret;
  }
};

} // namespace Telnyx


#endif	// PROPERTYMAP_H_INCLUDED

