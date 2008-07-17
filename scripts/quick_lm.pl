#!/usr/bin/perl

# NOTE: this is by no means an efficient implementation and performance will 
# deteriorate rapidly as a function of the corpus size. Larger corpora should be
# processed using the toolkit available at http://www.speech.cs.cmu.edu/SLM_info.html

# [2feb96] (air)
# cobbles together a language model from a set of exemplar sentences.
# features: 1) uniform discounting, 2) no cutoffs
# the "+" version allows insertion of extra words into the 1gram vector

# [27nov97] (air)
# bulletproof a bit for use in conjunction with a cgi script

# [20000711] (air)
# made visible the discount parmeter

# [20011123] (air)
# cleaned-up version for distribution

use Getopt::Std;

$VERBOSE = 1;

sub handler { local($sig) = @_;
	      print STDERR "quick_lm caught a SIG$sig -- dying\n";
	      exit(0);
	    }
foreach (qw(XCPU KILL TERM STOP)) { $SIG{$_} = \&handler; }


if ($#ARGV < 0) { die("usage: quick_lm -s <sentence_file> -o <output_file> [-w <word_file>] [-d discount]\n"); }
Getopt::Std::getopts("s:w:d:o:x");
$sentfile = $opt_s;
$wordfile = $opt_w;
$discount = $opt_d;
$output = $opt_o;

$output or die("No output file\n");
$sentfile or die("No sentence file\n");

$| = 1;  # always flush buffers

if ($VERBOSE>0) {print STDERR "Language model started at ",scalar localtime(),"\n";}


open(IN,"<$sentfile") or die("can't open $sentfile!\n");
if ($wordfile ne "") { open(WORDS,"$wordfile"); $wflag = 1;} else { $wflag = 0; }

$log10 = log(10.0);

if ($discount ne "") {
  if (($discount<=0.0) or ($discount>=1.0)) {
    print STDERR "\discount value out of range: must be 0.0 < x < 1.0! ...using 0.5\n";
    $discount_mass = 0.5;  # just use default
  } else {
    $discount_mass = $discount;
  }
} else {
  # Ben and Greg's experiments show that 0.5 is a way better default choice.
  $discount_mass = 0.5;  # Set a nominal discount...
}
$deflator = 1.0 - $discount_mass;

# create count tables
$sent_cnt = 0;
while (<IN>) {	 
  s/^\s*//; s/\s*$//;
  if ( $_ eq "" ) { next; } else { $sent_cnt++; } # skip empty lines
  @word = split(/\s/);    
  for ($j=0;$j<($#word-1);$j++) {	
    $trigram{join(" ",$word[$j],$word[$j+1],$word[$j+2])}++;
    $bigram{ join(" ",$word[$j],$word[$j+1])}++;
    $unigram{$word[$j]}++;
  }
  # finish up the bi and uni's at the end of the sentence...
  $bigram{join(" ",$word[$j],$word[$j+1])}++;
  $unigram{$word[$j]}++;
  
  $unigram{$word[$j+1]}++;
}
close(IN);
if ($VERBOSE) { print STDERR "$sent_cnt sentences found.\n"; }

# add in any words
if ($wflag) {
  $new = 0; $read_in = 0;
  while (<WORDS>) {
    s/^\s*//; s/\s*$//;
    if ( $_ eq "" ) { next; }  else { $read_in++; }  # skip empty lines
    if (! $unigram{$_}) { $unigram{$_} = 1; $new++; }
  }
  if ($VERBOSE) { print STDERR "tried to add $read_in word; $new were new words\n"; }
  close (WORDS);
}
if ( ($sent_cnt==0) && ($new==0) ) {
  print STDERR "no input?\n";
  exit;
}

open(LM,">$output") or die("can't open $myfile.lm for output!\n");

$preface = "";
$preface .= "Language model created by QuickLM on ".`date`;
$preface .= "Copyright (c) 1996-2002\nCarnegie Mellon University and Alexander I. Rudnicky\n\n";
$preface .= "This model based on a corpus of $sent_cnt sentences and ".scalar (keys %unigram). " words\n";
$preface .= "The (fixed) discount mass is $discount_mass\n\n";


# compute counts
$unisum = 0; $uni_count = 0; $bi_count = 0; $tri_count = 0;
foreach $x (keys(%unigram)) { $uni_count++; $unisum += $unigram{$x}; }
foreach $x (keys(%bigram))  { $bi_count++; }
foreach $x (keys(%trigram)) { $tri_count++; }

print LM $preface;
print LM "\\data\\\n";
print LM "ngram 1=$uni_count\n";
if ( $bi_count > 0 ) { print LM "ngram 2=$bi_count\n"; }
if ( $tri_count > 0 ) { print LM "ngram 3=$tri_count\n"; }
print LM "\n";

# compute uni probs
foreach $x (keys(%unigram)) {
  $uniprob{$x} = ($unigram{$x}/$unisum) * $deflator;
}

# compute alphas
foreach $y (keys(%unigram)) {
  $w1 = $y;
  $sum_denom = 0.0;
  foreach $x (keys(%bigram)) {
    if ( substr($x,0,rindex($x," ")) eq $w1 ) {
      $w2 = substr($x,index($x," ")+1);
      $sum_denom += $uniprob{$w2};
    }
  }
  $alpha{$w1} = $discount_mass / (1.0 - $sum_denom);
}

print LM "\\1-grams:\n";
foreach $x (sort keys(%unigram)) {
  printf LM "%6.4f %s %6.4f\n", log($uniprob{$x})/$log10, $x, log($alpha{$x})/$log10;
}
print LM "\n";

#compute bi probs
foreach $x (keys(%bigram)) {
  $w1 = substr($x,0,rindex($x," "));
  $biprob{$x} = ($bigram{$x}*$deflator)/$unigram{$w1};
}

#compute bialphas
foreach $x (keys(%bigram)) {
  $w1w2 = $x;
  $sum_denom = 0.0;
  foreach $y (keys(%trigram)) {
    if (substr($y,0,rindex($y," ")) eq $w1w2 ) {
      $w2w3 = substr($y,index($y," ")+1);
      $sum_denom += $biprob{$w2w3};
    }
  }
  $bialpha{$w1w2} = $discount_mass / (1.0 - $sum_denom);
}

# output the bigrams and trigrams (now that we have the alphas computed).
if ( $bi_count > 0 ) {
  print LM "\\2-grams:\n";
  foreach $x (sort keys(%bigram)) {
    printf LM "%6.4f %s %6.4f\n",
      log($biprob{$x})/$log10, $x, log($bialpha{$x})/$log10;
  }
  print LM "\n";
}

if ($tri_count > 0 ) {
  print LM "\\3-grams:\n";
  foreach $x (sort keys(%trigram)) {
    $w1w2 = substr($x,0,rindex($x," "));
    printf LM "%6.4f %s\n",
      log(($trigram{$x}*$deflator)/$bigram{$w1w2})/$log10, $x;
  }
  print LM "\n";
}

print LM "\\end\\\n";
close(LM);

if ($VERBOSE>0) { print STDERR "Language model completed at ",scalar localtime(),"\n"; }

#
__END__
=pod

/* ====================================================================
 * Copyright (c) 1996-2002 Alexander I. Rudnicky and Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All copies, used or distributed, must preserve the original wording of
 *    the copyright notice included in the output file.
 *
 * This work was supported in part by funding from the Defense Advanced 
 * Research Projects Agency and the CMU Sphinx Speech Consortium.
 *
 * THIS SOFTWARE IS PROVIDED BY CARNEGIE MELLON UNIVERSITY ``AS IS'' AND 
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
 * NOR ITS EMPLOYEES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ====================================================================
 *
 */


Pretty Good Language Modeler, now with unigram vector augmentation!

The Pretty Good Language Modeler is intended for quick construction of small 
language models, typically as might be needed in application development. Depending
on the version of Perl that you are running, a practical limitation is a
maximum vocabulary size on the order of 1000-2000 words. The limiting factor
is the number of n-grams observed, since each n-gram is stored as a hash key.
(So smaller vocabularies may turn out to be a problem as well.)

This package computes a stadard back-off language model. It differs in one significant 
respect, which is the computation of the discount. We adopt a "proportional" (or ratio)
discount in which a certain percentage of probability mass is removed (typically 50%)
from observed n-grams and redistributed over unobserved n-grams.

Conventionally, an absolute discount would be used, however we have found that the 
proportional discount appears to be robust for extremely small languages, as might be 
prototyped by a developer, as opposed to based on a collected corpus. We have found that 
absolute and proportional discounts produce comparable recognition results with perhaps
a slight advantage for proportional discounting. A more systematic investigation of
this technique would be desirable. In any case it also has the virtue of using a very
simple computation.

=end

