/**********************************************

License: BSD
Project Webpage: http://cajun-jsonapi.sourceforge.net/
Author: Terry Caton

***********************************************/

#pragma once

#include "elements.h"

namespace json
{


class Visitor
{
public:
   virtual ~Visitor() {}

   virtual void Visit(Array& array) = 0;
   virtual void Visit(Object& object) = 0;
   virtual void Visit(Number& number) = 0;
   virtual void Visit(String& string) = 0;
   virtual void Visit(Boolean& boolean) = 0;
   virtual void Visit(Null& null) = 0;
};

class ConstVisitor
{
public:
   virtual ~ConstVisitor() {}

   virtual void Visit(const Array& array) = 0;
   virtual void Visit(const Object& object) = 0;
   virtual void Visit(const Number& number) = 0;
   virtual void Visit(const String& string) = 0;
   virtual void Visit(const Boolean& boolean) = 0;
   virtual void Visit(const Null& null) = 0;
};


} // End namespace
