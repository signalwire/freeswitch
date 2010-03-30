// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_me_ta.h
// *
// * Purpose: Mobile Equipment/Terminal Adapter and SMS functions
// *          (ETSI GSM 07.07 and 07.05)
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 3.5.1999
// *************************************************************************

#ifndef GSM_ME_TA_H
#define GSM_ME_TA_H

#include <gsmlib/gsm_error.h>
#include <gsmlib/gsm_event.h>
#include <gsmlib/gsm_phonebook.h>
#include <gsmlib/gsm_sms_store.h>
#include <gsmlib/gsm_util.h>
#include <gsmlib/gsm_at.h>
#include <gsmlib/gsm_sms.h>
#include <string>
#include <vector>

using namespace std;

namespace gsmlib
{
  // *** phone capability description (you could also call it phone quirks)

  struct Capabilities
  {
    bool _hasSMSSCAprefix;      // SMS have service centre address prefix
    int _cpmsParamCount;        // number of SMS store parameters to
                                // CPMS command
    bool _omitsColon;           // omits trailing ':' in AT responses
    bool _veryShortCOPSanswer;  // Falcom A2-1
    bool _wrongSMSStatusCode;   // Motorola Timeport 260
    bool _CDSmeansCDSI;         // Nokia Cellular Card Phone RPE-1 GSM900
    bool _sendAck;              // send ack for directly routed SMS
    Capabilities();             // constructor, set default behaviours
  };
  
  // *** auxiliary structs

  // Static ME information (AT command sequences given in brackets)
  struct MEInfo
  {
    string _manufacturer;       // (+CGMI)
    string _model;              // (+CGMM)
    string _revision;           // (+CGMR)
    string _serialNumber;       // (+CGSN), IMEI
  };

  // modes for network operation selection
  enum OPModes {AutomaticOPMode = 0, ManualOPMode = 1,
                DeregisterOPMode = 2, ManualAutomaticOPMode = 4};

  // status codes or network operaton selection
  enum OPStatus {UnknownOPStatus = 0, AvailableOPStatus = 1,
                 CurrentOPStatus = 2, ForbiddenOPStatus = 3};

  // network operator info
  struct OPInfo
  {
    OPModes _mode;
    OPStatus _status;
    string _longName;
    string _shortName;
    int _numericName;           // may be NOT_SET

    OPInfo() : _status(UnknownOPStatus), _numericName(NOT_SET) {}
  };

  // facility classes
  enum FacilityClass {VoiceFacility = 1, DataFacility = 2, FaxFacility = 4};
  const int ALL_FACILITIES = VoiceFacility | DataFacility | FaxFacility;

  // struct to hold password info
  struct PWInfo
  {
    string _facility;
    int _maxPasswdLen;
  };

  // call forward reasons
  // AllReasons encompasses 0..3
  // AllConditionalReasons encompasses 1..3
  enum ForwardReason {UnconditionalReason = 0, MobileBusyReason = 1,
                      NoReplyReason = 2, NotReachableReason = 3,
                      AllReasons = 4, AllConditionalReasons = 5, NoReason = 6};

  // call forward modes
  enum ForwardMode {DisableMode = 0, EnableMode = 1,
                    RegistrationMode = 3, ErasureMode = 4};

  // call forward info
  struct ForwardInfo
  {
    bool _active;               // status in the network
    FacilityClass _cl;          // voice, fax, or data
    string _number;             // telephone number
    string _subAddr;            // subaddress
    int _time;                  // time in the range 1..30 (for NoReplyReason)
    ForwardReason _reason;      // reason for the forwarding
  };

  // SMS types
  typedef Ref<SMSStore> SMSStoreRef;
  typedef vector<SMSStoreRef> SMSStoreVector;

  // this class allows access to all functions of a ME/TA as described
  // in sections 5-8 of ETSI GSM 07.07
  // Note: If the ME is changed (ie. disconnected an another one connected
  // to the TA), a new ME object must be created
  // (Mobile equipment = ME, terminal adapter = TA)
  class MeTa : public RefBase
  {
  protected:
    Ref<Port> _port;            // port the ME/TA is connected to
    Ref<GsmAt> _at;             // chat object for the port
    PhonebookVector _phonebookCache; // cache of all used phonebooks
    SMSStoreVector _smsStoreCache; // cache of all used phonebooks
    string _lastPhonebookName;  // remember last phonebook set on ME/TA
    string _lastSMSStoreName;   // remember last SMS store set on ME/TA
    Capabilities _capabilities; // ME/TA quirks
    GsmEvent _defaultEventHandler; // default event handler
                                // see comments in MeTa::init()
    string _lastCharSet;        // remember last character set

    // init ME/TA to sensible defaults
    void init() throw(GsmException);

