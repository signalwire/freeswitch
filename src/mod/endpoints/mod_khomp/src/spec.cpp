/*******************************************************************************

    KHOMP generic endpoint/channel library.
    Copyright (C) 2007-2010 Khomp Ind. & Com.

  The contents of this file are subject to the Mozilla Public License 
  Version 1.1 (the "License"); you may not use this file except in compliance 
  with the License. You may obtain a copy of the License at 
  http://www.mozilla.org/MPL/ 

  Software distributed under the License is distributed on an "AS IS" basis,
  WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for
  the specific language governing rights and limitations under the License.

  Alternatively, the contents of this file may be used under the terms of the
  "GNU Lesser General Public License 2.1" license (the “LGPL" License), in which
  case the provisions of "LGPL License" are applicable instead of those above.

  If you wish to allow use of your version of this file only under the terms of
  the LGPL License and not to allow others to use your version of this file 
  under the MPL, indicate your decision by deleting the provisions above and 
  replace them with the notice and other provisions required by the LGPL 
  License. If you do not delete the provisions above, a recipient may use your 
  version of this file under either the MPL or the LGPL License.

  The LGPL header follows below:

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; if not, write to the Free Software Foundation, 
    Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*******************************************************************************/

#include <regex.hpp>
#include "spec.h"
#include "logger.h"
#include "khomp_pvt.h"
#include "khomp_pvt_kxe1.h"

/************************** Forward declaration *******************************/
static SpecRetType processSpecAtom(std::string &, SpecFlagsType &, SpecFunType &);
static SpecRetType processSpecAtoms(std::string &, SpecFlagsType &, SpecFunType &);

static bool processCallChannelString(std::string &, Board::KhompPvt *&, int *, bool need_free = true);

/******************************************************************************/

