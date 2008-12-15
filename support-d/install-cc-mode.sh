version=5.31.3
wget http://www.freeswitch.org/eg/cc-mode-${version}.tar.gz
tar -zxvf cc-mode-${version}.tar.gz
cd cc-mode-${version}
emacs -batch -no-site-file -q -f batch-byte-compile cc-*.el

if [ -d /usr/share/emacs ] ; then
target=/usr/share/emacs/site-lisp
else
target=/usr/local/share/emacs/site-lisp
fi

cp *.elc $target
wget http://www.freeswitch.org/eg/nxml-mode-20041004.tar.gz
tar -zxvf nxml-mode-20041004.tar.gz
cd nxml-mode-20041004
cp *.el* $target

