// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_error.h
// *
// * Purpose: Error codes and error handling functions
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 4.5.1999
// *************************************************************************

#ifndef GSM_ERROR_H
#define GSM_ERROR_H

#include <string>
#include <stdexcept>

using namespace std;

namespace gsmlib
{
  // different classes of GSM errors
  enum GsmErrorClass{OSError,   // error caused by OS call (eg. file handling)
                     ParserError, // error when parsing AT response
                     ChatError, // error in chat sequence (ME/TA/SMS error)
                     ParameterError, // gsmlib function called with bad params
                     NotImplementedError, // feature not implemented
                     MeTaCapabilityError, // non-existent capability in ME
                     SMSFormatError, // SMS format error
                     InterruptException, // gsmlib was interrupted()
                     OtherError}; // all other errors

  // all gsmlib exceptions

  class GsmException : public runtime_error
  {
  private:
    GsmErrorClass _errorClass;
    int _errorCode;

  public:
    GsmException(string errorText, GsmErrorClass errorClass) :
      runtime_error(errorText), _errorClass(errorClass), _errorCode(-1) {}

    GsmException(string errorText, GsmErrorClass errorClass, int errorCode) :
      runtime_error(errorText), _errorClass(errorClass),
      _errorCode(errorCode) {}

    int getErrorCode() const {return _errorCode;}
    
    GsmErrorClass getErrorClass() const {return _errorClass;}
  };

  // error codes returned by TA/ME (+CMEE)

  const int ME_PHONE_FAILURE = 0;
  const int ME_NO_CONNECTION_TO_PHONE = 1;
  const int ME_PHONE_ADAPTOR_LINK_RESERVED = 2;
  const int ME_OPERATION_NOT_ALLOWED = 3;
  const int ME_OPERATION_NOT_SUPPORTED = 4;
  const int ME_PH_SIM_PIN_REQUIRED = 5;
  const int ME_SIM_NOT_INSERTED = 10;
  const int ME_SIM_PIN_REQUIRED = 11;
  const int ME_SIM_PUK_REQUIRED = 12;
  const int ME_SIM_FAILURE = 13;
  const int ME_SIM_BUSY = 14;
  const int ME_SIM_WRONG = 15;
  const int ME_INCORRECT_PASSWORD = 16;
  const int ME_SIM_PIN2_REQUIRED = 17;
  const int ME_SIM_PUK2_REQUIRED = 18;
  const int ME_MEMORY_FULL = 20;
  const int ME_INVALID_INDEX = 21;
  const int ME_NOT_FOUND = 22;
  const int ME_MEMORY_FAILURE = 23;
  const int ME_TEXT_STRING_TOO_LONG = 24;
  const int ME_INVALID_CHARACTERS_IN_TEXT_STRING = 25;
  const int ME_DIAL_STRING_TOO_LONG = 26;
  const int ME_INVALID_CHARACTERS_IN_DIAL_STRING = 27;
  const int ME_NO_NETWORK_SERVICE = 30;
  const int ME_NETWORK_TIMEOUT = 31;
  const int ME_UNKNOWN = 100;

  // return descriptive text for the given error code
  // the text is already translated
  extern string getMEErrorText(const int errorCode) throw(GsmException);

  // SMS error codes

  // error codes from ETSI GSM 04.11, Annex E
  const int SMS_UNASSIGNED_OR_UNALLOCATED_NUMBER = 1;
  const int SMS_OPERATOR_DETERMINED_BARRING = 8;
  const int SMS_CALL_BARRED = 10;
  const int SMS_NETWORK_FAILURE = 17;
  const int SMS_SHORT_MESSAGE_TRANSFER_REJECTED = 21;
  const int SMS_CONGESTION = 22;
  const int SMS_DESTINATION_OUT_OF_SERVICE = 27;
  const int SMS_UNIDENTIFIED_SUBSCRIBER = 28;
  const int SMS_FACILITY_REJECTED = 29;
  const int SMS_UNKNOWN_SUBSCRIBER = 30;
  const int SMS_NETWORK_OUT_OF_ORDER = 38;
  const int SMS_TEMPORARY_FAILURE = 41;
  const int SMS_CONGESTION2 = 42;
  const int SMS_RESOURCES_UNAVAILABLE_UNSPECIFIED = 47;
  const int SMS_REQUESTED_FACILITY_NOT_SUBSCRIBED = 50;
  const int SMS_REQUESTED_FACILITY_NOT_IMPLEMENTED = 69;
  const int SMS_INVALID_TRANSACTION_IDENTIFIER = 81;
  const int SMS_SEMANTICALLY_INCORRECT_MESSAGE = 95;
  const int SMS_INVALID_MANDATORY_INFORMATION = 96;
  const int SMS_MESSAGE_TYPE_NONEXISTENT_OR_NOT_IMPLEMENTED = 97;
  const int SMS_MESSAGE_NOT_COMPATIBLE_WITH_SHORT_MESSAGE_PROTOCOL_STATE = 98;
  const int SMS_INFORMATION_ELEMENT_NONEXISTENT_OR_NOT_IMPLEMENTED = 99;
  const int SMS_UNSPECIFIED_PROTOCOL_ERROR = 111;
  const int SMS_UNSPECIFIED_INTERWORKING_ERROR = 127;

