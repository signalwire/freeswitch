#if defined ( WIN32 )
# include <conio.h>
#else
# include <stdio.h>
# include <termios.h>
#endif

namespace ctb {

    char GetKey()
    {
#if defined ( WIN32 )
	   if(_kbhit()) {
		  return _getch();
	   }
	   return '\0';
#else
	   int ch;
	   static struct termios t, save_t;
	   tcgetattr(0,&t);
	   save_t = t;
	   t.c_lflag &= ~(ICANON);
	   t.c_cc[VMIN] = 0;
	   t.c_cc[VTIME] = 0;
	   tcsetattr(0,TCSANOW,&t);

	   ch = fgetc(stdin);

	   tcsetattr(0,TCSANOW,&save_t);
	   if(ch != EOF) {
		  return ch;
	   }
	   return '\0';
#endif
    }

} // namespace ctb