static SpecRetType processSpecAtom(std::string & atom, SpecFlagsType & flags, SpecFunType & fun)
{
	std::string allocstr = Strings::trim(atom);

	DBG(FUNC, D("allocation string 'atom': %s") % allocstr.c_str());

    /* check if is a group */
   	if (allocstr.size() >= 1 && (allocstr[0] == 'g' || allocstr[0] == 'G'))
    {
   		std::string group_name = allocstr.substr(1);

		GroupToDestMapType::iterator it = Opt::_groups.find(group_name);

		if (it == Opt::_groups.end())
		{
			LOG(ERROR, FMT("invalid dial string '%s': no valid group found!") % allocstr.c_str());
        	return SPR_FAIL;
		}

		allocstr = it->second;
		return processSpecAtoms(allocstr, flags, fun);
    }

	Regex::Match what(allocstr, Globals::regex_allocation);

	if (!what.matched())
	{
		LOG(ERROR, FMT("invalid dial string '%s': this is not a valid expression.") % allocstr.c_str());
		return SPR_FAIL;
	}

	bool reverse = true;

	try
	{

		unsigned long int board_id = UINT_MAX;

		if (what.matched(3)) //matched [bB]
		{
			board_id = Strings::toulong(what.submatch(5));

			DBG(FUNC, D("board matched: %d") % board_id);


			if (board_id >= Globals::k3lapi.device_count())
			{
            	LOG(ERROR, FMT("invalid dial string '%s': no such board '%d'.") % allocstr.c_str() % board_id);
				return SPR_FAIL;
			}

			switch ((what.submatch(4))[0])
			{
				case 'b': reverse = false; break;
				case 'B': reverse = true;  break;
			}
		}

		else if (what.matched(6)) //matched [sS]
		{
			unsigned int serial_id = Strings::toulong(what.submatch(8));

			DBG(FUNC, D("serial matched: %d") % serial_id);

			for (unsigned int i = 0; i < Globals::k3lapi.device_count(); i++)
			{

				K3L_DEVICE_CONFIG conf = Globals::k3lapi.device_config(i);

				unsigned int tmp = Strings::toulong((const char *)conf.SerialNumber);

				if (tmp == serial_id)
				{
					board_id = i;
					break;
				}
			}

			if (board_id == UINT_MAX)
			{
   	        	LOG(ERROR, FMT("invalid dial string '%s': there is no board with serial '%04d'.") % allocstr.c_str() % serial_id);
				return SPR_FAIL;
			}

			switch ((what.submatch(7))[0])
			{
				case 's': reverse = false; break;
				case 'S': reverse = true;  break;
			}
		}
		else if (what.matched(14)) //matched [rR]
		{
			std::string base_addr = what.submatch(16);

			unsigned int branch_id = Strings::toulong(base_addr);

			if (what.matched(17))
			{
				unsigned int branch2_id = Strings::toulong(what.submatch(18));

				DBG(FUNC, D("branch range matched (%d to %d)") % branch_id % branch2_id);

				switch ((what.submatch(15))[0])
				{
					case 'r': reverse = false; break;
					case 'R': reverse = true;  break;
				}

				if (!reverse)
				{
					for (unsigned int i = branch_id; i <= branch2_id; i++)
					{
						std::string call_addr = BoardE1::KhompPvtFXS::padOrig(base_addr, i - branch_id);
						BranchToObjectMapType::iterator i = Opt::_fxs_branch_map.find(call_addr);

						if (i == Opt::_fxs_branch_map.end())
						{
   	    			    	LOG(WARNING, FMT("invalid value '%s': there is no such branch number.") % call_addr);
							return SPR_FAIL;
						}

						if (!fun(i->second.first, i->second.second, flags))
							return SPR_SUCCESS;
					}
				}
				else
				{
					for (unsigned int i = branch2_id; i >= branch_id; i--)
					{
						std::string call_addr = BoardE1::KhompPvtFXS::padOrig(base_addr, i - branch_id);

						BranchToObjectMapType::iterator i = Opt::_fxs_branch_map.find(call_addr);

						if (i == Opt::_fxs_branch_map.end())
						{
   	    			    	LOG(WARNING, FMT("invalid value '%s': there is no such branch number.") % call_addr);
							return SPR_FAIL;
						}

						if (!fun(i->second.first, i->second.second, flags))
							return SPR_SUCCESS;
					}
				}
			}
			else
			{
				DBG(FUNC, D("branch matched: %s") % base_addr);

				BranchToObjectMapType::iterator i = Opt::_fxs_branch_map.find(base_addr);

				if (i == Opt::_fxs_branch_map.end())
				{
   	    	    	LOG(WARNING, FMT("invalid value '%s': there is no such branch number.") % base_addr);
					return SPR_FAIL;
				}

				if (!fun(i->second.first, i->second.second, flags))
					return SPR_SUCCESS;
			}
		}

		else
		{
           	LOG(ERROR, FMT("invalid dial string '%s': unknown allocation method.") % allocstr.c_str());
			return SPR_FAIL;
		}

		if (what.matched(9)) // matched something about links/channels [cClL]n|[cC]n-m
		{
			DBG(FUNC, D("channel/link matched"));
			unsigned long int object_id = Strings::toulong(what.submatch(11));

			if (what.matched(12))
			{

				DBG(FUNC, D("channel range matched")); // matched [cC]n-m

				if ((what.submatch(10))[0] != 'c' && (what.submatch(10))[0] != 'C')
				{
					LOG(ERROR, FMT("invalid dial string '%s': range just allowed for channels.") % allocstr.c_str());
					return SPR_FAIL;
				}

				unsigned long int object2_id = Strings::toulong(what.submatch(13));

				DBG(FUNC, D("(d=%d,lo=%d,up=%d,r=%s) c") % board_id % object_id % object2_id % (reverse ? "true" : "false"));

				if (reverse)
				{
					for (unsigned int obj = std::min<unsigned int>(object2_id + 1, Globals::k3lapi.channel_count(board_id)); obj > 0 && obj > object_id; obj--)
					{
						if (!fun(board_id, obj-1, flags))
							return SPR_SUCCESS;
					}
				}
				else
				{
					for (unsigned int obj = object_id; obj < std::min<unsigned int>(Globals::k3lapi.channel_count(board_id), object2_id + 1); obj++)
					{
						if (!fun(board_id, obj, flags))
							return SPR_SUCCESS;
					}
				}

			}

			else // matched [cClL]n
			{
				DBG(FUNC, D("individual channel/link matched"));

				switch ((what.submatch(10))[0])
				{
					case 'C':
					case 'c':
						DBG(FUNC, D("individual channel matched"));

						if (!fun(board_id, object_id, flags))
							return SPR_SUCCESS;

                        return SPR_CONTINUE;

					case 'l':
					case 'L':
						DBG(FUNC, D("individual link matched"));

						switch (Globals::k3lapi.device_type(board_id))
						{
							case kdtE1:
							case kdtPR:
							case kdtE1GW:
							case kdtE1IP:
							case kdtE1Spx:
#if K3L_AT_LEAST(2,1,0)
							case kdtE1FXSSpx:
#endif
							{
								unsigned int link_first = object_id * 30;
								unsigned int link_final = ((object_id + 1) * 30);

								if (reverse)
								{
									for (unsigned int obj = std::min(link_final, Globals::k3lapi.channel_count(board_id)); obj > 0 && obj > link_first; obj--)
									{
										if (!fun(board_id, obj-1, flags))
											return SPR_SUCCESS;
									}
								}
								else
								{
									for (unsigned int obj = link_first; obj < std::min(Globals::k3lapi.channel_count(board_id), link_final); obj++)
									{
										if (!fun(board_id, obj, flags))
											return SPR_SUCCESS;
									}
								}
                                return SPR_CONTINUE;

							}

							default:
								LOG(ERROR, FMT("invalid dial string '%s': board '%d' does not have links.")
									% allocstr.c_str() % board_id);
								return SPR_FAIL;
						}

					default:
						LOG(ERROR, FMT("invalid dial string '%s': invalid object specification.") % allocstr.c_str());
						return SPR_FAIL;
				}
			}
		}
		else if (what.matched(3) || what.matched(6)) // matched something about boards [bBsS]
		{
			if (reverse)
			{
				for (unsigned int obj = Globals::k3lapi.channel_count(board_id); obj > 0; obj--)
				{
					if (!fun(board_id, obj-1, flags))
						return SPR_SUCCESS;
		        }
	        }
			else
			{
				for (unsigned int obj = 0; obj < Globals::k3lapi.channel_count(board_id); obj++)
				{
					if (!fun(board_id, obj, flags))
						return SPR_SUCCESS;
				}
			}
		}

	}
	catch (Strings::invalid_value e)
	{
		LOG(ERROR, FMT("invalid dial string '%s': invalid numeric value specified.") % allocstr.c_str());
		return SPR_FAIL;
	}
    catch (Function::EmptyFunction & e)
    {
		LOG(ERROR, FMT("invalid function."));
		return SPR_FAIL;
    }

	return SPR_CONTINUE;
}