  public:
    // initialize a new MeTa object given the port
    MeTa(Ref<Port> port) throw(GsmException);

    // initialize a new MeTa object given the AT handler
    //MeTa(Ref<GsmAt> at) throw(GsmException);

    // set the current phonebook in the ME
    // remember the last phonebook set for optimisation
    void setPhonebook(string phonebookName) throw(GsmException);

    // set the current SMS store in the ME
    // set storeTypes to
    //   1 to set store for reading and deleting
    //   2 to set store for writing and sending (includes type 1)
    //   3 to preferred store for receiving SMS (includes types 1 and 2)
    // remember the last SMS store set for optimisation
    // if needResultCode is set this optimisation is not done
    string setSMSStore(string smsStore, int storeTypes,
                       bool needResultCode = false)
      throw(GsmException);

    // get current SMS store settings
    void getSMSStore(string &readDeleteStore,
                     string &writeSendStore,
                     string &receiveStore) throw(GsmException);

    // get capabilities of this ME/TA
    Capabilities getCapabilities() const {return _capabilities;}

    // return my port
    Ref<Port> getPort() {return _port;}

    // return my at handler
    Ref<GsmAt> getAt() {return _at;}

    // set event handler for unsolicited result codes
    GsmEvent *setEventHandler(GsmEvent *newHandler)
      {return _at->setEventHandler(newHandler);}

    // wait for an event
    void waitEvent(GsmTime timeout) throw(GsmException);

    // *** ETSI GSM 07.07 Section 5: "General Commands"

    // return ME information
    MEInfo getMEInfo() throw(GsmException);

    // return available character sets
    vector<string> getSupportedCharSets() throw(GsmException);// (+CSCS=?)
    
    // return current character set (default: GSM)
    string getCurrentCharSet() throw(GsmException);// (+CSCS?)

    // set character set to use
    void setCharSet(string charSetName) throw(GsmException);// (+CSCS=)
    
    // *** ETSI GSM 07.07 Section 6: "Call control commands and methods"
    
    // get extended error report
    string getExtendedErrorReport() throw(GsmException);// (+CEER)

    // dial a number, CLI presentation as defined in network
    void dial(string number) throw(GsmException);// (ATD)

    // answer
    void answer() throw(GsmException); // (ATA)

    // hangup
    void hangup() throw(GsmException); // (ATH)
    
    // set Personal Identification Number
    void setPIN(string number) throw(GsmException);// (+CPIN)

    // get PIN Status
    string getPINStatus() throw(GsmException);// (+CPIN?)

    // *** ETSI GSM 07.07 Section 7: "Network service related commands"
    
    // return available network operators
    // this fills in all fields of OPInfo with the exception of _mode
    vector<OPInfo> getAvailableOPInfo() throw(GsmException); // (+COPS=?)

    // return current network operators
    // this fills in all the fields of OPInfo with the exception of _status
    OPInfo getCurrentOPInfo() throw(GsmException);

    // set network operator
    // caller must fill in ALL names it has read from previous calls
    // of getCurrentOPInfo() or getAvailableOPInfo()
    // (because ME/TA might not implement all names)
    void setCurrentOPInfo(OPModes mode,
                          string longName = "",
                          string shortName = "",
                          int numericName = NOT_SET) throw(GsmException);

    // get facility lock capabilities (+CLCK)
    vector<string> getFacilityLockCapabilities() throw(GsmException);

    // query facility lock status for named facility
    bool getFacilityLockStatus(string facility, FacilityClass cl)
      throw(GsmException);

    // lock facility
    void lockFacility(string facility, FacilityClass cl, string passwd = "")
      throw(GsmException);

    // unlock facility
    void unlockFacility(string facility, FacilityClass cl, string passwd = "")
      throw(GsmException);

    // return names of facility for which a password can be set
    // and the maximum length of the respective password
    vector<PWInfo> getPasswords() throw(GsmException);// (+CPWD=?)

    // set password for the given facility
    void setPassword(string facility, string oldPasswd, string newPasswd)
      throw(GsmException);
    // (+CPWD=)

    // get CLIP (caller line identification presentation) in the network
    bool getNetworkCLIP() throw(GsmException);// (+CLIP?)

    // set CLIP presentation on or off
    // enables GsmEvent::callerLineID
    void setCLIPPresentation(bool enable) throw(GsmException);// (+CLIP=)

    // returns if the above is enable
    bool getCLIPPresentation() throw(GsmException);// (+CLIP?)

    // set call forwarding
    void setCallForwarding(ForwardReason reason,
                           ForwardMode mode,
                           string number,
                           string subaddr,
                           FacilityClass cl = (FacilityClass)ALL_FACILITIES,
                           int forwardTime = NOT_SET)
      throw(GsmException); // (+CCFC=)

