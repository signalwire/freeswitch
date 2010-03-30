// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_error.cc
// *
// * Purpose: Error codes and error handling functions
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 11.5.1999
// *************************************************************************

#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <gsmlib/gsm_nls.h>
#include <gsmlib/gsm_error.h>
#include <gsmlib/gsm_util.h>
#include <strstream>

using namespace std;
using namespace gsmlib;

string gsmlib::getMEErrorText(const int errorCode) throw(GsmException)
{
  switch (errorCode)
  {
  case ME_PHONE_FAILURE:
    return _("phone failure");
    break;
  case ME_NO_CONNECTION_TO_PHONE:
    return _("no connection to phone");
    break;
  case ME_PHONE_ADAPTOR_LINK_RESERVED:
    return _("phone adaptor link reserved");
    break;
  case ME_OPERATION_NOT_ALLOWED:
    return _("operation not allowed");
    break;
  case ME_OPERATION_NOT_SUPPORTED:
    return _("operation not supported");
    break;
  case ME_PH_SIM_PIN_REQUIRED:
    return _("ph SIM PIN required");
    break;
  case ME_SIM_NOT_INSERTED:
    return _("SIM not inserted");
    break;
  case ME_SIM_PIN_REQUIRED:
    return _("SIM PIN required");
    break;
  case ME_SIM_PUK_REQUIRED:
    return _("SIM PUK required");
    break;
  case ME_SIM_FAILURE:
    return _("SIM failure");
    break;
  case ME_SIM_BUSY:
    return _("SIM busy");
    break;
  case ME_SIM_WRONG:
    return _("SIM wrong");
    break;
  case ME_INCORRECT_PASSWORD:
    return _("incorrect password");
    break;
  case ME_SIM_PIN2_REQUIRED:
    return _("SIM PIN2 required");
    break;
  case ME_SIM_PUK2_REQUIRED:
    return _("SIM PUK2 required");
    break;
  case ME_MEMORY_FULL:
    return _("memory full");
    break;
  case ME_INVALID_INDEX:
    return _("invalid index");
    break;
  case ME_NOT_FOUND:
    return _("not found");
    break;
  case ME_MEMORY_FAILURE:
    return _("memory failure");
    break;
  case ME_TEXT_STRING_TOO_LONG:
    return _("text string too long");
    break;
  case ME_INVALID_CHARACTERS_IN_TEXT_STRING:
    return _("invalid characters in text string");
    break;
  case ME_DIAL_STRING_TOO_LONG:
    return _("dial string too long");
    break;
  case ME_INVALID_CHARACTERS_IN_DIAL_STRING:
    return _("invalid characters in dial string");
    break;
  case ME_NO_NETWORK_SERVICE:
    return _("no network service");
    break;
  case ME_NETWORK_TIMEOUT:
    return _("network timeout");
    break;
  case ME_UNKNOWN:
    return _("unknown");
    break;
  default:
    throw GsmException(stringPrintf(_("invalid ME error %d"), errorCode),
                       OtherError);
  }
}

