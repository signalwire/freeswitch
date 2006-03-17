/*

  This file is a part of JRTPLIB
  Copyright (c) 1999-2006 Jori Liesenborgs

  Contact: jori@lumumba.uhasselt.be

  This library was developed at the "Expertisecentrum Digitale Media"
  (http://www.edm.uhasselt.be), a research center of the Hasselt University
  (http://www.uhasselt.be). The library is based upon work done for 
  my thesis at the School for Knowledge Technology (Belgium/The Netherlands).

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.

*/

#ifndef RTPHASHTABLE_H

#define RTPHASHTABLE_H

#include "rtperrors.h"

#ifdef RTPDEBUG
#include <iostream>
#endif // RTPDEBUG

template<class Element,int GetIndex(const Element &k),int hashsize>
class RTPHashTable
{
public:
	RTPHashTable();
	~RTPHashTable()						{ Clear(); }

	void GotoFirstElement()					{ curhashelem = firsthashelem; }
	void GotoLastElement()					{ curhashelem = lasthashelem; }
	bool HasCurrentElement()				{ return (curhashelem == 0)?false:true; }
	int DeleteCurrentElement();
	Element &GetCurrentElement()				{ return curhashelem->GetElement(); }
	int GotoElement(const Element &e);
	bool HasElement(const Element &e);
	void GotoNextElement();
	void GotoPreviousElement();
	void Clear();

	int AddElement(const Element &elem);
	int DeleteElement(const Element &elem);

#ifdef RTPDEBUG
	void Dump();
#endif // RTPDEBUG
private:
	class HashElement
	{
	public:
		HashElement(const Element &e,int index):element(e) { hashprev = 0; hashnext = 0; listnext = 0; listprev = 0; hashindex = index; }
		int GetHashIndex() 						{ return hashindex; }
		Element &GetElement()						{ return element; }
#ifdef RTPDEBUG
		void Dump()							{ std::cout << "\tHash index " << hashindex << " | Element " << element << std::endl; }
#endif // RTPDEBUG
	private:
		int hashindex;
		Element element;
	public:
		HashElement *hashprev,*hashnext;
		HashElement *listprev,*listnext;
	};

	HashElement *table[hashsize];
	HashElement *firsthashelem,*lasthashelem;
	HashElement *curhashelem;
};

template<class Element,int GetIndex(const Element &e),int hashsize>
inline RTPHashTable<Element,GetIndex,hashsize>::RTPHashTable()
{
	for (int i = 0 ; i < hashsize ; i++)
		table[i] = 0;
	firsthashelem = 0;
	lasthashelem = 0;
}

template<class Element,int GetIndex(const Element &e),int hashsize>
inline int RTPHashTable<Element,GetIndex,hashsize>::DeleteCurrentElement()
{
	if (curhashelem)
	{
		HashElement *tmp1,*tmp2;
		int index;
		
		// First, relink elements in current hash bucket
		
		index = curhashelem->GetHashIndex();
		tmp1 = curhashelem->hashprev;
		tmp2 = curhashelem->hashnext;
		if (tmp1 == 0) // no previous element in hash bucket
		{
			table[index] = tmp2;
			if (tmp2 != 0)
				tmp2->hashprev = 0;
		}
		else // there is a previous element in the hash bucket
		{
			tmp1->hashnext = tmp2;
			if (tmp2 != 0)
				tmp2->hashprev = tmp1;
		}

		// Relink elements in list
		
		tmp1 = curhashelem->listprev;
		tmp2 = curhashelem->listnext;
		if (tmp1 == 0) // curhashelem is first in list
		{
			firsthashelem = tmp2;
			if (tmp2 != 0)
				tmp2->listprev = 0;
			else // curhashelem is also last in list
				lasthashelem = 0;	
		}
		else
		{
			tmp1->listnext = tmp2;
			if (tmp2 != 0)
				tmp2->listprev = tmp1;
			else // curhashelem is last in list
				lasthashelem = tmp1;
		}
		
		// finally, with everything being relinked, we can delete curhashelem
		delete curhashelem;
		curhashelem = tmp2; // Set to next element in the list
	}
	else
		return ERR_RTP_HASHTABLE_NOCURRENTELEMENT;
	return 0;
}

