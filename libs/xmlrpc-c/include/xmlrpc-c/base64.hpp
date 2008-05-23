#ifndef XMLRPC_BASE64_HPP_INCLUDED
#define XMLRPC_BASE64_HPP_INCLUDED

#include <string>
#include <vector>

namespace xmlrpc_c {


enum newlineCtl {NEWLINE_NO, NEWLINE_YES};

std::string
base64FromBytes(
    std::vector<unsigned char> const& bytes,
    xmlrpc_c::newlineCtl       const  newlineCtl = xmlrpc_c::NEWLINE_YES);



std::vector<unsigned char>
bytesFromBase64(std::string const& base64);


} // namespace

#endif
