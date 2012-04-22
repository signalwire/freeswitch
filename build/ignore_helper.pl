################################################################################
# ignore_helper.pl
# Copyright (c) 2007-2009 Anthony Minessale II <anthm@freeswitch.org>
# 
# Permission is hereby granted, free of charge, to any person
# obtaining a copy of this software and associated documentation
# files (the "Software"), to deal in the Software without
# restriction, including without limitation the rights to use,
# copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following
# conditions:
# 
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
# OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
# HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
# WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
# 
# Usage: cat <file with list of things to ignore (full path from trunk root) > | ignore_helper.pl
#
################################################################################

while (<>) {
  my $path = $_;
  my ($dir, $file) = $path =~ /(.*)\/([^\/]+)$/;
  if (!$dir) {
    $dir = ".";
    $file = $path;
  }

  my $props = $PROP_HASH{$dir};
  if (!$props) {
    my @prop_tmp = `svn propget svn:ignore $dir`;
    my @prop_tmp2;
    foreach (@prop_tmp) {
      $_ =~ s/[\r\n]//g;
      if ($_) {
	push @prop_tmp2, $_;
      }
    }
    $props = \@prop_tmp2;
      
    $PROP_HASH{$dir} = $props;
  }
  if ($props) {
    push @{$props}, "$file";
  }
}

foreach (keys %PROP_HASH) {
  my $dir = $_;
  my @list = @{$PROP_HASH{$dir}};
  my $path = $dir;
  $path =~ s/\//_/g;
  $path = "/tmp/$path.tmp";

  print "Setting Properties on $dir\n";
  
  open O, ">$path";
  foreach (@list) {
    print O "$_\n";
  }
  close O;
  my $cmd = "svn propset svn:ignore -F $path $dir";
  system($cmd);
  unlink($path);
}
