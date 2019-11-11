// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.

#ifndef TELNYX_EXCEPTION_H_INCLUDED
#define TELNYX_EXCEPTION_H_INCLUDED


#include <stdexcept>
#include <boost/system/system_error.hpp>
#include <boost/system/error_code.hpp>
#include "Telnyx/Telnyx.h"


namespace Telnyx {


class TELNYX_API Exception: public std::exception
	/// This is the base class for all exceptions defined
	/// in the OSS class library.
{
public:
	Exception(const std::string& msg, int code = 0);
		/// Creates an exception.

	Exception(const std::string& msg, const std::string& arg, int code = 0);
		/// Creates an exception.

	Exception(const std::string& msg, const Exception& nested, int code = 0);
		/// Creates an exception and stores a clone
		/// of the nested exception.

	Exception(const Exception& exc);
		/// Copy constructor.

  Exception(const std::exception& exc);
		/// Copy constructor.

  Exception(const boost::system::system_error& exc);
		/// Copy constructor.
		
	~Exception() throw();
		/// Destroys the exception and deletes the nested exception.

	Exception& operator = (const Exception& exc);
		/// Assignment operator.

  Exception& operator = (const std::exception& exc);
    /// Assignment operator.

  Exception& operator = (const boost::system::system_error& exc);

	virtual const char* name() const throw();
		/// Returns a static string describing the exception.
		
	virtual const char* className() const throw();
		/// Returns the name of the exception class.
		
	virtual const char* what() const throw();
		/// Returns a static string describing the exception.
		///
		/// Same as name(), but for compatibility with std::exception.
		
	const Exception* nested() const;
		/// Returns a pointer to the nested exception, or
		/// null if no nested exception exists.
			
	const std::string& message() const;
		/// Returns the message text.
			
	int code() const;
		/// Returns the exception code if defined.
		
	std::string displayText() const;
		/// Returns a string consisting of the
		/// message name and the message text.

	virtual Exception* clone() const;
		/// Creates an exact copy of the exception.
		///
		/// The copy can later be thrown again by
		/// invoking rethrow() on it.
		
	virtual void rethrow() const;
		/// (Re)Throws the exception.
		///
		/// This is useful for temporarily storing a
		/// copy of an exception (see clone()), then
		/// throwing it again.

protected:
	Exception(int code = 0);
		/// Standard constructor.
		
private:
	std::string _msg;
	Exception*  _pNested;
	int			_code;
};


//
// inlines
//
inline const Exception* Exception::nested() const
{
	return _pNested;
}


inline const std::string& Exception::message() const
{
	return _msg;
}


inline int Exception::code() const
{
	return _code;
}


//
// Macros for quickly declaring and implementing exception classes.
// Unfortunately, we cannot use a template here because character
// pointers (which we need for specifying the exception name)
// are not allowed as template arguments.
//
#define TELNYX_DECLARE_EXCEPTION(API, CLS, BASE) \
	class API CLS: public BASE														\
	{																				\
	public:																			\
		CLS(int code = 0);															\
		CLS(const std::string& msg, int code = 0);									\
		CLS(const std::string& msg, const std::string& arg, int code = 0);			\
		CLS(const std::string& msg, const Telnyx::Exception& exc, int code = 0);		\
		CLS(const CLS& exc);														\
		~CLS() throw();																\
		CLS& operator = (const CLS& exc);											\
		const char* name() const throw();											\
		const char* className() const throw();										\
		Telnyx::Exception* clone() const;												\
		void rethrow() const;														\
	};


#define TELNYX_IMPLEMENT_EXCEPTION(CLS, BASE, NAME)	\
	CLS::CLS(int code): BASE(code)		\
	{					\
	}					\
	CLS::CLS(const std::string& msg, int code): BASE(msg, code)	\
	{					\
	}					\
	CLS::CLS(const std::string& msg, const std::string& arg, int code): BASE(msg, arg, code)		\
	{					\
	}					\
	CLS::CLS(const std::string& msg, const Telnyx::Exception& exc, int code): BASE(msg, exc, code)	\
	{					\
	}					\
	CLS::CLS(const CLS& exc): BASE(exc)	\
	{					\
	}					\
	CLS::~CLS() throw()			\
	{					\
	}					\
	CLS& CLS::operator = (const CLS& exc)			\
	{					\
		BASE::operator = (exc);	\
		return *this;	\
	}	\
	const char* CLS::name() const throw()	\
	{				\
		return NAME;	\
	}		\
	const char* CLS::className() const throw() \
	{																								\
		return typeid(*this).name();	\
	}				\
	Telnyx::Exception* CLS::clone() const \
	{																								\
		return new CLS(*this); 	\
	}			\
	void CLS::rethrow() const \
	{			\
		throw *this;	\
	}


#define TELNYX_CREATE_INLINE_EXCEPTION(CLS, BASE, NAME) \
	class TELNYX_API CLS: public BASE	{ \
	public: \
		CLS(int code = 0); \
		CLS(const std::string& msg, int code = 0); \
		CLS(const std::string& msg, const std::string& arg, int code = 0); \
		CLS(const std::string& msg, const Telnyx::Exception& exc, int code = 0); \
		CLS(const CLS& exc); \
		~CLS() throw();																\
		CLS& operator = (const CLS& exc); \
		const char* name() const throw(); \
		const char* className() const throw();	\
		Telnyx::Exception* clone() const;	\
		void rethrow() const; \
	}; \
        inline CLS::CLS(int code): BASE(code)		\
	{		                    \
	}			            \
	inline CLS::CLS(const std::string& msg, int code): BASE(msg, code) \
	{			\
	}			\
	inline CLS::CLS(const std::string& msg, const std::string& arg, int code): BASE(msg, arg, code)		\
	{ \
	}  \
	inline CLS::CLS(const std::string& msg, const Telnyx::Exception& exc, int code): BASE(msg, exc, code)	\
	{	\
	}       \
	inline CLS::CLS(const CLS& exc): BASE(exc)	\
	{						\
	}						\
	inline CLS::~CLS() throw()			\
	{						\
	}						\
	inline CLS& CLS::operator = (const CLS& exc)	\
	{						\
		BASE::operator = (exc);			\
		return *this;				\
	}						\
	inline const char* CLS::name() const throw()	\
	{						\
		return NAME;				\
	}						\
	inline const char* CLS::className() const throw()		\
	{								\
		return typeid(*this).name();                            \
	}								\
	inline Telnyx::Exception* CLS::clone() const			\
	{								\
		return new CLS(*this);					\
	}								\
	inline void CLS::rethrow() const				\
	{								\
		throw *this;						\
	}


//
// Standard exception classes
//
TELNYX_DECLARE_EXCEPTION(TELNYX_API, LogicException, Exception)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, AssertionViolationException, LogicException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, NullPointerException, LogicException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, BugcheckException, LogicException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, InvalidArgumentException, LogicException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, NotImplementedException, LogicException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, RangeException, LogicException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, IllegalStateException, LogicException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, InvalidAccessException, LogicException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, SignalException, LogicException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, UnhandledException, LogicException)

