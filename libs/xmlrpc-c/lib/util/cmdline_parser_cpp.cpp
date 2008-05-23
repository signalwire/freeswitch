#include <string>
#include <stdexcept>

#include "girstring.h"
#include "casprintf.h"
#include "cmdline_parser.h"

#include "cmdline_parser.hpp"

using namespace std;



static enum optiontype
optTypeConvert(
    CmdlineParser::optType const arg) {

    enum optiontype retval;

    retval = OPTTYPE_FLAG;  // defeat compiler warning

    switch (arg) {
    case CmdlineParser::FLAG:    retval = OPTTYPE_FLAG;    break;
    case CmdlineParser::INT:     retval = OPTTYPE_INT;     break;
    case CmdlineParser::UINT:    retval = OPTTYPE_UINT;    break;
    case CmdlineParser::STRING:  retval = OPTTYPE_STRING;  break;
    case CmdlineParser::BINUINT: retval = OPTTYPE_BINUINT; break;
    case CmdlineParser::FLOAT:   retval = OPTTYPE_FLOAT;   break;
    }
    return retval;
}



CmdlineParser::CmdlineParser() {

    this->cp = cmd_createOptionParser();
}



CmdlineParser::~CmdlineParser() {
    cmd_destroyOptionParser(this->cp);
}



void
CmdlineParser::defineOption(
    string  const optionName,
    optType const optionType) {

    cmd_defineOption(this->cp, optionName.c_str(), 
                     optTypeConvert(optionType));
}



void
CmdlineParser::processOptions(
    int           const argc,
    const char ** const argv) {

    const char * error;

    cmd_processOptions(this->cp, argc, argv, &error);
    if (error) {
        string const errorS(error);
        strfree(error);
        throw(runtime_error(errorS));
    }
}



bool
CmdlineParser::optionIsPresent(
    string const optionName) const {

    return (cmd_optionIsPresent(this->cp, optionName.c_str()) ? true : false);
}



int
CmdlineParser::getOptionValueInt(
    string const optionName) const {

    return cmd_getOptionValueInt(this->cp, optionName.c_str());
}



unsigned int
CmdlineParser::getOptionValueUint(
    string const optionName) const {

    return cmd_getOptionValueUint(this->cp, optionName.c_str());
}



unsigned long long
CmdlineParser::getOptionValueBinUint(
    string const optionName) const {

    return cmd_getOptionValueBinUint(this->cp, optionName.c_str());
}



double
CmdlineParser::getOptionValueFloat(
    string const optionName) const {

    return cmd_getOptionValueFloat(this->cp, optionName.c_str());
}



string
CmdlineParser::getOptionValueString(
    string const optionName) const {

    const char * const value =
        cmd_getOptionValueString(this->cp, optionName.c_str());

    string retval;

    if (value) {
        retval = string(value);
        strfree(value);
    } else
        retval = "";
    
    return retval;
}
    


unsigned int
CmdlineParser::argumentCount() const {

    return cmd_argumentCount(this->cp);
}



string
CmdlineParser::getArgument(
    unsigned int const argNumber) const {

    const char * const value = cmd_getArgument(this->cp, argNumber);
    string const retval(value);
    strfree(value);
    return retval;
}
