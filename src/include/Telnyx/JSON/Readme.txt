CAJUN* is a C++ API for the JSON object interchange format. JSON is like XML, except it doesn't suck**. It is specifically designed for representing (in plain text format) structures familiar to software engineers: booleans, numerics, strings, arrays, and objects (i.e. name/value pairs, associative array, etc.); it humbly leaves text markup to XML. It is ideal for storing persistent application data, such as configuration or user data files. 

Too many JSON parsers I've seen suffer from overly complex designs and confusing interfaces, so in true software engineer form, I thought I could do better. The goal of JSON was to create an simple, "minimalist" interface while sacrificing absolutely no power or flexibility. The STL containers, while not without their violations of that spirit, served as an inspiration. The end result is (IMHO) an interface that should be immediately intuitive to anyone familiar with C++ and the Standard Library containers. It can best be described as working with an "element", where an element may consist of:
* String (mimics std::string)
* Numeric (double)
* Boolean (bool)
* Array (std::vector<UnknownElement>)
* Object (unsorted std::map<std::string, UnknownElement>)
* UnknownElement - like boost::any, but restricted to types below. Used to aggregate elements within Objects & Arrays, and for reading documents of unknown structure

As with any design, sacrifices were made with CAJUN. Most situations I've encountered where JSON is well-suited (reading & writing application configuration and data files) are not typically performance bottlenecks, so simplicity, safety & flexibility were favored over raw speed. The end result is a library with simple, typesafe classes, no memory-management burden on the user, and exception-based error reporting.

* C++ API for JSON. A pint on me for who ever comes up with a good meaning for "UN".
** To be fair, XML doesn't suck intentionally, it is just often used inappropriately.