    // get Information of currently set CF in the network
    // the caller must give the reason to query
    void getCallForwardInfo(ForwardReason reason,
                            ForwardInfo &voice,
                            ForwardInfo &fax,
                            ForwardInfo &data)
      throw(GsmException); // (+CCFC=)


    // *** ETSI GSM 07.07 Section 8: "Mobile Equipment control
    //                                and status commands"

    // return/set ME functionality level (+CFUN):
    // 0 Minimum functionality
    // 1 full functionality
    // 2 disable phone transmit RF circuits only
    // 3 disable phone receive RF circuits only
    // 4 disable phone both transmit and receive RF circuits
    // 5...127 implementation-defined
    int getFunctionalityLevel() throw(GsmException);
    void setFunctionalityLevel(int level) throw(GsmException);

    // return battery charge status (+CBC):
    // 0 ME is powered by the battery
    // 1 ME has a battery connected, but is not powered by it
    // 2 ME does not have a battery connected
    // 3 Recognized power fault, calls inhibited
    int getBatteryChargeStatus() throw(GsmException);

    // return battery charge (range 0..100) (+CBC)
    int getBatteryCharge() throw(GsmException);

    // get signal strength indication (+CSQ):
    // 0 -113 dBm or less
    // 1 -111 dBm
    // 2...30 -109... -53 dBm
    // 31 -51 dBm or greater
    // 99 not known or not detectable
    int getSignalStrength() throw(GsmException);

    // get channel bit error rate (+CSQ):
    // 0...7 as RXQUAL values in the table in GSM 05.08 [20] subclause 8.2.4
    // 99 not known or not detectable
    int getBitErrorRate() throw(GsmException);

    // get available phone book memory storage strings (+CPBS=?)
    vector<string> getPhoneBookStrings() throw(GsmException);

    // get phone book given the phone book memory storage string
    PhonebookRef getPhonebook(string phonebookString,
                              bool preload = false) throw(GsmException);


    // *** ETSI GSM 07.05 SMS functions

    // return service centre address (+CSCA?)
    string getServiceCentreAddress() throw(GsmException);

    // set service centre address (+CSCA=)
    void setServiceCentreAddress(string sca) throw(GsmException);
    
    // return names of available message stores (<mem1>, +CPMS=?)
    vector<string> getSMSStoreNames() throw(GsmException);

    // return SMS store given the name
    SMSStoreRef getSMSStore(string storeName) throw(GsmException);

    // send a single SMS message
    void sendSMS(Ref<SMSSubmitMessage> smsMessage) throw(GsmException);

    // send one or several (concatenated) SMS messages
    // The SUBMIT message template must have all options set, only
    // the userData and the userDataHeader are changed.
    // If oneSMS is true, only one SMS is sent. Otherwise several SMSs
    // are sent. If concatenatedMessageId is != -1 this is used as the message
    // ID for concatenated SMS (for this a user data header as defined in
    // GSM GTS 3.40 is used, the old UDH in the template is overwritten).
    void sendSMSs(Ref<SMSSubmitMessage> smsTemplate, string text,
                  bool oneSMS = false,
                  int concatenatedMessageId = -1)
      throw(GsmException);

    // set SMS service level
    // if set to 1 send commands return ACK PDU, 0 is the default
    void setMessageService(int serviceLevel) throw(GsmException);

    // return SMS service level
    unsigned int getMessageService() throw(GsmException);

    // return true if any of the thre message types GsmEvent::SMSMessageType
    // is routed directly to the TA and not stored in the ME
    void getSMSRoutingToTA(bool &smsRouted, // (+CNMI?)
                           bool &cbsRouted,
                           bool &statusReportsRouted) throw(GsmException);

    // sets routing of SMS to TA to true for all supported SMSMessageTypes
    // if onlyReceptionIndication is set to true
    // only GsmEvent::SMSReceptionIndication is called
    // this has two reasons: GSM 07.05 section 3.4.1 does not recommend
    // direct routing of new SMS to the TA
    // I cannot test direct routing of SMS because it does not work with
    // my hardware
    void setSMSRoutingToTA(bool enableSMS, bool enableCBS,
                           bool enableStatReport,
                           bool onlyReceptionIndication = true)
      throw(GsmException);
    // (+CNMI=)

    bool getCallWaitingLockStatus(FacilityClass cl)
      throw(GsmException);
	
    void setCallWaitingLockStatus(FacilityClass cl,
                                  bool lock)throw(GsmException);

    void setCLIRPresentation(bool enable) throw(GsmException);
    //(+CLIR)
    
    // 0:according to the subscription of the CLIR service
    // 1:CLIR invocation
    // 2:CLIR suppression
    int getCLIRPresentation() throw(GsmException);

    friend class Phonebook;
    friend class SMSStore;
  };
};

#endif // GSM_ME_TA_H
