// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.

#include "Telnyx/Telnyx.h"
#include "Telnyx/UTL/CoreUtils.h"
#include "Poco/Util/Util.h"
#include "Poco/Util/Application.h"
#include "Poco/Util/LayeredConfiguration.h"
#include "Poco/Util/OptionSet.h"
#include "Poco/Util/Option.h"
#include "Poco/Util/SystemConfiguration.h"
#include "Poco/Util/MapConfiguration.h"
#include "Poco/Util/PropertyFileConfiguration.h"
#include "Poco/Util/IniFileConfiguration.h"
#include "Poco/Util/LoggingSubsystem.h"
#include "Poco/Util/OptionProcessor.h"
#include "Poco/Util/Validator.h"
#include "Poco/Util/LoggingConfigurator.h"
#include "Poco/AutoPtr.h"
#include "Poco/Logger.h"
#include "Poco/Path.h"
#include "Poco/Timestamp.h"
#include "Poco/Timespan.h"
#include "Poco/AutoPtr.h"
#include "Poco/Event.h"
#include "Poco/Exception.h"
#include "Poco/Process.h"
#include "Poco/NumberFormatter.h"
#include "Poco/NamedEvent.h"
#include "Poco/Environment.h"
#include "Poco/File.h"
#include "Poco/String.h"
#include "Poco/ConsoleChannel.h"

#include <stdio.h>
#include <vector>
#include <typeinfo>

#if defined(POCO_OS_FAMILY_UNIX)
#include "Poco/TemporaryFile.h"
#include "Poco/SignalHandler.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/stat.h>
#include <fstream>
#elif defined(POCO_OS_FAMILY_WINDOWS)
#include "Poco/Util/WinService.h"
#include "Poco/UnWindows.h"
#include <cstring>
#endif
#if defined(POCO_WIN32_UTF8) && !defined(POCO_NO_WSTRING)
#include "Poco/UnicodeConverter.h"
#endif


using Poco::Logger;
using Poco::Path;
using Poco::File;
using Poco::Environment;
using Poco::SystemException;
using Poco::ConsoleChannel;
using Poco::NumberFormatter;
using Poco::AutoPtr;
using Poco::icompare;