static SpecRetType processSpecAtoms(std::string & gotatoms, SpecFlagsType & flags, SpecFunType & fun)
{
	std::string atoms(gotatoms);

	DBG(FUNC, D("allocation string 'atoms': %s") % atoms);


	if (!atoms.empty() && (atoms.at(0) == '*'))  //so it is a "cyclical" allocation
	{
		atoms.erase(0, 1);

		if (flags & SPF_FIRST)
		{
			if (!(flags & SPF_CYCLIC))
			{
				DBG(FUNC, D("got a cyclic/fair allocation (%s), priorizing less used channels...") % atoms);
				flags |= SPF_CYCLIC;
			}
		}
		else
		{
			DBG(FUNC, D("cyclic/fair allocation NOT at first string, ignoring..."));
		}
	}


	Strings::vector_type boundaries;
	Strings::tokenize(atoms, boundaries, "+");

    if (boundaries.size() < 1)
    {
        LOG(ERROR, FMT("invalid dial string '%s': no allocation string found!") % atoms);
        return SPR_FAIL;
    }

	for (Strings::vector_type::iterator iter = boundaries.begin(); iter != boundaries.end(); iter++)
	{
		switch (processSpecAtom(*iter, flags, fun))
		{
			// if had some error processing dialstring, bail out..
			case SPR_FAIL:
				return SPR_FAIL;

			// found someone? return ASAP!
			case SPR_SUCCESS:
				return SPR_SUCCESS;

			// else, keep going..
			case SPR_CONTINUE:
				break;
		}

		flags &= ~SPF_FIRST;
	}

	/* found nothing, but this is NOT an error */
	return SPR_CONTINUE;
}

