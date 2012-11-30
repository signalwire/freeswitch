#ifndef XML_HPP_INCLUDED
#define XML_HPP_INCLUDED

#include <string>
#include <xmlrpc-c/c_util.h>
#include <xmlrpc-c/base.hpp>

namespace xmlrpc_c {
namespace xml {

XMLRPC_DLLEXPORT
void
generateCall(std::string         const& methodName,
             xmlrpc_c::paramList const& paramList,
             std::string *       const  callXmlP);
    
XMLRPC_DLLEXPORT
void
generateCall(std::string         const& methodName,
             xmlrpc_c::paramList const& paramList,
             xmlrpc_dialect      const  dialect,
             std::string *       const  callXmlP);
    
XMLRPC_DLLEXPORT
void
parseCall(std::string           const& callXml,
          std::string *         const  methodNameP,
          xmlrpc_c::paramList * const  paramListP);

XMLRPC_DLLEXPORT
void
generateResponse(xmlrpc_c::rpcOutcome const& outcome,
                 xmlrpc_dialect       const  dialect,
                 std::string *        const  respXmlP);

XMLRPC_DLLEXPORT
void
generateResponse(xmlrpc_c::rpcOutcome const& outcome,
                 std::string *        const  respXmlP);

XMLRPC_DLLEXPORT
void
parseSuccessfulResponse(std::string       const& responseXml,
                        xmlrpc_c::value * const  resultP);

XMLRPC_DLLEXPORT
void
parseResponse(std::string            const& responseXml,
              xmlrpc_c::rpcOutcome * const  outcomeP);

XMLRPC_DLLEXPORT
void
trace(std::string const& label,
      std::string const& xml);


}} // namespace
#endif
