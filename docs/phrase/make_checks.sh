#!/bin/sh
SAXON_JAR=/usr/share/java/saxon9he.jar
XML=$1

if  [ ! -f "$SAXON_JAR" ]; then
	echo "Please update 'SAXON_JAR' variable value to location of SAXON jar"
	exit -1
fi

if [ -z "$XML" ]; then
	echo "Error: Please enter xml file name that must be ckecked."
	echo "Example: make_checks.sh phrase_es_ES.xml > comparison_result.xml"
	exit -1
fi

java -jar "$SAXON_JAR" -xsl:make_checks.xslt -t "$XML"