template<class Element,int GetIndex(const Element &e),int hashsize>
inline int RTPHashTable<Element,GetIndex,hashsize>::GotoElement(const Element &e)
{
	int index;
	bool found;
	
	index = GetIndex(e);
	if (index >= hashsize)
		return ERR_RTP_HASHTABLE_FUNCTIONRETURNEDINVALIDHASHINDEX;
	
	curhashelem = table[index]; 
	found = false;
	while(!found && curhashelem != 0)
	{
		if (curhashelem->GetElement() == e)
			found = true;
		else
			curhashelem = curhashelem->hashnext;
	}
	if (!found)
		return ERR_RTP_HASHTABLE_ELEMENTNOTFOUND;
	return 0;
}

template<class Element,int GetIndex(const Element &e),int hashsize>
inline bool RTPHashTable<Element,GetIndex,hashsize>::HasElement(const Element &e)
{
	int index;
	bool found;
	HashElement *tmp;
	
	index = GetIndex(e);
	if (index >= hashsize)
		return false;
	
	tmp = table[index]; 
	found = false;
	while(!found && tmp != 0)
	{
		if (tmp->GetElement() == e)
			found = true;
		else
			tmp = tmp->hashnext;
	}
	return found;
}

template<class Element,int GetIndex(const Element &e),int hashsize>
inline void RTPHashTable<Element,GetIndex,hashsize>::GotoNextElement()
{
	if (curhashelem)
		curhashelem = curhashelem->listnext;
}

template<class Element,int GetIndex(const Element &e),int hashsize>
inline void RTPHashTable<Element,GetIndex,hashsize>::GotoPreviousElement()
{
	if (curhashelem)
		curhashelem = curhashelem->listprev;
}

template<class Element,int GetIndex(const Element &e),int hashsize>
inline void RTPHashTable<Element,GetIndex,hashsize>::Clear()
{
	HashElement *tmp1,*tmp2;
	
	for (int i = 0 ; i < hashsize ; i++)
		table[i] = 0;
	
	tmp1 = firsthashelem;
	while (tmp1 != 0)
	{
		tmp2 = tmp1->listnext;
		delete tmp1;
		tmp1 = tmp2;
	}
	firsthashelem = 0;
	lasthashelem = 0;
}

template<class Element,int GetIndex(const Element &e),int hashsize>
inline int RTPHashTable<Element,GetIndex,hashsize>::AddElement(const Element &elem)
{
	int index;
	bool found;
	HashElement *e,*newelem;
	
	index = GetIndex(elem);
	if (index >= hashsize)
		return ERR_RTP_HASHTABLE_FUNCTIONRETURNEDINVALIDHASHINDEX;
	
	e = table[index];
	found = false;
	while(!found && e != 0)
	{
		if (e->GetElement() == elem)
			found = true;
		else
			e = e->hashnext;
	}
	if (found)
		return ERR_RTP_HASHTABLE_ELEMENTALREADYEXISTS;
	
	// Okay, the key doesn't exist, so we can add the new element in the hash table
	
	newelem = new HashElement(elem,index);
	if (newelem == 0)
		return ERR_RTP_OUTOFMEM;

	e = table[index];
	table[index] = newelem;
	newelem->hashnext = e;
	if (e != 0)
		e->hashprev = newelem;
	
	// Now, we still got to add it to the linked list
	
	if (firsthashelem == 0)
	{
		firsthashelem = newelem;
		lasthashelem = newelem;
	}
	else // there already are some elements in the list
	{
		lasthashelem->listnext = newelem;
		newelem->listprev = lasthashelem;
		lasthashelem = newelem;
	}
	return 0;
}

template<class Element,int GetIndex(const Element &e),int hashsize>
inline int RTPHashTable<Element,GetIndex,hashsize>::DeleteElement(const Element &elem)
{
	int status;

	status = GotoElement(elem);
	if (status < 0)
		return status;
	return DeleteCurrentElement();
}

#ifdef RTPDEBUG
template<class Element,int GetIndex(const Element &e),int hashsize>
inline void RTPHashTable<Element,GetIndex,hashsize>::Dump()
{
	HashElement *e;
	
	std::cout << "DUMPING TABLE CONTENTS:" << std::endl;
	for (int i = 0 ; i < hashsize ; i++)
	{
		e = table[i];
		while (e != 0)
		{
			e->Dump();
			e = e->hashnext;
		}
	}
	
	std::cout << "DUMPING LIST CONTENTS:" << std::endl;
	e = firsthashelem;
	while (e != 0)
	{
		e->Dump();
		e = e->listnext;
	}
}
#endif // RTPDEBUG

#endif // RTPHASHTABLE_H