string gsmlib::getSMSErrorText(const int errorCode) throw(GsmException)
{
  switch (errorCode)
  {
  case SMS_UNASSIGNED_OR_UNALLOCATED_NUMBER:
    return _("Unassigned (unallocated) number");
    break;
  case SMS_OPERATOR_DETERMINED_BARRING:
    return _("Operator determined barring");
    break;
  case SMS_CALL_BARRED:
    return _("Call barred");
    break;
  case SMS_NETWORK_FAILURE:
    return _("Network failure");
    break;
  case SMS_SHORT_MESSAGE_TRANSFER_REJECTED:
    return _("Short message transfer rejected");
    break;
  case SMS_CONGESTION:
  case SMS_CONGESTION2:
    return _("Congestion");
    break;
  case SMS_DESTINATION_OUT_OF_SERVICE:
    return _("Destination out of service");
    break;
  case SMS_UNIDENTIFIED_SUBSCRIBER:
    return _("Unidentified subscriber");
    break;
  case SMS_FACILITY_REJECTED:
    return _("Facility rejected");
    break;
  case SMS_UNKNOWN_SUBSCRIBER:
    return _("Unknown subscriber");
    break;
  case SMS_NETWORK_OUT_OF_ORDER:
    return _("Network out of order");
    break;
  case SMS_TEMPORARY_FAILURE:
    return _("Temporary failure");
    break;
  case SMS_RESOURCES_UNAVAILABLE_UNSPECIFIED:
    return _("Resources unavailable, unspecified");
    break;
  case SMS_REQUESTED_FACILITY_NOT_SUBSCRIBED:
    return _("Requested facility not subscribed");
    break;
  case SMS_REQUESTED_FACILITY_NOT_IMPLEMENTED:
    return _("Requested facility not implemented");
    break;
  case SMS_INVALID_TRANSACTION_IDENTIFIER:
    return _("Invalid Transaction Identifier");
    break;
  case SMS_SEMANTICALLY_INCORRECT_MESSAGE:
    return _("Semantically incorrect message");
    break;
  case SMS_INVALID_MANDATORY_INFORMATION:
    return _("Invalid mandatory information");
    break;
  case SMS_MESSAGE_TYPE_NONEXISTENT_OR_NOT_IMPLEMENTED:
    return _("Message type non-existent or not implemented");
    break;
  case SMS_MESSAGE_NOT_COMPATIBLE_WITH_SHORT_MESSAGE_PROTOCOL_STATE:
    return _("Message not compatible with short message protocol state");
    break;
  case SMS_INFORMATION_ELEMENT_NONEXISTENT_OR_NOT_IMPLEMENTED:
    return _("Information element non-existent or not implemented");
    break;
  case SMS_UNSPECIFIED_PROTOCOL_ERROR:
    return _("Protocol error, unspecified");
    break;
  case SMS_UNSPECIFIED_INTERWORKING_ERROR:
    return _("Interworking, unspecified");
    break;
  case SMS_TELEMATIC_INTERWORKING_NOT_SUPPORTED:
    return _("Telematic interworking not supported");
    break;
  case SMS_SHORT_MESSAGE_TYPE_0_NOT_SUPPORTED:
    return _("Short message Type 0 not supported");
    break;
  case SMS_CANNOT_REPLACE_SHORT_MESSAGE:
    return _("Cannot replace short message");
    break;
  case SMS_UNSPECIFIED_TP_PID_ERROR:
    return _("Unspecified TP-PID error");
    break;
  case SMS_DATA_CODING_SCHEME_NOT_SUPPORTED:
    return _("Data coding scheme (alphabet) not supported");
    break;
  case SMS_MESSAGE_CLASS_NOT_SUPPORTED:
    return _("Message class not supported");
    break;
  case SMS_UNSPECIFIEC_TP_DCS_ERROR:
    return _("Unspecifiec TP-DCS error");
    break;
  case SMS_COMMAND_CANNOT_BE_ACTIONED:
    return _("Command cannot be actioned");
    break;
  case SMS_COMMAND_UNSUPPORTED:
    return _("Command unsupported");
    break;
  case SMS_UNSPECIFIED_TP_COMMAND_ERROR:
    return _("Unspecified TP-Command error");
    break;
  case SMS_TPDU_NOT_SUPPORTED:
    return _("TPDU not supported");
    break;
  case SMS_SC_BUSY:
    return _("SC busy");
    break;
  case SMS_NO_SC_SUBSCRIPTION:
    return _("No SC subscription");
    break;
  case SMS_SC_SYSTEM_FAILURE:
    return _("SC system failure");
    break;
  case SMS_INVALID_SME_ADDRESS:
    return _("Invalid SME address");
    break;
  case SMS_DESTINATION_SME_BARRED:
    return _("Destination SME barred");
    break;
  case SMS_SM_REJECTED_DUPLICATED_SM:
    return _("SM Rejected-Duplicated SM");
    break;
  case SMS_SIM_SMS_STORAGE_FULL:
    return _("SIM SMS storage full");
    break;
  case SMS_NO_SMS_STORAGE_CAPABILITY_IN_SIM:
    return _("No SMS storage capability in SIM");
    break;
  case SMS_ERROR_IN_MS:
    return _("Error in MS");
    break;
  case SMS_MEMORY_CAPACITY_EXCEED:
    return _("Memory Capacity Exceed");
    break;
  case SMS_UNSPECIFIED_ERROR_CAUSE:
    return _("Unspecified error cause");
    break;
  case SMS_ME_FAILURE:
    return _("ME failure");
    break;
  case SMS_SMS_SERVICE_OF_ME_RESERVED:
    return _("SMS service of ME reserved");
    break;
  case SMS_OPERATION_NOT_ALLOWED:
    return _("operation not allowed");
    break;
  case SMS_OPERATION_NOT_SUPPORTED:
    return _("operation not supported");
    break;
  case SMS_INVALID_PDU_MODE_PARAMETER:
    return _("invalid PDU mode parameter");
    break;
  case SMS_INVALID_TEXT_MODE_PARAMETER:
    return _("invalid text mode parameter");
    break;
  case SMS_SIM_NOT_INSERTED:
    return _("SIM not inserted");
    break;
  case SMS_SIM_PIN_REQUIRED:
    return _("SIM PIN required");
    break;
  case SMS_PH_SIM_PIN_REQUIRED:
    return _("PH-SIM PIN required");
    break;
  case SMS_SIM_FAILURE:
    return _("SIM failure");
    break;
  case SMS_SIM_BUSY:
    return _("SIM busy");
    break;
  case SMS_SIM_WRONG:
    return _("SIM wrong");
    break;
  case SMS_SIM_PUK_REQUIRED:
    return _("SIM PUK required");
    break;
  case SMS_SIM_PIN2_REQUIRED:
    return _("SIM PIN2 required");
    break;
  case SMS_SIM_PUK2_REQUIRED:
    return _("SIM PUK2 required");
    break;
  case SMS_MEMORY_FAILURE:
    return _("memory failure");
    break;
  case SMS_INVALID_MEMORY_INDEX:
    return _("invalid memory index");
    break;
  case SMS_MEMORY_FULL:
    return _("memory full");
    break;
  case SMS_SMSC_ADDRESS_UNKNOWN:
    return _("SMSC address unknown");
    break;
  case SMS_NO_NETWORK_SERVICE:
    return _("no network service");
    break;
  case SMS_NETWORK_TIMEOUT:
    return _("network timeout");
    break;
  case SMS_NO_CNMA_ACKNOWLEDGEMENT_EXPECTED:
    return _("no +CNMA acknowledgement expected");
    break;
  case SMS_UNKNOWN_ERROR:
    return _("unknown error");
    break;
  default:
    throw GsmException(stringPrintf(_("invalid SMS error %d"), errorCode),
                       OtherError);
  }
}

