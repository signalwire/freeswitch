#ifndef CMDLINE_PARSER_HPP_INCLUDED
#define CMDLINE_PARSER_HPP_INCLUDED

#include <string>

struct cmdlineParserCtl;

class CmdlineParser {
public:
    CmdlineParser();

    ~CmdlineParser();

    enum optType {FLAG, INT, UINT, STRING, BINUINT, FLOAT};

    void
    defineOption(std::string const optionName,
                 optType     const optionType);

    void
    processOptions(int           const argc,
                   const char ** const argv);

    bool
    optionIsPresent(std::string const optionName) const;

    int
    getOptionValueInt(std::string const optionName) const;

    unsigned int
    getOptionValueUint(std::string const optionName) const;

    std::string
    getOptionValueString(std::string const optionName) const;
    
    unsigned long long
    getOptionValueBinUint(std::string const optionName) const;

    double
    getOptionValueFloat(std::string const optionName) const;

    unsigned int
    argumentCount() const;

    std::string
    getArgument(unsigned int const argNumber) const;

private:
    struct cmdlineParserCtl * cp;

    // Make sure no one can copy this object, because if there are two
    // copies, there will be two attempts to destroy *cp.
    CmdlineParser(CmdlineParser const&) {};

    CmdlineParser&
    operator=(CmdlineParser const&) {return *this;}
};

#endif