struct funProcessCallChannelString
{
	funProcessCallChannelString(int *cause, bool need_free)
	: _cause(cause), _need_free(need_free),
	  _all_fail(true), //_fxs_only(true),
	  _pvt(NULL)
	{};

	bool operator()(unsigned int dev, unsigned int obj, SpecFlagsType & flags)
	{
        try
        {
    	    Board::KhompPvt *tmp = Board::get(dev, obj);

	    	// used for cause definition
		    if (_all_fail)
			    _all_fail = (tmp ? !tmp->isOK() : true);
        }
        catch (K3LAPITraits::invalid_channel & err)
        {
            _all_fail = true;
        }

		//used for precise cause definition
        //if (_fxs_only)
		//	_fxs_only = (tmp ? tmp->is_fxs() : false);

		if (flags & SPF_CYCLIC)
		{
			Board::queueAddChannel(_channels, dev, obj);
			return true;
		}
		else
		{
			_pvt = Board::findFree(dev, obj, _need_free);
			return (_pvt == NULL);
		}
		return true;
	}

	Board::KhompPvt * pvt(SpecFlagsType & flags)
	{

		if ((flags & SPF_CYCLIC) && !_pvt)
		{
			 //we have no pvt 'till now, lets find a suitable one..
			_pvt = Board::queueFindFree(_channels);
		}

		if (!_pvt && _cause && !(*_cause))
		{

			if (_all_fail)
			{
				// all channels are in fail
				*_cause = SWITCH_CAUSE_NETWORK_OUT_OF_ORDER;
			}
			else
			{
				//if (_fxs_only)
				//	*_cause = SWITCH_CAUSE_USER_BUSY;
				//else
					*_cause = SWITCH_CAUSE_SWITCH_CONGESTION;
			}
		}

		return _pvt;
	}

	int   * _cause;

	bool    _need_free;

	bool    _all_fail;
	//bool    _fxs_only; //TODO: futuro implementar a parte de FXS

	Board::KhompPvt          * _pvt;
	Board::PriorityCallQueue   _channels;
};

struct funProcessSMSChannelString
{

    funProcessSMSChannelString(int *cause)
    : _cause(cause), _all_fail(true), _pvt(NULL)
    {};  

    bool operator()(unsigned int dev, unsigned int obj, SpecFlagsType & flags)
    {    
        Board::KhompPvt *pvt = Board::findFree(dev, obj);

        if (pvt)
        {
            // found something? check if its GSM 
            if (pvt->application(SMS_CHECK, NULL, NULL))
            {    
                // used for cause definition 
                if (_all_fail)
                    _all_fail = (pvt ? !pvt->isOK() : true);

                if (flags & SPF_CYCLIC)
                {    
                    Board::queueAddChannel(_channels, dev, obj);
                    return true;
                }    
                else 
                {    
                    _pvt = pvt; 
                    return false;
                }    
            }    
            else 
            {    
                // not gsm, return ASAP and stop search 
                LOG(ERROR, PVT_FMT(pvt->target(), "channel is NOT a GSM channel! unable to send message!"));
                return false;
            }
        }

        // keep searching 
        return true;
    }

    Board::KhompPvt * pvt(SpecFlagsType & flags)
    {    
        if ((flags & SPF_CYCLIC) && !_pvt)
        {    
            // we have no pvt 'till now, lets find a suitable one.. 
            _pvt = Board::queueFindFree(_channels);
        }

        if (!_pvt && _cause && !(*_cause))
        {    
            if (_all_fail)
            {    
                // all channels are in fail 
                *_cause = SWITCH_CAUSE_NETWORK_OUT_OF_ORDER;
            }    
            else 
            {    
                // otherwise, congestion..
                *_cause = SWITCH_CAUSE_SWITCH_CONGESTION;
            }
        }

        return _pvt;
    }

    int                            * _cause;

    bool                             _need_free;

    bool                             _all_fail;

	Board::KhompPvt          * _pvt;
	Board::PriorityCallQueue   _channels;
};

