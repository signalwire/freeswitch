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

#include "rtpconfig.h"

#ifdef RTPDEBUG

#include "rtptypes.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct MemoryInfo
{
	void *ptr;
	size_t size;
	int lineno;
	char *filename;
	
	MemoryInfo *next;
};

class MemoryTracker
{
public:
	MemoryTracker() { firstblock = NULL; }
	~MemoryTracker()
	{
		MemoryInfo *tmp;
		int count = 0;
		
		printf("Checking for memory leaks...\n");fflush(stdout);
		while(firstblock)
		{
			count++;
			printf("Unfreed block %p of %d bytes (file '%s', line %d)\n",firstblock->ptr,(int)firstblock->size,firstblock->filename,firstblock->lineno);;
			
			tmp = firstblock->next;
			
			free(firstblock->ptr);
			if (firstblock->filename)
				free(firstblock->filename);
			free(firstblock);
			firstblock = tmp;
		}
		if (count == 0)
			printf("No memory leaks found\n");
		else
			printf("%d leaks found\n",count);
	}
	
	MemoryInfo *firstblock;	
};

static MemoryTracker memtrack;

void *donew(size_t s,char filename[],int line)
{	
	void *p;
	MemoryInfo *meminf;
	
	p = malloc(s);
	meminf = (MemoryInfo *)malloc(sizeof(MemoryInfo));
	
	meminf->ptr = p;
	meminf->size = s;
	meminf->lineno = line;
	meminf->filename = (char *)malloc(strlen(filename)+1);
	strcpy(meminf->filename,filename);
	meminf->next = memtrack.firstblock;
	
	memtrack.firstblock = meminf;
	
	return p;
}

void dodelete(void *p)
{
	MemoryInfo *tmp,*tmpprev;
	bool found;
	
	tmpprev = NULL;
	tmp = memtrack.firstblock;
	found = false;
	while (tmp != NULL && !found)
	{
		if (tmp->ptr == p)
			found = true;
		else
		{
			tmpprev = tmp;
			tmp = tmp->next;
		}
	}
	if (!found)
	{
		printf("Couldn't free block %p!\n",p);
		fflush(stdout);
	}
	else
	{
		MemoryInfo *n;
		
		fflush(stdout);
		n = tmp->next;
		free(tmp->ptr);
		if (tmp->filename)
			free(tmp->filename);
		free(tmp);
		
		if (tmpprev)
			tmpprev->next = n;
		else
			memtrack.firstblock = n;
	}
}

void *operator new(size_t s)
{
	return donew(s,"UNKNOWN FILE",0);
}

void *operator new[](size_t s)
{
	return donew(s,"UNKNOWN FILE",0);
}

void *operator new(size_t s,char filename[],int line)
{
	return donew(s,filename,line);
}

void *operator new[](size_t s,char filename[],int line)
{
	return donew(s,filename,line);
}

void operator delete(void *p)
{
	dodelete(p);
}

void operator delete[](void *p)
{
	dodelete(p);
}

#endif // RTPDEBUG

