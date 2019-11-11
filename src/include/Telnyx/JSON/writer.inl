/**********************************************

License: BSD
Project Webpage: http://cajun-jsonapi.sourceforge.net/
Author: Terry Caton

***********************************************/

#include "writer.h"
#include <iostream>
#include <iomanip>

/*  

TODO:
* better documentation
* unicode character encoding

*/

namespace json
{


inline void Writer::Write(const UnknownElement& elementRoot, std::ostream& ostr) { Write_i(elementRoot, ostr); }
inline void Writer::Write(const Object& object, std::ostream& ostr)              { Write_i(object, ostr); }
inline void Writer::Write(const Array& array, std::ostream& ostr)                { Write_i(array, ostr); }
inline void Writer::Write(const Number& number, std::ostream& ostr)              { Write_i(number, ostr); }
inline void Writer::Write(const String& string, std::ostream& ostr)              { Write_i(string, ostr); }
inline void Writer::Write(const Boolean& boolean, std::ostream& ostr)            { Write_i(boolean, ostr); }
inline void Writer::Write(const Null& null, std::ostream& ostr)                  { Write_i(null, ostr); }


inline Writer::Writer(std::ostream& ostr) :
   m_ostr(ostr),
   m_nTabDepth(0)
{}

template <typename ElementTypeT>
void Writer::Write_i(const ElementTypeT& element, std::ostream& ostr)
{
   Writer writer(ostr);
   writer.Write_i(element);
   ostr.flush(); // all done
}

inline void Writer::Write_i(const Array& array)
{
   if (array.Empty())
      m_ostr << "[]";
   else
   {
      m_ostr << '[' << std::endl;
      ++m_nTabDepth;

      Array::const_iterator it(array.Begin()),
                            itEnd(array.End());
      while (it != itEnd) {
         m_ostr << std::string(m_nTabDepth, '\t');
         
         Write_i(*it);

         if (++it != itEnd)
            m_ostr << ',';
         m_ostr << std::endl;
      }

      --m_nTabDepth;
      m_ostr << std::string(m_nTabDepth, '\t') << ']';
   }
}

inline void Writer::Write_i(const Object& object)
{
   if (object.Empty())
      m_ostr << "{}";
   else
   {
      m_ostr << '{' << std::endl;
      ++m_nTabDepth;

      Object::const_iterator it(object.Begin()),
                             itEnd(object.End());
      while (it != itEnd) {
         m_ostr << std::string(m_nTabDepth, '\t') << '"' << it->name << "\" : ";
         Write_i(it->element); 

         if (++it != itEnd)
            m_ostr << ',';
         m_ostr << std::endl;
      }

      --m_nTabDepth;
      m_ostr << std::string(m_nTabDepth, '\t') << '}';
   }
}

inline void Writer::Write_i(const Number& numberElement)
{
   m_ostr << std::setprecision(20) << numberElement.Value();
}

inline void Writer::Write_i(const Boolean& booleanElement)
{
   m_ostr << (booleanElement.Value() ? "true" : "false");
}

inline void Writer::Write_i(const String& stringElement)
{
   m_ostr << '"';

   const std::string& s = stringElement.Value();
   std::string::const_iterator it(s.begin()),
                               itEnd(s.end());
   for (; it != itEnd; ++it)
   {
      switch (*it)
      {
         case '"':         m_ostr << "\\\"";   break;
         case '\\':        m_ostr << "\\\\";   break;
         case '\b':        m_ostr << "\\b";    break;
         case '\f':        m_ostr << "\\f";    break;
         case '\n':        m_ostr << "\\n";    break;
         case '\r':        m_ostr << "\\r";    break;
         case '\t':        m_ostr << "\\t";    break;
         //case '\u':        m_ostr << "";    break;  ??
         default:          m_ostr << *it;       break;
      }
   }

   m_ostr << '"';   
}

inline void Writer::Write_i(const Null& )
{
   m_ostr << "null";
}

inline void Writer::Write_i(const UnknownElement& unknown)
{
   unknown.Accept(*this); 
}

inline void Writer::Visit(const Array& array)       { Write_i(array); }
inline void Writer::Visit(const Object& object)     { Write_i(object); }
inline void Writer::Visit(const Number& number)     { Write_i(number); }
inline void Writer::Visit(const String& string)     { Write_i(string); }
inline void Writer::Visit(const Boolean& boolean)   { Write_i(boolean); }
inline void Writer::Visit(const Null& null)         { Write_i(null); }



} // End namespace