struct FunProcessGroupString
{
    /* used for group processing */
    FunProcessGroupString(std::string ctx)
        : _ctx(ctx) {};

    FunProcessGroupString(const FunProcessGroupString &o)
        : _ctx(o._ctx) {};

    bool operator()(unsigned int dev, unsigned int obj, SpecFlagsType & flags) const
    {
        try
        {
            Board::KhompPvt * pvt = Board::get(dev,obj);

            DBG(CONF, FMT("loading context %s for channel %d,%d") % _ctx % dev % obj);

            if (pvt) pvt->_group_context = _ctx;
        }
        catch (K3LAPITraits::invalid_channel & err)
        {
        }

        return true;
    }


    std::string _ctx;
};

static bool processCallChannelString(std::string & str, Board::KhompPvt *& pvt, int * cause, bool need_free)
{
	funProcessCallChannelString  proc(cause, need_free);

	SpecFlagsType   flags = SPF_FIRST;
	SpecFunType     fun(proc, false); //   = ReferenceWrapper < SpecFunType > (proc);

    bool ret = true;

	switch (processSpecAtoms(str, flags, fun))
	{
		case SPR_FAIL:
			DBG(FUNC, D("SPR_FAIL: %p") % cause);

			if (cause)
				*cause = SWITCH_CAUSE_INVALID_NUMBER_FORMAT;

			ret = false;
            break;
		case SPR_SUCCESS:
		case SPR_CONTINUE:
            pvt = proc.pvt(flags);
			DBG(FUNC, D("pvt = %p") % pvt);

			if (cause && !(*cause))
            {
    			if (!pvt)
			    	*cause = SWITCH_CAUSE_INTERWORKING;
                else
			    	*cause = SWITCH_CAUSE_SUCCESS;
            }

			ret = true;
            break;
	}

	return ret;
}

bool processSMSChannelString(std::string & str, Board::KhompPvt *& pvt, int *cause)
{
    funProcessSMSChannelString proc(cause);

    SpecFlagsType flags = SPF_FIRST;
    SpecFunType   fun(proc, false); 

    switch (processSpecAtoms(str, flags, fun)) 
    {    
        case SPR_FAIL:
            DBG(FUNC, FMT("SPR_FAIL: %p") % cause);
            if (cause)
                *cause = SWITCH_CAUSE_INVALID_NUMBER_FORMAT;
            return false;

        case SPR_SUCCESS:
        case SPR_CONTINUE:
            pvt = proc.pvt(flags);
            DBG(FUNC, FMT("pvt = %p") % pvt);
			
            if (cause && !(*cause))
            {
    			if (!pvt)
			    	*cause = SWITCH_CAUSE_INTERWORKING;
                else
			    	*cause = SWITCH_CAUSE_SUCCESS;
            }

            return true;
    }    

    return true;
}

Board::KhompPvt * processDialString (const char *dial_charv, int *cause)
{
	DBG(FUNC, D("c (%p, %p)") % dial_charv % cause);
	std::string           dial_string(dial_charv);
	Strings::vector_type  dial_args;

	Strings::tokenize(dial_string, dial_args, "/");

	Board::KhompPvt *pvt = NULL;

    DBG(FUNC, FMT("processing dial string [%d] : '%s'") % dial_args.size() % dial_string);

	if ((dial_args.size() < 1 || dial_args.size() > 3))
	{
		LOG(ERROR, FMT("invalid dial string '%s': wrong number of separators ('/').") % dial_string);
		return NULL;
	}

	bool dial_string_ok = processCallChannelString(dial_args[0], pvt, cause, true);

	if (pvt == NULL)
	{
		if (dial_string_ok)
            LOG(WARNING, "unable to allocate channel -- no free channel found!");
		return NULL;
    }

    DBG(FUNC, PVT_FMT(pvt->target(), "pvt %p") % pvt);

	unsigned int opt_size = (pvt->hasNumberDial() ? 3 : 2);
	unsigned int opt_arg  = opt_size - 1;
    
	if (dial_args.size() == opt_size)
	{
        Strings::vector_type options_args;
        Strings::tokenize (dial_args[opt_arg], options_args, ":");

		for (Strings::vector_type::iterator opt_arg = options_args.begin();
		     opt_arg != options_args.end(); opt_arg++)
		{
			std::string str = (*opt_arg);

			Strings::vector_type option_item;
			Strings::tokenize (str, option_item, "=");

			switch (option_item.size())
			{
				case 2:
				{
					std::string index (option_item[0]);
					std::string value (option_item[1]);

                    if(pvt->call()->process(index, value))
						continue;

					break;
				}

				case 1:
				{
		            std::string index (option_item[0]);

                    if(pvt->call()->process(index))
						continue;

					break;
				}

				default:
	            {
					LOG(ERROR, FMT("invalid option specification '%s'.") % str);
	                continue;
	            }
			}
            LOG(ERROR, FMT("unknown option name '%s', ignoring...") % str);
        }
	}

    if(pvt->hasNumberDial())
    {
		if (dial_args.size() <= 1)
		{
            LOG(ERROR, FMT("invalid dial string '%s': missing destination number!") % dial_string);
			return NULL;
		}

        std::string name ("dest");
        std::string value (dial_args[1]);

        pvt->call()->process(name, value);

		//pvt->call()->dest_addr = dial_args[1];
	}

    return pvt;
};