  // error codes from ETSI GSM 03.40, section 9.2.3.22
  const int SMS_TELEMATIC_INTERWORKING_NOT_SUPPORTED = 0x80;
  const int SMS_SHORT_MESSAGE_TYPE_0_NOT_SUPPORTED = 0x81;
  const int SMS_CANNOT_REPLACE_SHORT_MESSAGE = 0x82;
  const int SMS_UNSPECIFIED_TP_PID_ERROR = 0x8f;
  const int SMS_DATA_CODING_SCHEME_NOT_SUPPORTED = 0x90;
  const int SMS_MESSAGE_CLASS_NOT_SUPPORTED = 0x91;
  const int SMS_UNSPECIFIEC_TP_DCS_ERROR = 0x9f;
  const int SMS_COMMAND_CANNOT_BE_ACTIONED = 0xa0;
  const int SMS_COMMAND_UNSUPPORTED = 0xa1;
  const int SMS_UNSPECIFIED_TP_COMMAND_ERROR = 0xaf;
  const int SMS_TPDU_NOT_SUPPORTED = 0xb0;
  const int SMS_SC_BUSY = 0xc0;
  const int SMS_NO_SC_SUBSCRIPTION = 0xc1;
  const int SMS_SC_SYSTEM_FAILURE = 0xc2;
  const int SMS_INVALID_SME_ADDRESS = 0xc3;
  const int SMS_DESTINATION_SME_BARRED = 0xc4;
  const int SMS_SM_REJECTED_DUPLICATED_SM = 0xc5;
  const int SMS_SIM_SMS_STORAGE_FULL = 0xd0;
  const int SMS_NO_SMS_STORAGE_CAPABILITY_IN_SIM = 0xd1;
  const int SMS_ERROR_IN_MS = 0xd2;
  const int SMS_MEMORY_CAPACITY_EXCEED = 0xd3;
  const int SMS_UNSPECIFIED_ERROR_CAUSE = 0xff;

  // error codes from ETSI GSM 07.05, section 3.2.5
  const int SMS_ME_FAILURE = 300;
  const int SMS_SMS_SERVICE_OF_ME_RESERVED = 301;
  const int SMS_OPERATION_NOT_ALLOWED = 302;
  const int SMS_OPERATION_NOT_SUPPORTED = 303;
  const int SMS_INVALID_PDU_MODE_PARAMETER = 304;
  const int SMS_INVALID_TEXT_MODE_PARAMETER = 305;
  const int SMS_SIM_NOT_INSERTED = 310;
  const int SMS_SIM_PIN_REQUIRED = 311;
  const int SMS_PH_SIM_PIN_REQUIRED = 312;
  const int SMS_SIM_FAILURE = 313;
  const int SMS_SIM_BUSY = 314;
  const int SMS_SIM_WRONG = 315;
  const int SMS_SIM_PUK_REQUIRED = 316;
  const int SMS_SIM_PIN2_REQUIRED = 317;
  const int SMS_SIM_PUK2_REQUIRED = 318;
  const int SMS_MEMORY_FAILURE = 320;
  const int SMS_INVALID_MEMORY_INDEX = 321;
  const int SMS_MEMORY_FULL = 322;
  const int SMS_SMSC_ADDRESS_UNKNOWN = 330;
  const int SMS_NO_NETWORK_SERVICE = 331;
  const int SMS_NETWORK_TIMEOUT = 332;
  const int SMS_NO_CNMA_ACKNOWLEDGEMENT_EXPECTED = 340;
  const int SMS_UNKNOWN_ERROR = 500;

  // return descriptive text for the given error code
  // the text is already translated
  extern string getSMSErrorText(const int errorCode) throw(GsmException);

  // SMS status handling
  // success codes
  const int SMS_STATUS_RECEIVED = 0;
  const int SMS_STATUS_FORWARDED = 1;
  const int SMS_STATUS_SM_REPLACES = 2;

  // if this bit is set, the error is only temporary and
  // the SC is still trying to transfer the SM
  const int SMS_STATUS_TEMPORARY_BIT = 32;

  // if this bit is set, the error is only temporary and
  // the SC is still trying to transfer the SM
  const int SMS_STATUS_PERMANENT_BIT = 64;
  // both bits may be set at once

  // temporary errors (both bits may be set)
  const int SMS_STATUS_CONGESTION = 0;
  const int SMS_STATUS_SME_BUSY = 1;
  const int SMS_STATUS_NO_RESPONSE_FROM_SME = 2;
  const int SMS_STATUS_SERVICE_REJECTED = 3;
  const int SMS_STATUS_QUALITY_OF_SERVICE_UNAVAILABLE = 4;
  const int SMS_STATUS_ERROR_IN_SME = 5;

  // permanent errors (SMS_STATUS_PERMANENT_BIT is set)
  const int SMS_STATUS_REMOTE_PROCECURE_ERROR = 0;
  const int SMS_STATUS_INCOMPATIBLE_DESTINATION = 1;
  const int SMS_STATUS_CONNECTION_REJECTED_BY_SME = 2;
  const int SMS_STATUS_NOT_OBTAINABLE = 3;
  // const int SMS_STATUS_QUALITY_OF_SERVICE_UNAVAILABLE = 4;
  const int SMS_STATUS_NO_INTERWORKING_AVAILABLE = 5;
  const int SMS_STATUS_SM_VALIDITY_PERDIOD_EXPIRED = 6;
  const int SMS_STATUS_SM_DELETED_BY_ORIGINATING_SME = 7;
  const int SMS_STATUS_SM_DELETED_BY_ADMINISTRATION = 8;
  const int SMS_STATUS_SM_DOES_NOT_EXIST = 9;

  // return text for SMS status code
  // the text is already translated
  string getSMSStatusString(unsigned char status);
};

#endif // GSM_ERROR_H
