// json.cpp : Defines the entry point for the console application.
//


#include "Telnyx/JSON/reader.h"
#include "Telnyx/JSON/writer.h"
#include "Telnyx/JSON/elements.h"

#include <sstream>


int main()
{
   using namespace json;

   /* we'll generate: 
      {
         "Delicious Beers" : [
            {
               "Name" : "Schlafly American Pale Ale",
               "Origin" : "St. Louis, MO, USA",
               "ABV" : 5.9,
               "BottleConditioned" : true
            },
            {
               "Name" : "John Smith's Extra Smooth",
               "Origin" : "Tadcaster, Yorkshire, UK",
               "ABV" : 3.8,
               "Bottle Conditioned" : false
            }
         ]
      }
   */

   ////////////////////////////////////////////////////////////////////
   // construction

   // we can build a document piece by piece...
   Object objAPA;
   objAPA["Name"] = String("Schlafly American Pale Ale");
   objAPA["Origin"] = String("St. Louis, MO, USA");
   objAPA["ABV"] = Number(3.8);
   objAPA["BottleConditioned"] = Boolean(true);

   Array arrayBeer;
   arrayBeer.Insert(objAPA);

   Object objDocument;
   objDocument["Delicious Beers"] = arrayBeer;

   Number numDeleteThis = objDocument["AnotherMember"];

   // ...or, we can use UnknownElement's chaining child element access to quickly
   //  construct the remainder

   objDocument["Delicious Beers"][1]["Name"] = String("John Smith's Extra Smooth");
   objDocument["Delicious Beers"][1]["Origin"] = String("Tadcaster, Yorkshire, UK");
   objDocument["Delicious Beers"][1]["ABV"] = Number(3.8);
   objDocument["Delicious Beers"][1]["BottleConditioned"] = Boolean(false);

   
   ////////////////////////////////////////////////////////////////////
   // interpretation
      
   // perform all read operations on a const ref, otherwise we may end up
   //  manipulating the document instead of catching errors
   const Object& objRoot = objDocument;

   // the return type of Object::operator[string] & Array::operator[size_t] is UnknownElement, which 
   //  provides implicit casting to any of the other element types...
   const Array& arrayBeers = objRoot["Delicious Beers"];
   const Object& objBeer0 = arrayBeers[0];
   const String& strName0 = objBeer0["Name"];

   // ...it also supports operator[string] & operator[size_t] itself, which takes the implicit casting
   //  one step further. operator[string] implicitly casts to Object, and operator[size_t] to Array.
   //  the return value is another UnknownElement, so these operations can be strung together
   const Number numAbv1 = objRoot["Delicious Beers"][1]["ABV"];

   std::cout << "First beer name: " << strName0.Value() << std::endl;
   std::cout << "First beer ABV: " << numAbv1.Value() << std::endl;

   // we can also iterate through the child elements of an array or object, which is necessary
   //  when we don't know the structure of the document
   Array::const_iterator itBeers(arrayBeers.Begin()),
                         itBeersEnd(arrayBeers.End());
   for (; itBeers != itBeersEnd; ++itBeers)
   {
      // remember, *itArray is an UnknownElement, which can be implicitly cast to another element type
      const Object& objBeer = *itBeers;
      Object::const_iterator itBeerFacts(objBeer.Begin()),
                             itBeerFactsEnd(objBeer.End());
      for (; itBeerFacts != itBeerFactsEnd; ++itBeerFacts)
      {
         const Object::Member& member = *itBeerFacts;
         const std::string& name = member.name;
         const UnknownElement& element = member.element;

         // if we didn't know the structure of the itBeerFacts subtree, we could visit it
         // element.Accept(nonExistantVisitor);
      }
   }

   // everything's cool until we try to access a non-existent array element
   try
   {
      std::cout << "Expecting exception: Array out of bounds" << std::endl;
      const String& strName2 = arrayBeers[2];
   }
   catch (const Exception& e)
   {
      std::cout << "Caught json::Exception: " << e.what() << std::endl << std::endl;
   }

   // an exception will be thrown when expected data not found, since "Rice" is never a member of good beer
   try 
   {
      std::cout << "Expecting exception: Object member not found" << std::endl;
      const Boolean& boolRice = objRoot["Delicious Beers"][1]["Rice"];
   }
   catch (const Exception& e)
   {
      std::cout << "Caught json::Exception: " << e.what() << std::endl << std::endl;
   }

   // we'll also get an error if the document structure isn't quite what we expect
   try 
   {
      // objRoot["Delicious Beers"] is an Array, not another Object, so the second chained operator[string] will fail
      std::cout << "Expecting exception: Bad cast" << std::endl;
      const UnknownElement& elem = objRoot["Delicious Beers"]["Some Object Member"];
   }
   catch (json::Exception& e)
   {
      std::cout << "Caught json::Exception: " << e.what() << std::endl << std::endl;
   }


   ////////////////////////////////////////////////////////////////////
   // document deep copying
    
   // we can make an exact duplicate too
   Object objRoot2 = objRoot; 

   // the two documents should start out equal
   bool bEqualInitially = (objRoot == objRoot2);
   std::cout << "Document copies should start out equivalent. operator == returned: "
             << (bEqualInitially ? "true" : "false") << std::endl;

   // prove objRoot2 is a deep copy of objRoot:
   //  remove Beers[1]
   Array& array = objRoot2["Delicious Beers"];
   array.Erase(array.Begin()); // trim it down to one. this leaves elemRoot the same

   // the two documents should start out equal
   bool bEqualNow = (objRoot == objRoot2);
   std::cout << "Document copies should now be different. operator == returned: "
             << (bEqualNow ? "true" : "false") << std::endl << std::endl;


   ////////////////////////////////////////////////////////////////////
   // read/write sanity check

   // write it out to a string stream (file stream would work the same)....
   std::cout << "Writing file out...";

   std::stringstream stream;
   Writer::Write(objRoot, stream);

   // ...then read it back in. we know it's an Object
   std::cout << "then reading it back in." << std::endl;
   Object elemRootFile;
   Reader::Read(elemRootFile, stream);

   // still look right?
   bool bEquals = (objRoot == elemRootFile);
   std::cout << "Original document and streamed document should be equivalent. operator == returned: "
      << (bEquals ? "true" : "false") << std::endl << std::endl;


   ////////////////////////////////////////////////////////////////////
   // document read error handling

   // mis-predicting type type will fail with a parse error. we'll try reading an array into an object
   try
   {
      std::istringstream sBadDocument("[1, 2]"); // missing comma!
      std::cout << "Reading Object-based document into an Array; expecting Parse exception" << std::endl;
      Object objDocument;
      Reader::Read(objDocument, sBadDocument);
   }
   catch (Reader::ParseException& e)
   {
      // lines/offsets are zero-indexed, so bump them up by one for human presentation
      std::cout << "Caught json::ParseException: " << e.what() << ", Line/offset: " << e.m_locTokenBegin.m_nLine + 1
                << '/' << e.m_locTokenBegin.m_nLineOffset + 1 << std::endl << std::endl;
   }

   // reading in a slightly malformed document may result in a parse error
   try
   {
      std::istringstream sBadDocument("[1, 2 3]"); // missing comma!
      std::cout << "Reading malformed document; expecting Parse exception" << std::endl;
      Array arrayDocument;
      Reader::Read(arrayDocument, sBadDocument);
   }
   catch (Reader::ParseException& e)
   {
      std::cout << "Caught json::ParseException: " << e.what() << ", Line/offset: " << e.m_locTokenBegin.m_nLine + 1
                << '/' << e.m_locTokenBegin.m_nLineOffset + 1 << std::endl << std::endl;
   }

   // reading in gibberish will generate a scan error
   try
   {
      std::istringstream sBadDocument("[true, false, true, #.&@k*k4L!`1");
      std::cout << "Reading complete garbage; expecting Scan exception" << std::endl;
      Array arrayDocument;
      Reader::Read(arrayDocument, sBadDocument);
   }
   catch (Reader::ScanException& e)
   {
      std::cout << "Caught json::ScanException: " << e.what() << ", Line/offset: " << e.m_locError.m_nLine + 1
                << '/' << e.m_locError.m_nLineOffset + 1 << std::endl << std::endl;
   }

   return 0;
}


