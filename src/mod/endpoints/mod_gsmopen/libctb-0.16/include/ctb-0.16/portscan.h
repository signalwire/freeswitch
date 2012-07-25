#ifndef LIBCTB_PORTSCAN_H_INCLUDED_
#define LIBCTB_PORTSCAN_H_INCLUDED_

#include <string>
#include <vector>

/**
   \file portscan.h
*/

namespace ctb {

    /*!
	 \brief returns all available COM ports as an array of strings.
	 \param result stores the available COM ports
	 \param checkInUse return only ports which are available AND 
	 unused (default)
	 \return true if successful, false otherwise
    */
    bool GetAvailablePorts( std::vector<std::string>& result,
					   bool checkInUse = true );

} // namespace ctb

#endif
