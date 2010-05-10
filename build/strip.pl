#!/usr/bin/perl
while(<>) {
  s/SWITCH_DECLARE_CONSTRUCTOR//g;
  s/SWITCH_DECLARE[^\(]*\((.*?)\)/$1/g;
  print;
}