namespace Poco {
namespace Util {


class OSSApplication;
class OptionSet;


class OSSSubsystem: public Poco::RefCountedObject
	/// Subsystems extend an application in a modular way.
	///
	/// The Subsystem class provides a common interface
	/// for subsystems so that subsystems can be automatically
	/// initialized at startup and uninitialized at shutdown.
	///
	/// Subsystems should also support dynamic reconfiguration,
	/// so that they can be reconfigured anytime during the
	/// life of a running application.
	///
	/// The degree to which dynamic reconfiguration is supported
	/// is up to the actual subsystem implementation. It can
	/// range from ignoring the reconfiguration request (not
	/// recommended), to changing certain settings that affect
	/// the performance, to a complete reinitialization.
{
public:
	OSSSubsystem();
		/// Creates the Subsystem.

protected:
	virtual const char* name() const = 0;
		/// Returns the name of the subsystem.
		/// Must be implemented by subclasses.

	virtual void initialize(OSSApplication& app) = 0;
		/// Initializes the subsystem.

	virtual void uninitialize() = 0;
		/// Uninitializes the subsystem.

	virtual void reinitialize(OSSApplication& app);
		/// Re-initializes the subsystem.
		///
		/// The default implementation just calls
		/// uninitialize() followed by initialize().
		/// Actual implementations might want to use a
		/// less radical and possibly more performant
		/// approach.

	virtual void defineOptions(OptionSet& options);
		/// Called before the Application's command line processing begins.
		/// If a subsystem wants to support command line arguments,
		/// it must override this method.
		/// The default implementation does not define any options.
		///
		/// To effectively handle options, a subsystem should either bind
		/// the option to a configuration property or specify a callback
		/// to handle the option.

	virtual ~OSSSubsystem();
		/// Destroys the Subsystem.

	friend class OSSApplication;

private:
	OSSSubsystem(const OSSSubsystem&);
	OSSSubsystem& operator = (const OSSSubsystem&);
};

OSSSubsystem::OSSSubsystem()
{
}


OSSSubsystem::~OSSSubsystem()
{
}


void OSSSubsystem::reinitialize(OSSApplication& app)
{
	uninitialize();
	initialize(app);
}


void OSSSubsystem::defineOptions(OptionSet& options)
{
}

class OSSLoggingSubsystem: public OSSSubsystem
	/// The OSSLoggingSubsystem class initializes the logging
	/// framework using the LoggingConfigurator.
	///
	/// It also sets the Application's logger to
	/// the logger specified by the "application.logger"
	/// property, or to "Application" if the property
	/// is not specified.
{
public:
	OSSLoggingSubsystem();
	const char* name() const;

protected:
	void initialize(OSSApplication& self);
	void uninitialize();
	~OSSLoggingSubsystem();
};

OSSLoggingSubsystem::OSSLoggingSubsystem()
{
}


OSSLoggingSubsystem::~OSSLoggingSubsystem()
{
}


const char* OSSLoggingSubsystem::name() const
{
	return "Logging Subsystem";
}


void OSSLoggingSubsystem::uninitialize()
{
}


class OSSApplication: public OSSSubsystem
	/// The Application class implements the main subsystem
	/// in a process. The application class is responsible for
	/// initializing all its subsystems.
	///
	/// Subclasses can and should override the following virtual methods:
	///   - initialize() (the one-argument, protected variant)
	///   - uninitialize()
	///   - reinitialize()
	///   - defineOptions()
	///   - handleOption()
	///   - main()
	///
	/// The application's main logic should be implemented in
	/// the main() method.
	///
	/// There may be at most one instance of the Application class
	/// in a process.
	///
	/// The Application class maintains a LayeredConfiguration (available
	/// via the config() member function) consisting of:
	///   - a MapConfiguration (priority -100) storing application-specific
	///     properties, as well as properties from bound command line arguments.
	///   - a SystemConfiguration (priority 100)
	///   - the configurations loaded with loadConfiguration().
	///
	/// The Application class sets a few default properties in
	/// its configuration. These are:
	///   - application.path: the absolute path to application executable
	///   - application.name: the file name of the application executable
	///   - application.baseName: the file name (excluding extension) of the application executable
	///   - application.dir: the path to the directory where the application executable resides
	///   - application.configDir: the path to the directory where the last configuration file loaded with loadConfiguration() was found.
	///
	/// If loadConfiguration() has never been called, application.configDir will be equal to application.dir.
	///
	/// The POCO_APP_MAIN macro can be used to implement main(argc, argv).
	/// If POCO has been built with POCO_WIN32_UTF8, POCO_APP_MAIN supports
	/// Unicode command line arguments.
{
public:
	enum ExitCode
		/// Commonly used exit status codes.
		/// Based on the definitions in the 4.3BSD <sysexits.h> header file.
	{
		EXIT_OK          = 0,  /// successful termination
		EXIT_USAGE	     = 64, /// command line usage error
		EXIT_DATAERR     = 65, /// data format error
		EXIT_NOINPUT     = 66, /// cannot open input
		EXIT_NOUSER      = 67, /// addressee unknown
		EXIT_NOHOST      = 68, /// host name unknown
		EXIT_UNAVAILABLE = 69, /// service unavailable
		EXIT_SOFTWARE    = 70, /// internal software error
		EXIT_OSERR	     = 71, /// system error (e.g., can't fork)
		EXIT_OSFILE      = 72, /// critical OS file missing
		EXIT_CANTCREAT   = 73, /// can't create (user) output file
		EXIT_IOERR       = 74, /// input/output error
		EXIT_TEMPFAIL    = 75, /// temp failure; user is invited to retry
		EXIT_PROTOCOL    = 76, /// remote error in protocol
		EXIT_NOPERM      = 77, /// permission denied
		EXIT_CONFIG      = 78  /// configuration error
	};

	enum ConfigPriority
	{
		PRIO_APPLICATION = -100,
		PRIO_DEFAULT     = 0,
		PRIO_SYSTEM      = 100
	};

	OSSApplication();
		/// Creates the Application.

	OSSApplication(int argc, char* argv[]);
		/// Creates the Application and calls init(argc, argv).

	void addSubsystem(OSSSubsystem* pSubsystem);
		/// Adds a new subsystem to the application. The
		/// application immediately takes ownership of it, so that a
		/// call in the form
		///     OSSApplication::instance().addSubsystem(new MySubsystem);
		/// is okay.

	void init(int argc, char* argv[]);
		/// Initializes the application and all registered subsystems,
		/// using the given command line arguments.

#if defined(POCO_WIN32_UTF8) && !defined(POCO_NO_WSTRING)
	void init(int argc, wchar_t* argv[]);
		/// Initializes the application and all registered subsystems,
		/// using the given command line arguments.
		///
		/// This Windows-specific version of init is used for passing
		/// Unicode command line arguments from wmain().
#endif

	void init(const std::vector<std::string>& args);
		/// Initializes the application and all registered subsystems,
		/// using the given command line arguments.

	bool initialized() const;
		/// Returns true iff the application is in initialized state
		/// (that means, has been initialized but not yet uninitialized).

	void setUnixOptions(bool flag);
		/// Specify whether command line option handling is Unix-style
		/// (flag == true; default) or Windows/OpenVMS-style (flag == false).
		///
		/// This member function should be called from the constructor of
		/// a subclass to be effective.

	int loadConfiguration(int priority = PRIO_DEFAULT);
		/// Loads configuration information from a default location.
		///
		/// The configuration(s) will be added to the application's
		/// LayeredConfiguration with the given priority.
		///
		/// The configuration file(s) must be located in the same directory
		/// as the executable or a parent directory of it, and must have the
		/// same base name as the executable, with one of the following extensions:
		/// .properties, .ini or .xml.
		///
		/// The .properties file, if it exists, is loaded first, followed
		/// by the .ini file and the .xml file.
		///
		/// If the application is built in debug mode (the _DEBUG preprocessor
		/// macro is defined) and the base name of the appication executable
		/// ends with a 'd', a config file without the 'd' ending its base name is
		/// also found.
		///
		/// Example: Given the application "SampleAppd.exe", built in debug mode.
		/// Then loadConfiguration() will automatically find a configuration file
		/// named "SampleApp.properties" if it exists and if "SampleAppd.properties"
		/// cannot be found.
		///
		/// Returns the number of configuration files loaded, which may be zero.
		///
		/// This method must not be called before initialize(argc, argv)
		/// has been called.

	void loadConfiguration(const std::string& path, int priority = PRIO_DEFAULT);
		/// Loads configuration information from the file specified by
		/// the given path. The file type is determined by the file
		/// extension. The following extensions are supported:
		///   - .properties - properties file (PropertyFileConfiguration)
		///   - .ini        - initialization file (IniFileConfiguration)
		///   - .xml        - XML file (XMLConfiguration)
		///
		/// Extensions are not case sensitive.
		///
		/// The configuration will be added to the application's
		/// LayeredConfiguration with the given priority.
		///

  void loadConfiguration(std::istream& strm, int priority = PRIO_DEFAULT);
		/// Loads configuration information from the stream specified by
		/// the given path. The file type is determined by the file
		/// extension. The following extensions are supported:
		///   - .properties - properties file (PropertyFileConfiguration)
		///   - .ini        - initialization file (IniFileConfiguration)
		///   - .xml        - XML file (XMLConfiguration)
		///
		/// Extensions are not case sensitive.
		///
		/// The configuration will be added to the application's
		/// LayeredConfiguration with the given priority.
		///

	template <class C> C& getSubsystem() const;
		/// Returns a reference to the subsystem of the class
		/// given as template argument.
		///
		/// Throws a NotFoundException if such a subsystem has
		/// not been registered.

	virtual int run();
		/// Runs the application by performing additional initializations
		/// and calling the main() method.

	std::string commandName() const;
		/// Returns the command name used to invoke the application.

	LayeredConfiguration& config() const;
		/// Returns the application's configuration.

	Poco::Logger& logger() const;
		/// Returns the application's logger.
		///
		/// Before the logging subsystem has been initialized, the
		/// application's logger is "ApplicationStartup", which is
		/// connected to a ConsoleChannel.
		///
		/// After the logging subsystem has been initialized, which
		/// usually happens as the first action in OSSApplication::initialize(),
		/// the application's logger is the one specified by the
		/// "application.logger" configuration property. If that property
		/// is not specified, the logger is "Application".

	const OptionSet& options() const;
		/// Returns the application's option set.

	static OSSApplication& instance();
		/// Returns a reference to the Application singleton.
		///
		/// Throws a NullPointerException if no Application instance exists.

	const Poco::Timestamp& startTime() const;
		/// Returns the application start time (UTC).

	Poco::Timespan uptime() const;
		/// Returns the application uptime.

	void stopOptionsProcessing();
		/// If called from an option callback, stops all further
		/// options processing.
		///
		/// If called, the following options on the command line
		/// will not be processed, and required options will not
		/// be checked.
		///
		/// This is useful, for example, if an option for displaying
		/// help information has been encountered and no other things
		/// besides displaying help shall be done.

	const char* name() const;

protected:
	void initialize(OSSApplication& self);
		/// Initializes the application and all registered subsystems.
		/// Subsystems are always initialized in the exact same order
		/// in which they have been registered.
		///
		/// Overriding implementations must call the base class implementation.

	void uninitialize();
		/// Uninitializes the application and all registered subsystems.
		/// Subsystems are always uninitialized in reverse order in which
		/// they have been initialized.
		///
		/// Overriding implementations must call the base class implementation.

	void reinitialize(OSSApplication& self);
		/// Re-nitializes the application and all registered subsystems.
		/// Subsystems are always reinitialized in the exact same order
		/// in which they have been registered.
		///
		/// Overriding implementations must call the base class implementation.

	virtual void defineOptions(OptionSet& options);
		/// Called before command line processing begins.
		/// If a subclass wants to support command line arguments,
		/// it must override this method.
		/// The default implementation does not define any options itself,
		/// but calls defineOptions() on all registered subsystems.
		///
		/// Overriding implementations should call the base class implementation.

	virtual void handleOption(const std::string& name, const std::string& value);
		/// Called when the option with the given name is encountered
		/// during command line arguments processing.
		///
		/// The default implementation does option validation, bindings
		/// and callback handling.
		///
		/// Overriding implementations must call the base class implementation.

	void setLogger(Poco::Logger& logger);
		/// Sets the logger used by the application.

	virtual int main(const std::vector<std::string>& args);
		/// The application's main logic.
		///
		/// Unprocessed command line arguments are passed in args.
		/// Note that all original command line arguments are available
		/// via the properties application.argc and application.argv[<n>].
		///
		/// Returns an exit code which should be one of the values
		/// from the ExitCode enumeration.

	bool findFile(Poco::Path& path) const;
		/// Searches for the file in path in the application directory.
		///
		/// If path is absolute, the method immediately returns true and
		/// leaves path unchanged.
		///
		/// If path is relative, searches for the file in the application
		/// directory and in all subsequent parent directories.
		/// Returns true and stores the absolute path to the file in
		/// path if the file could be found. Returns false and leaves path
		/// unchanged otherwise.

	void init();
		/// Common initialization code.

	~OSSApplication();
		/// Destroys the Application and deletes all registered subsystems.

private:
	void setup();
	void setArgs(int argc, char* argv[]);
	void setArgs(const std::vector<std::string>& args);
	void getApplicationPath(Poco::Path& path) const;
	void processOptions();
	bool findAppConfigFile(const std::string& appName, const std::string& extension, Poco::Path& path) const;

	typedef Poco::AutoPtr<OSSSubsystem> SubsystemPtr;
	typedef std::vector<SubsystemPtr> SubsystemVec;
	typedef Poco::AutoPtr<LayeredConfiguration> ConfigPtr;
	typedef std::vector<std::string> ArgVec;

	ConfigPtr       _pConfig;
	SubsystemVec    _subsystems;
	bool            _initialized;
	std::string     _command;
	ArgVec          _args;
	OptionSet       _options;
	bool            _unixOptions;
	Poco::Logger*   _pLogger;
	Poco::Timestamp _startTime;
	bool            _stopOptionsProcessing;

	static OSSApplication* _pInstance;

	friend class OSSLoggingSubsystem;

	OSSApplication(const OSSApplication&);
	OSSApplication& operator = (const OSSApplication&);
};


//
// inlines
//
template <class C> C& OSSApplication::getSubsystem() const
{
	for (SubsystemVec::const_iterator it = _subsystems.begin(); it != _subsystems.end(); ++it)
	{
		const OSSSubsystem* pSS(it->get());
		const C* pC = dynamic_cast<const C*>(pSS);
		if (pC) return *const_cast<C*>(pC);
	}
	throw Poco::NotFoundException("The subsystem has not been registered", typeid(C).name());
}


inline bool OSSApplication::initialized() const
{
	return _initialized;
}


inline LayeredConfiguration& OSSApplication::config() const
{
	return *const_cast<LayeredConfiguration*>(_pConfig.get());
}


inline Poco::Logger& OSSApplication::logger() const
{
	poco_check_ptr (_pLogger);
	return *_pLogger;
}


inline const OptionSet& OSSApplication::options() const
{
	return _options;
}


inline OSSApplication& OSSApplication::instance()
{
	poco_check_ptr (_pInstance);
	return *_pInstance;
}


inline const Poco::Timestamp& OSSApplication::startTime() const
{
	return _startTime;
}


inline Poco::Timespan OSSApplication::uptime() const
{
	Poco::Timestamp now;
	Poco::Timespan uptime = now - _startTime;

	return uptime;
}

OSSApplication* OSSApplication::_pInstance = 0;


OSSApplication::OSSApplication():
	_pConfig(new LayeredConfiguration),
	_initialized(false),
	_unixOptions(true),
	_pLogger(&Logger::get("ApplicationStartup")),
	_stopOptionsProcessing(false)
{
	setup();
}


OSSApplication::OSSApplication(int argc, char* argv[]):
	_pConfig(new LayeredConfiguration),
	_initialized(false),
	_unixOptions(true),
	_pLogger(&Logger::get("ApplicationStartup")),
	_stopOptionsProcessing(false)
{
	setup();
	init(argc, argv);
}


OSSApplication::~OSSApplication()
{
	try
	{
		uninitialize();
	}
	catch (...)
	{
	}
	_pInstance = 0;
}


void OSSApplication::setup()
{
	poco_assert (_pInstance == 0);

	_pConfig->add(new SystemConfiguration, PRIO_SYSTEM, false, false);
	_pConfig->add(new MapConfiguration, PRIO_APPLICATION, true, false);

	addSubsystem(new OSSLoggingSubsystem);

#if defined(POCO_OS_FAMILY_UNIX)
	#if !defined(_DEBUG)
	Poco::SignalHandler::install();
	#endif
#else
	setUnixOptions(false);
#endif

	_pInstance = this;

	AutoPtr<ConsoleChannel> pCC = new ConsoleChannel;
	Logger::setChannel("", pCC);
}


void OSSApplication::addSubsystem(OSSSubsystem* pSubsystem)
{
	poco_check_ptr (pSubsystem);

	_subsystems.push_back(pSubsystem);
}


void OSSApplication::init(int argc, char* argv[])
{
	setArgs(argc, argv);
	init();
}


#if defined(POCO_WIN32_UTF8) && !defined(POCO_NO_WSTRING)
void OSSApplication::init(int argc, wchar_t* argv[])
{
	std::vector<std::string> args;
	for (int i = 0; i < argc; ++i)
	{
		std::string arg;
		Poco::UnicodeConverter::toUTF8(argv[i], arg);
		args.push_back(arg);
	}
	init(args);
}
#endif


void OSSApplication::init(const std::vector<std::string>& args)
{
	setArgs(args);
	init();
}


void OSSApplication::init()
{
	Path appPath;
	getApplicationPath(appPath);
	_pConfig->setString("application.path", appPath.toString());
	_pConfig->setString("application.name", appPath.getFileName());
	_pConfig->setString("application.baseName", appPath.getBaseName());
	_pConfig->setString("application.dir", appPath.parent().toString());
	_pConfig->setString("application.configDir", appPath.parent().toString());
	processOptions();
	initialize(*this);
}


const char* OSSApplication::name() const
{
	return "Application";
}


void OSSApplication::initialize(OSSApplication& self)
{
	for (SubsystemVec::iterator it = _subsystems.begin(); it != _subsystems.end(); ++it)
	{
		_pLogger->debug(std::string("Initializing subsystem: ") + (*it)->name());
		(*it)->initialize(self);
	}
	_initialized = true;
}


void OSSApplication::uninitialize()
{
	if (_initialized)
	{
		for (SubsystemVec::reverse_iterator it = _subsystems.rbegin(); it != _subsystems.rend(); ++it)
		{
			_pLogger->debug(std::string("Uninitializing subsystem: ") + (*it)->name());
			(*it)->uninitialize();
		}
		_initialized = false;
	}
}


void OSSApplication::reinitialize(OSSApplication& self)
{
	for (SubsystemVec::iterator it = _subsystems.begin(); it != _subsystems.end(); ++it)
	{
		_pLogger->debug(std::string("Re-initializing subsystem: ") + (*it)->name());
		(*it)->reinitialize(self);
	}
}


void OSSApplication::setUnixOptions(bool flag)
{
	_unixOptions = flag;
}


int OSSApplication::loadConfiguration(int priority)
{
    Path appPath;
    getApplicationPath(appPath);
    Path cfgPath;
    //
    // Check if the INI file is located in the same folder as the binary
    //
    if (findAppConfigFile(appPath.getBaseName(), "properties", cfgPath))
    {
        _pConfig->add(new PropertyFileConfiguration(cfgPath.toString()), priority, false, false);
        _pConfig->setString("application.configDir", cfgPath.parent().toString());
        return 1;
    }

    return 0;
}


void OSSApplication::loadConfiguration(const std::string& path, int priority)
{
Path confPath(path);
std::string ext = confPath.getExtension();
if (icompare(ext, "properties") == 0)
    _pConfig->add(new PropertyFileConfiguration(confPath.toString()), priority, false, false);
else
    throw Poco::InvalidArgumentException("Unsupported configuration file type", ext);
}

void OSSApplication::loadConfiguration(std::istream& strm, int priority)
{
  _pConfig->add(new PropertyFileConfiguration(strm), priority, false, false);
}

std::string OSSApplication::commandName() const
{
	return _pConfig->getString("application.baseName");
}

void OSSApplication::stopOptionsProcessing()
{
	_stopOptionsProcessing = true;
}


int OSSApplication::run()
{
	try
	{
		return main(_args);
	}
	catch (Poco::Exception& exc)
	{
		logger().log(exc);
	}
	catch (std::exception& exc)
	{
		logger().error(exc.what());
	}
	catch (...)
	{
		logger().fatal("system exception");
	}
	return EXIT_SOFTWARE;
}


int OSSApplication::main(const std::vector<std::string>& args)
{
	return EXIT_OK;
}


void OSSApplication::setArgs(int argc, char* argv[])
{
	_command = argv[0];
	_pConfig->setInt("application.argc", argc);
	_args.reserve(argc);
	std::string argvKey = "application.argv[";
	for (int i = 0; i < argc; ++i)
	{
		std::string arg(argv[i]);
		_pConfig->setString(argvKey + NumberFormatter::format(i) + "]", arg);
		_args.push_back(arg);
	}
}


void OSSApplication::setArgs(const std::vector<std::string>& args)
{
	poco_assert (!args.empty());

	_command = args[0];
	_pConfig->setInt("application.argc", (int) args.size());
	_args = args;
	std::string argvKey = "application.argv[";
	for (std::size_t i = 0; i < args.size(); ++i)
	{
		_pConfig->setString(argvKey + NumberFormatter::format(i) + "]", args[i]);
	}
}


void OSSApplication::processOptions()
{
	defineOptions(_options);
	OptionProcessor processor(_options);
	processor.setUnixStyle(_unixOptions);
	_args.erase(_args.begin());
	ArgVec::iterator it = _args.begin();
	while (it != _args.end() && !_stopOptionsProcessing)
	{
		std::string name;
		std::string value;
		if (processor.process(*it, name, value))
		{
			if (!name.empty()) // "--" option to end options processing
			{
				handleOption(name, value);
			}
			it = _args.erase(it);
		}
		else ++it;
	}
	if (!_stopOptionsProcessing)
		processor.checkRequired();
}


void OSSApplication::getApplicationPath(Poco::Path& appPath) const
{
#if defined(POCO_OS_FAMILY_UNIX)
	if (_command.find('/') != std::string::npos)
	{
		Path path(_command);
		if (path.isAbsolute())
		{
			appPath = path;
		}
		else
		{
			appPath = Path::current();
			appPath.append(path);
		}
	}
	else
	{
		if (!Path::find(Environment::get("PATH"), _command, appPath))
			appPath = Path(Path::current(), _command);
		appPath.makeAbsolute();
	}
#elif defined(POCO_OS_FAMILY_WINDOWS)
	#if defined(POCO_WIN32_UTF8) && !defined(POCO_NO_WSTRING)
		wchar_t path[1024];
		int n = GetModuleFileNameW(0, path, sizeof(path)/sizeof(wchar_t));
		if (n > 0)
		{
			std::string p;
			Poco::UnicodeConverter::toUTF8(path, p);
			appPath = p;
		}
		else throw SystemException("Cannot get application file name.");
	#else
		char path[1024];
		int n = GetModuleFileNameA(0, path, sizeof(path));
		if (n > 0)
			appPath = path;
		else
			throw SystemException("Cannot get application file name.");
	#endif
#else
	appPath = _command;
#endif
}


bool OSSApplication::findFile(Poco::Path& path) const
{
	if (path.isAbsolute()) return true;

	Path appPath;
	getApplicationPath(appPath);
	Path base = appPath.parent();
	do
	{
		Path p(base, path);
		File f(p);
		if (f.exists())
		{
			path = p;
			return true;
		}
		if (base.depth() > 0) base.popDirectory();
	}
	while (base.depth() > 0);
	return false;
}


bool OSSApplication::findAppConfigFile(const std::string& appName, const std::string& extension, Path& path) const
{
	poco_assert (!appName.empty());

	Path p(appName);
	p.setExtension(extension);
	bool found = findFile(p);
	if (!found)
	{
#if defined(_DEBUG)
		if (appName[appName.length() - 1] == 'd')
		{
			p.setBaseName(appName.substr(0, appName.length() - 1));
			found = findFile(p);
		}
#endif
	}
	if (found)
		path = p;
	return found;
}


void OSSApplication::defineOptions(OptionSet& options)
{
	for (SubsystemVec::iterator it = _subsystems.begin(); it != _subsystems.end(); ++it)
	{
		(*it)->defineOptions(options);
	}
}


void OSSApplication::handleOption(const std::string& name, const std::string& value)
{
	const Option& option = _options.getOption(name);
	if (option.validator())
	{
		option.validator()->validate(option, value);
	}
	if (!option.binding().empty())
	{
		AbstractConfiguration* pConfig = option.config();
		if (!pConfig) pConfig = &config();
		pConfig->setString(option.binding(), value);
	}
	if (option.callback())
	{
		option.callback()->invoke(name, value);
	}
}


void OSSApplication::setLogger(Logger& logger)
{
	_pLogger = &logger;
}

void OSSLoggingSubsystem::initialize(OSSApplication& app)
{
	LoggingConfigurator configurator;
	configurator.configure(&app.config());
	std::string logger = app.config().getString("application.logger", "Application");
	app.setLogger(Logger::get(logger));
}



class OSSServerApplication: public OSSApplication
	/// A subclass of the Application class that is used
	/// for implementing server applications.
	///
	/// A OSSServerApplication allows for the application
	/// to run as a Windows service or as a Unix daemon
	/// without the need to add extra code.
	///
	/// For a OSSServerApplication to work both from the command line
	/// and as a daemon or service, a few rules must be met:
	///   - Subsystems must be registered in the constructor.
	///   - All non-trivial initializations must be made in the
	///     initialize() method.
	///   - At the end of the main() method, waitForTerminationRequest()
	///     should be called.
	///   - The main(argc, argv) function must look as follows:
	///
	///   int main(int argc, char** argv)
	///   {
	///       MyOSSServerApplication app;
	///       return app.run(argc, argv);
	///   }
	///
	/// The POCO_SERVER_MAIN macro can be used to implement main(argc, argv).
	/// If POCO has been built with POCO_WIN32_UTF8, POCO_SERVER_MAIN supports
	/// Unicode command line arguments.
	///
	/// On Windows platforms, an application built on top of the
	/// OSSServerApplication class can be run both from the command line
	/// or as a service.
	///
	/// To run an application as a Windows service, it must be registered
	/// with the Windows Service Control Manager (SCM). To do this, the application
	/// can be started from the command line, with the /registerService option
	/// specified. This causes the application to register itself with the
	/// SCM, and then exit. Similarly, an application registered as a service can
	/// be unregistered, by specifying the /unregisterService option.
	/// The file name of the application executable (excluding the .exe suffix)
	/// is used as the service name. Additionally, a more user-friendly name can be
	/// specified, using the /displayName option (e.g., /displayName="Demo Service").
	///
	/// An application can determine whether it is running as a service by checking
	/// for the "application.runAsService" configuration property.
	///
	///     if (config().getBool("application.runAsService", false))
	///     {
	///         // do service specific things
	///     }
	///
	/// Note that the working directory for an application running as a service
	/// is the Windows system directory (e.g., C:\Windows\system32). Take this
	/// into account when working with relative filesystem paths. Also, services
	/// run under a different user account, so an application that works when
	/// started from the command line may fail to run as a service if it depends
	/// on a certain environment (e.g., the PATH environment variable).
	///
	/// An application registered as a Windows service can be started
	/// with the NET START <name> command and stopped with the NET STOP <name>
	/// command. Alternatively, the Services MMC applet can be used.
	///
	/// On Unix platforms, an application built on top of the OSSServerApplication
	/// class can be optionally run as a daemon by giving the --daemon
	/// command line option. A daemon, when launched, immediately
	/// forks off a background process that does the actual work. After launching
	/// the background process, the foreground process exits.
	///
	/// After the initialization is complete, but before entering the main() method,
	/// the current working directory for the daemon process is changed to the root
	/// directory ("/"), as it is common practice for daemon processes. Therefore, be
	/// careful when working with files, as relative paths may not point to where
	/// you expect them point to.
	///
	/// An application can determine whether it is running as a daemon by checking
	/// for the "application.runAsDaemon" configuration property.
	///
	///     if (config().getBool("application.runAsDaemon", false))
	///     {
	///         // do daemon specific things
	///     }
	///
	/// When running as a daemon, specifying the --pidfile option (e.g.,
	/// --pidfile=/var/run/sample.pid) may be useful to record the process ID of
	/// the daemon in a file. The PID file will be removed when the daemon process
	/// terminates (but not, if it crashes).
{
public:
	OSSServerApplication();
		/// Creates the OSSServerApplication.

	~OSSServerApplication();
		/// Destroys the OSSServerApplication.

	bool isInteractive() const;
		/// Returns true if the application runs from the command line.
		/// Returns false if the application runs as a Unix daemon
		/// or Windows service.

	int run(int argc, char** argv);
		/// Runs the application by performing additional initializations
		/// and calling the main() method.

#if defined(POCO_WIN32_UTF8) && !defined(POCO_NO_WSTRING)
	int run(int argc, wchar_t** argv);
		/// Runs the application by performing additional initializations
		/// and calling the main() method.
		///
		/// This Windows-specific version of init is used for passing
		/// Unicode command line arguments from wmain().
#endif

protected:
	int run();
	void waitForTerminationRequest();
	void defineOptions(OptionSet& options);
	void handleOption(const std::string& name, const std::string& value);
	static void terminate();

private:
#if defined(POCO_OS_FAMILY_UNIX)
	bool isDaemon(int argc, char** argv);
	void beDaemon();
#elif defined(POCO_OS_FAMILY_WINDOWS)
	enum Action
	{
		SRV_RUN,
		SRV_REGISTER,
		SRV_UNREGISTER
	};
	static BOOL __stdcall ConsoleCtrlHandler(DWORD ctrlType);
	static void __stdcall ServiceControlHandler(DWORD control);
#if defined(POCO_WIN32_UTF8) && !defined(POCO_NO_WSTRING)
	static void __stdcall ServiceMain(DWORD argc, LPWSTR* argv);
#else
	static void __stdcall ServiceMain(DWORD argc, LPTSTR* argv);
#endif

	bool hasConsole();
	bool isService();
	void beService();
	void registerService();
	void unregisterService();

	Action      _action;
	std::string _displayName;

	static Poco::Event           _terminated;
	static SERVICE_STATUS        _serviceStatus;
	static SERVICE_STATUS_HANDLE _serviceStatusHandle;
#endif
};

#if defined(POCO_OS_FAMILY_WINDOWS)
Poco::Event           OSSServerApplication::_terminated;
SERVICE_STATUS        OSSServerApplication::_serviceStatus;
SERVICE_STATUS_HANDLE OSSServerApplication::_serviceStatusHandle = 0;
#endif


OSSServerApplication::OSSServerApplication()
{
#if defined(POCO_OS_FAMILY_WINDOWS)
	_action = SRV_RUN;
	std::memset(&_serviceStatus, 0, sizeof(_serviceStatus));
#endif
}


OSSServerApplication::~OSSServerApplication()
{
}


bool OSSServerApplication::isInteractive() const
{
	bool runsInBackground = config().getBool("application.runAsDaemon", false) || config().getBool("application.runAsService", false);
	return !runsInBackground;
}


int OSSServerApplication::run()
{
	return OSSApplication::run();
}


void OSSServerApplication::terminate()
{
	Process::requestTermination(Process::id());
}


#if defined(POCO_OS_FAMILY_WINDOWS)


//
// Windows specific code
//
BOOL OSSServerApplication::ConsoleCtrlHandler(DWORD ctrlType)
{
	switch (ctrlType)
	{
	case CTRL_C_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_BREAK_EVENT:
		terminate();
		return _terminated.tryWait(10000) ? TRUE : FALSE;
	default:
		return FALSE;
	}
}


void OSSServerApplication::ServiceControlHandler(DWORD control)
{
	switch (control)
	{
	case SERVICE_CONTROL_STOP:
	case SERVICE_CONTROL_SHUTDOWN:
		terminate();
		_serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
		break;
	case SERVICE_CONTROL_INTERROGATE:
		break;
	}
	SetServiceStatus(_serviceStatusHandle,  &_serviceStatus);
}


#if defined(POCO_WIN32_UTF8) && !defined(POCO_NO_WSTRING)
void OSSServerApplication::ServiceMain(DWORD argc, LPWSTR* argv)
#else
void OSSServerApplication::ServiceMain(DWORD argc, LPTSTR* argv)
#endif
{
	OSSServerApplication& app = static_cast<OSSServerApplication&>(OSSApplication::instance());

	app.config().setBool("application.runAsService", true);

#if defined(POCO_WIN32_UTF8) && !defined(POCO_NO_WSTRING)
	_serviceStatusHandle = RegisterServiceCtrlHandlerW(L"", ServiceControlHandler);
#else
	_serviceStatusHandle = RegisterServiceCtrlHandler("", ServiceControlHandler);
#endif
	if (!_serviceStatusHandle)
		throw SystemException("cannot register service control handler");

	_serviceStatus.dwServiceType             = SERVICE_WIN32;
	_serviceStatus.dwCurrentState            = SERVICE_START_PENDING;
	_serviceStatus.dwControlsAccepted        = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	_serviceStatus.dwWin32ExitCode           = 0;
	_serviceStatus.dwServiceSpecificExitCode = 0;
	_serviceStatus.dwCheckPoint              = 0;
	_serviceStatus.dwWaitHint                = 0;
	SetServiceStatus(_serviceStatusHandle, &_serviceStatus);

	try
	{
#if defined(POCO_WIN32_UTF8) && !defined(POCO_NO_WSTRING)
		std::vector<std::string> args;
		for (DWORD i = 0; i < argc; ++i)
		{
			std::string arg;
			Poco::UnicodeConverter::toUTF8(argv[i], arg);
			args.push_back(arg);
		}
		app.init(args);
#else
		app.init(argc, argv);
#endif
		_serviceStatus.dwCurrentState = SERVICE_RUNNING;
		SetServiceStatus(_serviceStatusHandle, &_serviceStatus);
		int rc = app.run();
		_serviceStatus.dwWin32ExitCode           = rc ? ERROR_SERVICE_SPECIFIC_ERROR : 0;
		_serviceStatus.dwServiceSpecificExitCode = rc;
	}
	catch (Exception& exc)
	{
		app.logger().log(exc);
		_serviceStatus.dwWin32ExitCode           = ERROR_SERVICE_SPECIFIC_ERROR;
		_serviceStatus.dwServiceSpecificExitCode = EXIT_CONFIG;
	}
	catch (...)
	{
		app.logger().error("fatal error - aborting");
		_serviceStatus.dwWin32ExitCode           = ERROR_SERVICE_SPECIFIC_ERROR;
		_serviceStatus.dwServiceSpecificExitCode = EXIT_SOFTWARE;
	}
	try
	{
		app.uninitialize();
	}
	catch (...)
	{
	}
	_serviceStatus.dwCurrentState = SERVICE_STOPPED;
	SetServiceStatus(_serviceStatusHandle, &_serviceStatus);
}


void OSSServerApplication::waitForTerminationRequest()
{
	SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
	std::string evName("POCOTRM");
	NumberFormatter::appendHex(evName, Process::id(), 8);
	NamedEvent ev(evName);
	ev.wait();
	_terminated.set();
}


int OSSServerApplication::run(int argc, char** argv)
{
	if (!hasConsole() && isService())
	{
		return 0;
	}
	else
	{
		int rc = EXIT_OK;
		try
		{
			init(argc, argv);
			switch (_action)
			{
			case SRV_REGISTER:
				registerService();
				rc = EXIT_OK;
				break;
			case SRV_UNREGISTER:
				unregisterService();
				rc = EXIT_OK;
				break;
			default:
				rc = run();
				uninitialize();
			}
		}
		catch (Exception& exc)
		{
			logger().log(exc);
			rc = EXIT_SOFTWARE;
		}
		return rc;
	}
}


#if defined(POCO_WIN32_UTF8) && !defined(POCO_NO_WSTRING)
int OSSServerApplication::run(int argc, wchar_t** argv)
{
	if (!hasConsole() && isService())
	{
		return 0;
	}
	else
	{
		int rc = EXIT_OK;
		try
		{
			init(argc, argv);
			switch (_action)
			{
			case SRV_REGISTER:
				registerService();
				rc = EXIT_OK;
				break;
			case SRV_UNREGISTER:
				unregisterService();
				rc = EXIT_OK;
				break;
			default:
				rc = run();
				uninitialize();
			}
		}
		catch (Exception& exc)
		{
			logger().log(exc);
			rc = EXIT_SOFTWARE;
		}
		return rc;
	}
}
#endif


bool OSSServerApplication::isService()
{
#if defined(POCO_WIN32_UTF8) && !defined(POCO_NO_WSTRING)
	SERVICE_TABLE_ENTRYW svcDispatchTable[2];
	svcDispatchTable[0].lpServiceName = L"";
	svcDispatchTable[0].lpServiceProc = ServiceMain;
	svcDispatchTable[1].lpServiceName = NULL;
	svcDispatchTable[1].lpServiceProc = NULL;
	return StartServiceCtrlDispatcherW(svcDispatchTable) != 0;
#else
	SERVICE_TABLE_ENTRY svcDispatchTable[2];
	svcDispatchTable[0].lpServiceName = "";
	svcDispatchTable[0].lpServiceProc = ServiceMain;
	svcDispatchTable[1].lpServiceName = NULL;
	svcDispatchTable[1].lpServiceProc = NULL;
	return StartServiceCtrlDispatcher(svcDispatchTable) != 0;
#endif
}


bool OSSServerApplication::hasConsole()
{
	HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	return  hStdOut != INVALID_HANDLE_VALUE && hStdOut != NULL;
}


void OSSServerApplication::registerService()
{
	std::string name = config().getString("application.baseName");
	std::string path = config().getString("application.path");

	WinService service(name);
	if (_displayName.empty())
		service.registerService(path);
	else
		service.registerService(path, _displayName);
	logger().information("The application has been successfully registered as a service");
}


void OSSServerApplication::unregisterService()
{
	std::string name = config().getString("application.baseName");

	WinService service(name);
	service.unregisterService();
	logger().information("The service has been successfully unregistered");
}


void OSSServerApplication::defineOptions(OptionSet& options)
{
	OSSApplication::defineOptions(options);

	options.addOption(
		Option("registerService", "", "register application as a service")
			.required(false)
			.repeatable(false));

	options.addOption(
		Option("unregisterService", "", "unregister application as a service")
			.required(false)
			.repeatable(false));

	options.addOption(
		Option("displayName", "", "specify a display name for the service (only with /registerService)")
			.required(false)
			.repeatable(false)
			.argument("name"));
}


void OSSServerApplication::handleOption(const std::string& name, const std::string& value)
{
	if (name == "registerService")
		_action = SRV_REGISTER;
	else if (name == "unregisterService")
		_action = SRV_UNREGISTER;
	else if (name == "displayName")
		_displayName = value;
	else
		OSSApplication::handleOption(name, value);
}


#elif defined(POCO_OS_FAMILY_UNIX)


//
// Unix specific code
//
void OSSServerApplication::waitForTerminationRequest()
{
	sigset_t sset;
	sigemptyset(&sset);
	sigaddset(&sset, SIGINT);
	sigaddset(&sset, SIGQUIT);
	sigaddset(&sset, SIGTERM);
	sigprocmask(SIG_BLOCK, &sset, NULL);
	int sig;
	sigwait(&sset, &sig);
  std::cout << "Termination Signal RECEIVED" << std::endl;
}


int OSSServerApplication::run(int argc, char** argv)
{
	
	try
	{
		init(argc, argv);
    #if 0
		if (runAsDaemon)
		{
			int rc = chdir("/");
			if (rc != 0) return EXIT_OSERR;
		}
    #endif
	}
	catch (Exception& exc)
	{
		logger().log(exc);
		return EXIT_CONFIG;
	}

  //bool runAsDaemon = true; //isDaemon(argc, argv);
	//if (runAsDaemon)
	//{
	//	beDaemon();
	//}
  
	int rc = run();
	try
	{
		uninitialize();
	}
	catch (Exception& exc)
	{
		logger().log(exc);
		rc = EXIT_CONFIG;
	}
	return rc;
}


bool OSSServerApplication::isDaemon(int argc, char** argv)
{

	std::string option("--daemon");
	for (int i = 1; i < argc; ++i)
	{
		if (option == argv[i])
			return true;
	}
	return false;

}


void OSSServerApplication::beDaemon()
{
	pid_t pid;
	if ((pid = fork()) < 0)
		throw SystemException("cannot fork daemon process");
	else if (pid != 0)
		exit(0);

  setpgrp();
  ::close(STDIN_FILENO);

	setsid();
	umask(0);

  #if 0
	// attach stdin, stdout, stderr to /dev/null
	// instead of just closing them. This avoids
	// issues with third party/legacy code writing
	// stuff to stdout/stderr.
	FILE* fin  = freopen("/dev/null", "r+", stdin);
	if (!fin) throw Poco::OpenFileException("Cannot attach stdin to /dev/null");
	FILE* fout = freopen("/dev/null", "r+", stdout);
	if (!fout) throw Poco::OpenFileException("Cannot attach stdout to /dev/null");
	FILE* ferr = freopen("/dev/null", "r+", stderr);
	if (!ferr) throw Poco::OpenFileException("Cannot attach stderr to /dev/null");
  #endif
}


void OSSServerApplication::defineOptions(OptionSet& options)
{
	OSSApplication::defineOptions(options);

	options.addOption(
		Option("daemon", "", "run application as a daemon")
			.required(false)
			.repeatable(false));

	options.addOption(
		Option("pidfile", "", "write PID to given file")
			.required(false)
			.repeatable(false)
			.argument("path"));
}


void OSSServerApplication::handleOption(const std::string& name, const std::string& value)
{
	if (name == "daemon")
	{
		config().setBool("application.runAsDaemon", true);
	}
	else if (name == "pidfile")
	{
		std::ofstream ostr(value.c_str());
		if (ostr.good())
			ostr << Poco::Process::id() << std::endl;
		else
			throw Poco::CreateFileException("Cannot write PID to file", value);
		Poco::TemporaryFile::registerForDeletion(value);
	}
	else OSSApplication::handleOption(name, value);
}


#elif defined(POCO_OS_FAMILY_VMS)


//
// VMS specific code
//
namespace
{
	static void handleSignal(int sig)
	{
		OSSServerApplication::terminate();
	}
}


void OSSServerApplication::waitForTerminationRequest()
{
	struct sigaction handler;
	handler.sa_handler = handleSignal;
	handler.sa_flags   = 0;
	sigemptyset(&handler.sa_mask);
	sigaction(SIGINT, &handler, NULL);
	sigaction(SIGQUIT, &handler, NULL);

	long ctrlY = LIB$M_CLI_CTRLY;
	unsigned short ioChan;
	$DESCRIPTOR(ttDsc, "TT:");

	lib$disable_ctrl(&ctrlY);
	sys$assign(&ttDsc, &ioChan, 0, 0);
	sys$qiow(0, ioChan, IO$_SETMODE | IO$M_CTRLYAST, 0, 0, 0, terminate, 0, 0, 0, 0, 0);
	sys$qiow(0, ioChan, IO$_SETMODE | IO$M_CTRLCAST, 0, 0, 0, terminate, 0, 0, 0, 0, 0);

	std::string evName("POCOTRM");
	NumberFormatter::appendHex(evName, Process::id(), 8);
	NamedEvent ev(evName);
	try
	{
		ev.wait();
    }
	catch (...)
	{
		// CTRL-C will cause an exception to be raised
	}
	sys$dassgn(ioChan);
	lib$enable_ctrl(&ctrlY);
}


int OSSServerApplication::run(int argc, char** argv)
{
	try
	{
		init(argc, argv);
	}
	catch (Exception& exc)
	{
		logger().log(exc);
		return EXIT_CONFIG;
	}
	int rc = run();
	try
	{
		uninitialize();
	}
	catch (Exception& exc)
	{
		logger().log(exc);
		rc = EXIT_CONFIG;
	}
	return rc;
}


void OSSServerApplication::defineOptions(OptionSet& options)
{
	Application::defineOptions(options);
}


void OSSServerApplication::handleOption(const std::string& name, const std::string& value)
{
	Application::handleOption(name, value);
}


#endif


} } // namespace Poco::Util