string gsmlib::getSMSStatusString(unsigned char status)
{
  string result;
  if (status < SMS_STATUS_TEMPORARY_BIT)
  {
    switch (status)
    {
    case SMS_STATUS_RECEIVED:
      result = _("Short message received by the SME");
      break;
    case SMS_STATUS_FORWARDED:
      result = _("Short message forwarded by the SC to the SME but the SC "
                 "is unable to confirm delivery");
      break;
    case SMS_STATUS_SM_REPLACES:
      result = _("Short message replaced by the SC");
      break;
    default:
      result = _("reserved");
      break;
    }
    return result;
  }
  else if (status & SMS_STATUS_TEMPORARY_BIT)
  {
    switch (status & ~(SMS_STATUS_TEMPORARY_BIT | SMS_STATUS_PERMANENT_BIT))
    {
    case SMS_STATUS_CONGESTION:
      result = _("Congestion");
      break;
    case SMS_STATUS_SME_BUSY:
      result = _("SME busy");
      break;
    case SMS_STATUS_NO_RESPONSE_FROM_SME:
      result = _("No response from SME");
      break;
    case SMS_STATUS_SERVICE_REJECTED:
      result = _("Service rejected");
      break;
    case SMS_STATUS_QUALITY_OF_SERVICE_UNAVAILABLE:
      result = _("Quality of service not available");
      break;
    case SMS_STATUS_ERROR_IN_SME:
      result = _("Error in SME");
      break;
    default:
      result = _("reserved");
      break;
    }
    if (status & SMS_STATUS_PERMANENT_BIT)
      return result + _(" (Temporary error, SC is not making any "
                        "more transfer attempts)");
    else
      return result + _(" (Temporary error, SC still trying to "
                        "transfer SM)");
  }
  else
  {
    switch (status & ~SMS_STATUS_PERMANENT_BIT)
    {
    case SMS_STATUS_REMOTE_PROCECURE_ERROR:
      result = _("Remote Procedure Error");
      break;
    case SMS_STATUS_INCOMPATIBLE_DESTINATION:
      result = _("Incompatible destination");
      break;
    case SMS_STATUS_CONNECTION_REJECTED_BY_SME:
      result = _("Connection rejected by SME");
      break;
    case SMS_STATUS_NOT_OBTAINABLE:
      result = _("Not obtainable");
      break;
    case SMS_STATUS_QUALITY_OF_SERVICE_UNAVAILABLE:
      result = _("Quality of service not available");
      break;
    case SMS_STATUS_NO_INTERWORKING_AVAILABLE:
      result = _("No interworking available");
      break;
    case SMS_STATUS_SM_VALIDITY_PERDIOD_EXPIRED:
      result = _("SM validity period expired");
      break;
    case SMS_STATUS_SM_DELETED_BY_ORIGINATING_SME:
      result = _("SM deleted by originating SME");
      break;
    case SMS_STATUS_SM_DELETED_BY_ADMINISTRATION:
      result = _("SM deleted by SC administration");
      break;
    case SMS_STATUS_SM_DOES_NOT_EXIST:
      result = _("SM does not exit");
      break;
    default:
      result = _("reserved");
      break;
    }
    return result + _(" (Permanent Error, SC is not making any "
                      "more transfer attempts)");
  }
}
