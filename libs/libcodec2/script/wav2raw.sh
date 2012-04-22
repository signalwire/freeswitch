#!/bin/sh
# Converts wave files to raw (headerless) files
sox $1 -t raw $2