Board::KhompPvt * processSMSString (const char *sms_charv, int *cause)
{
    std::string           sms_string(sms_charv);
    Strings::vector_type  sms_args;

    Strings::tokenize(sms_string, sms_args, "/|,", 3); // '/' is a backward compatibility feature! 

    DBG(FUNC, FMT("processing SMS string [%d] : '%s'") % sms_args.size() % sms_string);

    if (sms_args.size () != 3)
    {
        LOG(ERROR, FMT("invalid dial string '%s': wrong number of separators.") % sms_string);
        return NULL;
    }

    Board::KhompPvt *pvt = NULL;

    bool dial_string_ok = processSMSChannelString(sms_args[0], pvt, cause);

    if (pvt == NULL)
    {
        if (dial_string_ok)
        {
            LOG(WARNING, "unable to allocate channel -- no free channel found!");
        }

        return NULL;
    }
    else
    {
        if (!pvt->application(SMS_CHECK, NULL, NULL))
        {
            LOG(ERROR, PVT_FMT(pvt->target(), "allocated channel is NOT a GSM channel! unable to send message!"));

            return NULL;
        }
    }


/*
    std::string dest(sms_args[1]);

    bool conf = false;

    if (dest[0] == '!')
    {
        dest.erase(0,1);
        conf = true;
    }

    if (dest[dest.size()-1] == '!')
    {
        dest.erase(dest.size()-1,1);
        conf = true;
    }

    // get options/values 
    pvt->send_sms.sms_dest = dest;
    pvt->send_sms.sms_conf = conf;
    pvt->send_sms.sms_body = sms_args[2];
*/

    return pvt;
};

void processGroupString()
{
    for (GroupToDestMapType::iterator i = Opt::_groups.begin(); i != Opt::_groups.end(); i++)
    {
        const std::string & name = (*i).first;
        std::string & opts = (*i).second;

        Strings::vector_type tokens;
        Strings::tokenize(opts, tokens, ",:", 2);

        if (tokens.size() != 2 && tokens.size() != 1)
        {
            LOG(WARNING, FMT("wrong number of arguments at group '%s', ignoring group!\n") % name.c_str());
            opts.clear();
            continue;
        }

        if (tokens.size() < 2)
            continue;
        FunProcessGroupString    proc(tokens[1]);

        SpecFlagsType   flags = SPF_FIRST;
        SpecFunType     fun(proc, false);

        switch (processSpecAtoms(tokens[0], flags, fun))
        {
            case SPR_CONTINUE:
                // remove context from spec 
                opts = tokens[0];

                // log this! 
                DBG(CONF, FMT("group '%s' is now '%s', with context '%s'...")
                        % name % tokens[0] % tokens[1]);
                break;

            default:
                LOG(WARNING, FMT("skipping group '%s', bad configuration!\n") % name.c_str());

                // "zero" group 
                opts.clear();

                // log this! 
                DBG(CONF, FMT("group '%s' have misconfigured options, ignoring...") % name);
                break;
        }
    }
}

