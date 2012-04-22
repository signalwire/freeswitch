open O, ">.indent.pro";
select O;

while (<>) {
  open I, $_;
  while(<I>) {
    if (/([\w\d]+_t)[\s\)]/) {
      ##print "-T $1 ";
      $h{$1}++;
    }
  }
  close I;
}

print "-brs -sai -npsl -di0 -br -ce -d0 -cli0 -npcs -nfc1 -ut -i4 -ts4 -l155 -cs ";

foreach (keys %h) {
  print "-T $_ ";
}

close O;