TELNYX_DECLARE_EXCEPTION(TELNYX_API, RuntimeException, Exception)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, NotFoundException, RuntimeException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, ExistsException, RuntimeException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, TimeoutException, RuntimeException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, SystemException, RuntimeException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, RegularExpressionException, RuntimeException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, LibraryLoadException, RuntimeException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, LibraryAlreadyLoadedException, RuntimeException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, NoThreadAvailableException, RuntimeException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, PropertyNotSupportedException, RuntimeException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, PoolOverflowException, RuntimeException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, NoPermissionException, RuntimeException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, OutOfMemoryException, RuntimeException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, DataException, RuntimeException)

TELNYX_DECLARE_EXCEPTION(TELNYX_API, DataFormatException, DataException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, SyntaxException, DataException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, CircularReferenceException, DataException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, PathSyntaxException, SyntaxException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, IOException, RuntimeException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, FileException, IOException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, FileExistsException, FileException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, FileNotFoundException, FileException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, PathNotFoundException, FileException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, FileReadOnlyException, FileException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, FileAccessDeniedException, FileException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, CreateFileException, FileException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, OpenFileException, FileException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, WriteFileException, FileException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, ReadFileException, FileException)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, UnknownURISchemeException, RuntimeException)

TELNYX_DECLARE_EXCEPTION(TELNYX_API, ApplicationException, Exception)
TELNYX_DECLARE_EXCEPTION(TELNYX_API, BadCastException, RuntimeException)

TELNYX_DECLARE_EXCEPTION(TELNYX_API, NetException, IOException);

} // namespace Telnyx


#endif //TELNYX_EXCEPTION_H_INCLUDED

