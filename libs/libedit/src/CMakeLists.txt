cmake_minimum_required(VERSION 2.6)

INCLUDE_DIRECTORIES(../include/ ./ ) 

SET ( libedit_SRCS 

chared.c common.c el.c emacs.c hist.c key.c map.c parse.c 
prompt.c read.c refresh.c search.c sig.c term.c tty.c vi.c 
                                                        fgetln.c strlcat.c strlcpy.c unvis.c vis.c tokenizer.c 
                                                        history.c filecomplete.c readline.c chared.h el.h hist.h 
                                                        histedit.h key.h map.h parse.h prompt.h read.h refresh.h 
                                                        search.h sig.h sys.h el_term.h tty.h vis.h filecomplete.h 
                                                        editline/readline.h

)


ADD_LIBRARY(libedit STATIC ${libedit_SRCS})







