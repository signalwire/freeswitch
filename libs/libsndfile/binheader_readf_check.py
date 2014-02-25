g#!/usr/bin/python

import re, string, sys

def trim_function_and_params (section):
	k = string.find (section, "(") + 1
	brackets = 1
	section_len = len (section)
	while k < section_len:
		if section [k] == '(':
			brackets += 1
		elif section [k] == ')':
			brackets -= 1
		if brackets < 1:
			return section [:k+1]
		k += 1
	print "Whoops!!!!"
	sys.exit (1)

def get_function_calls (filedata):
	filedata = string.split (filedata, "psf_binheader_readf")
	filedata = filedata [1:]

	func_calls = []
	for section in filedata:
		section = "psf_binheader_readf" + section
		func_calls.append (trim_function_and_params (section))

	return func_calls

def search_for_problems (filename):
	filedata = open (filename, "r").read ()
	
	if len (filedata) < 1:
		print "Error : file '%s' contains no data." % filename
		sys.exit (1) 
	
	count = 0
	
	calls = get_function_calls (filedata)
	for call in calls:
		if string.find (call, "sizeof") > 0:
			print "Found : ", call
			count += 1
	
	if count == 0:
		print "%-20s : No problems found." % filename
	else:
		print "\n%-20s : Found %d errors." % (filename, count)
		sys.exit (1)
	return 
	

#-------------------------------------------------------------------------------

if len (sys.argv) < 2:
	print "Usage : %s <file>" % sys.argv [0]
	sys.exit (1) 

for file in sys.argv [1:]:
	search_for_problems (file)