#include "Telnyx/Telnyx.h"
#include "Telnyx/UTL/Application.h"
#include "Telnyx/UTL/CoreUtils.h"
#include "Telnyx/UTL/Thread.h"

#include "Poco/Util/ServerApplication.h"
#include "Poco/Util/Option.h"
#include "Poco/Util/OptionSet.h"
#include "Poco/Util/HelpFormatter.h"
#include "Poco/Util/AbstractConfiguration.h"

#if TELNYX_OS_FAMILY_WINDOWS
#include <shellapi.h>
typedef int pid_t;
#define SIGKILL	9
#define SIGTERM	15
#else
#include <sys/types.h>
#include <signal.h>
#endif

using Poco::Util::ServerApplication;
using Poco::Util::Option;
using Poco::Util::OptionSet;
using Poco::Util::HelpFormatter;
using Poco::Util::AbstractConfiguration;
using Poco::Util::OptionCallback;


namespace Telnyx {

  
static pid_t write_pid_file(const char* pidFile, bool exclusive)
{
  int handle = open(pidFile, O_RDWR|O_CREAT, 0600);
  if (handle == -1)
  {
    return 0;
  }
  
  if (exclusive && lockf(handle,F_TLOCK,0) == -1)
  {
    return 0;
  }
  
  pid_t pid = getpid();
  
  char pidStr[10];
  sprintf(pidStr,"%d\n", pid);
  if (write(handle, pidStr, strlen(pidStr)) == -1)
  {
    pid = 0;
  }
  
  return pid;
}
  
class OSSApp : public Poco::Util::OSSServerApplication
{
public:

  OSSApp();

  ~OSSApp();
  	
  void initialize(OSSApplication& self);

  void uninitialize();

  void defineOptions(OptionSet& options);

  void handleHelp(const std::string& name, const std::string& value);

  void handlePID(const std::string& name, const std::string& value);

  int main(const std::vector<std::string>& args);

  void waitForTerminationSignal();

  void signalTermination();
  
  static OSSApp* _pAppInstance;
  static bool _helpRequested;
  static bool _isRunning;
  static std::string _logFile;
  static std::string _configFile;
  static std::string _pidFile;
  static bool _pidFileExclusive;
  static Poco::Logger* _pLogger;
  static Poco::Util::LayeredConfiguration* _pConfig;
  static app_exit_code _exitCode;
  static int _argc;
  static char** _argv;
  static char _internalArgv[10][256];

  //
  // Callback functions
  //
  static app_init_func _initHandler;
  static app_deinit_func _deinitHandler;
  static app_main_func _mainHandler;  
};

OSSApp* OSSApp::_pAppInstance;
bool OSSApp::_helpRequested = false;
bool OSSApp::_isRunning = false;
int OSSApp::_argc = 0;
char** OSSApp::_argv = 0;
std::string OSSApp::_logFile;
std::string OSSApp::_configFile;
std::string OSSApp::_pidFile;
bool OSSApp::_pidFileExclusive = true;
app_init_func OSSApp::_initHandler;
app_deinit_func OSSApp::_deinitHandler;
app_main_func OSSApp::_mainHandler; 
Poco::Logger* OSSApp::_pLogger = 0;
Poco::Util::LayeredConfiguration* OSSApp::_pConfig = 0;
app_exit_code OSSApp::_exitCode = Telnyx::APP_EXIT_OK;
char OSSApp::_internalArgv[10][256];

OSSApp::OSSApp()
{
  OSSApp::_pAppInstance = this;
}

OSSApp::~OSSApp()
{
}


void OSSApp::initialize(OSSApplication& self)
{
  Telnyx::TELNYX_init();

  boost::filesystem::path programPath= _argv[0];
  std::string fn = Telnyx::boost_file_name(programPath);
  
  // std::cerr << programPath << std::endl;
  
  #if TELNYX_OS_FAMILY_WINDOWS
    fn = Telnyx::string_left(fn, fn.size() - 4);
  #endif

  std::ostringstream defConfig;
  defConfig << fn << ".properties";
  std::ostringstream defLogger;
  defLogger << fn << ".log";

  if (!OSSApp::_configFile.empty())
  {
    loadConfiguration(OSSApp::_configFile);
  }
  else
  {
    loadConfiguration(); // load default configuration files, if present
  }
  
  OSSApp::_pConfig = &(config());
  OSSServerApplication::initialize(self);
  _pLogger = &(logger());

  //{
  //  std::ostringstream msg;
  //  msg << "Starting up " << fn << " with PID=" << getpid();
	//  logger().information(msg.str());
  //}
  
  {   

    std::string pidFile;
    if (OSSApp::_pidFile.empty())
    {
      std::ostringstream pfile;
      pfile << fn << ".pid";
      pidFile = config().getString("process.pidFile", pfile.str());
    }else
    {
      pidFile = OSSApp::_pidFile;
    }
    
    if (!OSSApp::_pidFile.empty())
    {
      if (!write_pid_file(OSSApp::_pidFile.c_str(), OSSApp::_pidFileExclusive))
      {
        //
        // Unable to create a PID file.
        //
        if (OSSApp::_pidFileExclusive)
        {
          std::cerr << "Unable to grab exclusive lock for " << OSSApp::_pidFile << std::endl;
        }
        else
        {
          std::cerr << "Unable to create PID file " << OSSApp::_pidFile << std::endl;
        }
        _exit(-1);
      }
    }
  }


  std::string libLogger = config().getString("library.logger.path", defLogger.str());
  std::string libLoggerPattern = config().getString("library.logger.pattern", "%h-%M-%S.%i: %t");
  std::string libLoggerPriority = config().getString("library.logger.priority", "information");
  std::string libLoggerCompress = config().getString("library.logger.compress", "true");
  std::string libLoggerPurgeCount = config().getString("library.logger.purgeCount", "7");
  std::string libLoggerPriorityCache = config().getString("library.logger.priority.cache", "");
  std::string libLoggerTimes = config().getString("library.logger.times.cache", "");

  if (!libLoggerPriorityCache.empty())
  {
    if (boost::filesystem::exists(libLoggerPriorityCache.c_str()))
    {
      std::ifstream cacheFile(libLoggerPriorityCache.c_str());
      if (cacheFile.is_open())
      {
        cacheFile >> libLoggerPriority;
      }
    }
  }

  try
  {
    boost::filesystem::path logFile(libLogger);
    if (!boost::filesystem::exists(logFile))
    {
      boost::filesystem::path logDir(logFile.parent_path());
      if( !boost::filesystem::exists(logDir))
        boost::filesystem::create_directories(logDir);
    }
  }
  catch(...){}

  Telnyx::LogPriority logPriority = Telnyx::PRIO_INFORMATION;

  if ("fatal" == libLoggerPriority)
    logPriority = Telnyx::PRIO_FATAL;
  else if ("critical" == libLoggerPriority)
    logPriority = Telnyx::PRIO_CRITICAL;
  else if ("error" == libLoggerPriority)
    logPriority = Telnyx::PRIO_ERROR;
  else if ("warning" == libLoggerPriority)
    logPriority = Telnyx::PRIO_WARNING;
  else if ("notice" == libLoggerPriority)
    logPriority = Telnyx::PRIO_NOTICE;
  else if ("information" == libLoggerPriority)
    logPriority = Telnyx::PRIO_INFORMATION;
  else if ("debug" == libLoggerPriority)
    logPriority = Telnyx::PRIO_DEBUG;
  else if ("trace" == libLoggerPriority)
    logPriority = Telnyx::PRIO_TRACE;

  Telnyx::logger_init(libLogger, logPriority, libLoggerPattern, libLoggerCompress, libLoggerPurgeCount, libLoggerTimes);

  if (_initHandler)
    _initHandler();
}

void OSSApp::uninitialize()
{
  std::ostringstream msg;
  msg << "Shutting down Process PID=" << Poco::Process::id();
	logger().information(msg.str());

	OSSServerApplication::uninitialize();
  if (_deinitHandler)
    _deinitHandler();

  Telnyx::logger_deinit();
  Telnyx::TELNYX_deinit();
}

void OSSApp::defineOptions(OptionSet& options)
{
}

void OSSApp::handleHelp(const std::string& name, const std::string& value)
{
  _helpRequested = true;
 #if 0
  HelpFormatter helpFormatter(options());
	helpFormatter.setCommand(commandName());
	helpFormatter.setUsage("OPTIONS");
	helpFormatter.setHeader("OSS DHT Service");
	helpFormatter.format(std::cout);
 #endif
	stopOptionsProcessing();
}

int OSSApp::main(const std::vector<std::string>& args)
{
  return _mainHandler(args);
}

void OSSApp::waitForTerminationSignal()
{
  waitForTerminationRequest();
}

void OSSApp::signalTermination()
{
  terminate();
}

//
// Start of OSS API functions
//

void app_set_main_handler(app_main_func handler)
{
  OSSApp::_mainHandler = handler;
}

void app_set_init_handler(app_init_func handler)
{
  OSSApp::_initHandler = handler;
}

void app_set_deinit_handler(app_deinit_func handler)
{
  OSSApp::_deinitHandler = handler;
}

int app_run(int argc, char** argv)
{
  TELNYX_VERIFY(OSSApp::_mainHandler);
  TELNYX_VERIFY(!OSSApp::_isRunning);
  OSSApp app;
  app._argc = argc;
  app._argv = argv;

  int retVal = app.run(1, argv);
  return retVal;
}

int app_get_argc()
{
  return OSSApp::_argc;
}

bool app_is_running()
{
  return OSSApp::_isRunning;
}

void app_wait_for_termination_request()
{
  OSSApp::_pAppInstance->waitForTerminationSignal();
}

void app_terminate()
{
  OSSApp::_pAppInstance->signalTermination();
}
  
void app_set_logger_file(const std::string& path)
{
  OSSApp::_logFile = path;
}

void app_set_config_file(const std::string& path)
{
  OSSApp::_configFile = path;
}

void app_set_pid_file(const std::string& path, bool exclusive)
{
  OSSApp::_pidFile = path;
  OSSApp::_pidFileExclusive = exclusive;
}

unsigned long app_get_pid()
{
  return Poco::Process::id();
}

void app_log_reset_level(LogPriority level)
{
  TELNYX_VERIFY_NULL(OSSApp::_pLogger);
  OSSApp::_pLogger->setLevel(level);
}

void app_log(const std::string& log, LogPriority priority)
{
  TELNYX_VERIFY_NULL(OSSApp::_pLogger);
  switch (priority)
  {
  case PRIO_FATAL :
    log_fatal(log);
    break;
  case PRIO_CRITICAL :
    log_critical(log);
    break;
  case PRIO_ERROR :
    log_error(log);
    break;
  case PRIO_WARNING :
    log_warning(log);
    break;
  case PRIO_NOTICE :
    log_notice(log);
    break;
  case PRIO_INFORMATION :
    log_information(log);
    break;
  case PRIO_DEBUG :
    log_debug(log);
    break;
  case PRIO_TRACE :
    log_trace(log);
    break;
  default:
    TELNYX_VERIFY(false);
  }
}

void app_log_fatal(const std::string& log)
{
  TELNYX_VERIFY_NULL(OSSApp::_pLogger);
  OSSApp::_pLogger->fatal(log);
}

void app_log_critical(const std::string& log)
{
  TELNYX_VERIFY_NULL(OSSApp::_pLogger);
  OSSApp::_pLogger->critical(log);
}

void app_log_error(const std::string& log)
{
  TELNYX_VERIFY_NULL(OSSApp::_pLogger);
  OSSApp::_pLogger->error(log);
}

void app_log_warning(const std::string& log)
{
  TELNYX_VERIFY_NULL(OSSApp::_pLogger);
  OSSApp::_pLogger->warning(log);
}

void app_log_notice(const std::string& log)
{
  TELNYX_VERIFY_NULL(OSSApp::_pLogger);
  OSSApp::_pLogger->notice(log);
}

void app_log_information(const std::string& log)
{
  TELNYX_VERIFY_NULL(OSSApp::_pLogger);
  OSSApp::_pLogger->information(log);
}

void app_log_debug(const std::string& log)
{
  TELNYX_VERIFY_NULL(OSSApp::_pLogger);
  OSSApp::_pLogger->debug(log);
}

void app_log_trace(const std::string& log)
{
  TELNYX_VERIFY_NULL(OSSApp::_pLogger);
  OSSApp::_pLogger->trace(log);
}

std::string app_config_get_string(const std::string& key) 
{
  TELNYX_VERIFY_NULL(OSSApp::_pConfig);
  return OSSApp::_pConfig->getString(key);
}
	
std::string app_config_get_string(const std::string& key, const std::string& defaultValue) 
{
  TELNYX_VERIFY_NULL(OSSApp::_pConfig);
  return OSSApp::_pConfig->getString(key, defaultValue);
}

std::string app_config_get_raw_string(const std::string& key) 
{
  TELNYX_VERIFY_NULL(OSSApp::_pConfig);
  return OSSApp::_pConfig->getRawString(key);
}
	
std::string app_config_get_raw_string(const std::string& key, const std::string& defaultValue) 
{
  TELNYX_VERIFY_NULL(OSSApp::_pConfig);
  return OSSApp::_pConfig->getRawString(key, defaultValue);
}
	
int app_config_get_int(const std::string& key) 
{
  TELNYX_VERIFY_NULL(OSSApp::_pConfig);
  return OSSApp::_pConfig->getInt(key);
}
	
int app_config_get_int(const std::string& key, int defaultValue) 
{
  TELNYX_VERIFY_NULL(OSSApp::_pConfig);
  return OSSApp::_pConfig->getInt(key, defaultValue);
}

double app_config_get_double(const std::string& key) 
{
  TELNYX_VERIFY_NULL(OSSApp::_pConfig);
  return OSSApp::_pConfig->getDouble(key);
}
	
double app_config_get_double(const std::string& key, double defaultValue) 
{
  TELNYX_VERIFY_NULL(OSSApp::_pConfig);
  return OSSApp::_pConfig->getDouble(key, defaultValue);
}

bool app_config_get_bool(const std::string& key) 
{
  TELNYX_VERIFY_NULL(OSSApp::_pConfig);
  return OSSApp::_pConfig->getBool(key);
}
	
bool app_config_get_bool(const std::string& key, bool defaultValue) 
{
  TELNYX_VERIFY_NULL(OSSApp::_pConfig);
  return OSSApp::_pConfig->getBool(key, defaultValue);
}
	
void app_config_set_string(const std::string& key, const std::string& value)
{
  TELNYX_VERIFY_NULL(OSSApp::_pConfig);
   OSSApp::_pConfig->setString(key, value);
}
	
void app_config_set_int(const std::string& key, int value)
{
  TELNYX_VERIFY_NULL(OSSApp::_pConfig);
  OSSApp::_pConfig->setInt(key, value);
}

void app_config_set_double(const std::string& key, double value)
{
  TELNYX_VERIFY_NULL(OSSApp::_pConfig);
  OSSApp::_pConfig->setDouble(key, value);
}

void app_config_set_bool(const std::string& key, bool value)
{
  TELNYX_VERIFY_NULL(OSSApp::_pConfig);
  OSSApp::_pConfig->setBool(key, value);
}

void app_set_exit_code(app_exit_code code)
{
  OSSApp::_exitCode = code;
}

app_exit_code app_get_exit_code()
{
  return OSSApp::_exitCode;
}

bool app_process_kill(int pid, int sig)
{
#if TELNYX_OS_FAMILY_WINDOWS
  HANDLE hProc;
  hProc=OpenProcess( PROCESS_TERMINATE|PROCESS_QUERY_INFORMATION|PROCESS_VM_READ, FALSE,pid );
  if(hProc)
	{
		if(TerminateProcess(hProc,0))
		{
			// process terminated
			return CloseHandle(hProc) != FALSE;
		}
    return CloseHandle(hProc) != FALSE;
  }
  return false;
#else
  kill(pid, sig);
#endif
  return true;
}

bool app_is_process_exist(int pid)
{
#if TELNYX_OS_FAMILY_WINDOWS
  HANDLE hProc;
  hProc=OpenProcess( PROCESS_QUERY_INFORMATION|PROCESS_VM_READ, FALSE,pid );
  if(hProc)
    return CloseHandle(hProc) != FALSE;
  return false;
#else
  return kill( pid, 0 ) == 0; 
#endif
}

bool app_shell_execute(const std::string& app, const std::string& args, bool hidden)
{
#if TELNYX_OS_FAMILY_WINDOWS
  TELNYX_VERIFY(false);
  return false;
#else
  std::ostringstream cmd;
  cmd << app << " " << args;
  return system(cmd.str().c_str()) != -1;
#endif
}

bool app_shell_command(const std::string& command, std::string& result)
{
#ifndef TELNYX_OS_FAMILY_WINDOWS
  FILE *fd = popen( command.c_str(), "r" );
  result.reserve(1024);
  if (!fd)
      return false;
  while (true)
  {
    int c = fgetc(fd);
    if (c != EOF)
    {
      result.push_back((char)c);
    }
    else
    {
      pclose(fd);
      break;
    }
  }
  
  return true;
#else
  return false;
#endif
 
}

std::string app_environment_get(const std::string& name)
{
  return Poco::Environment::get(name);
}

std::string app_environment_get(const std::string& name, const std::string& defaultValue)
{
  return Poco::Environment::get(name, defaultValue);
}

bool app_environment_has(const std::string& name)
{
  return Poco::Environment::has(name);
}

void app_environment_set(const std::string& name, const std::string& value)
{
  Poco::Environment::set(name, value);
}

std::string app_environment_os_name()
{
  return Poco::Environment::osName();
}

std::string app_environment_os_version()
{
  return Poco::Environment::osVersion();
}

std::string app_environment_os_architecture()
{
  return Poco::Environment::osArchitecture();
}

std::string app_environment_node_name()
{
  return Poco::Environment::nodeName();
}

std::string app_environment_node_id()
{
  try
  {
    return Poco::Environment::nodeId();
  }
  catch(...)
  {
    return "";
  }
}

unsigned app_environment_processor_count()
{
  return Poco::Environment::processorCount();
}

} // OSS
