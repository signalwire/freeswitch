cmake_minimum_required(VERSION 2.6)

add_definition(-DSCCSID)
add_definition(-DHAVE_CONFIG_H)
add_definition(-DPIC)


INCLUDE_DIRECTORIES(include/ ./ src/) 

SET ( libedit_SRCS src/filecomplete.c src/key.c src/parse.h src/refresh.c src/sig.h src/tty.c src/el.c src/filecomplete.h src/key.h src/prompt.c src/refresh.h src/strlcat.c src/tty.h src/el.h src/hist.c src/prompt.h src/search.c src/strlcpy.c src/unvis.c src/chared.c src/el_term.h src/hist.h src/map.c src/read.c src/search.h src/sys.h src/vi.c src/chared.h src/emacs.c src/histedit.h src/map.h src/read.h src/term.c src/vis.c src/common.c src/fgetln.c src/history.c src/parse.c src/readline.c src/sig.c src/tokenizer.c src/vis.h)

ADD_LIBRARY(libedit STATIC ${libedit_SRCS})







