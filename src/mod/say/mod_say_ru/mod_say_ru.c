/*
 * Copyright (c) 2007, Anthony Minessale II
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 * Michael B. Murdock <mike@mmurdock.org>
 * Oleg Dolya <oleg.dolya@gmail.com>
 * Boris Buklov <buklov@mail.ru>
 *
 * mod_say_ru.c -- Say for Russian
 *
 */

#include <switch.h>
#include <math.h>
#include <ctype.h>

typedef enum {
    male,       //мужского пола
    female,     //женского
    it          //оно
} sex_t;
            
            
            
typedef enum {
    how_much,   //сколько  с полом
    when,       //какого - когда   -- без пола
    what_       //какая/какой/какое с полом
} question_t;   //вопрос
                                


SWITCH_MODULE_LOAD_FUNCTION(mod_say_ru_load);
SWITCH_MODULE_DEFINITION(mod_say_ru, mod_say_ru_load, NULL, NULL);

#define say_num(num, t) {							\
		char tmp[80];\
		switch_status_t tstatus;\
		switch_snprintf(tmp, sizeof(tmp), "%u", (unsigned)num);				\
	if ((tstatus = ru_say_general_count(session, tmp, SST_ITEMS, t, args)) != SWITCH_STATUS_SUCCESS) {\
		return tstatus;\
	}}\

#define say_file(...) {\
		char tmp[80];\
		switch_status_t tstatus;\
		switch_snprintf(tmp, sizeof(tmp), __VA_ARGS__);\
		if ((tstatus = switch_ivr_play_file(session, NULL, tmp, args)) != SWITCH_STATUS_SUCCESS){ \
			return tstatus;\
		}\
		if (!switch_channel_ready(switch_core_session_get_channel(session))) {\
			return SWITCH_STATUS_FALSE;\
		}}\


static switch_status_t ru_spell(switch_core_session_t *session, char *tosay, switch_say_type_t type, switch_say_method_t method, switch_input_args_t *args)
{
	char *p;

	for (p = tosay; p && *p; p++) {
		int a = tolower((int) *p);
		if (a >= 48 && a <= 57) {
			say_file("digits/%d.wav", a - 48);
		} else {
			if (type == SST_NAME_SPELLED) {
				say_file("ascii/%d.wav", a);
			} else if (type == SST_NAME_PHONETIC) {
				say_file("phonetic-ascii/%d.wav", a);
			}
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

//воспроизводить по 3 цифры
static switch_status_t play_group( sex_t sex,question_t question, int a, int b, int c,
                                            char *what,int last, switch_core_session_t *session, switch_input_args_t *args)
{
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "play group %d %d %d   sex=%d  q=%d  last=%d\n", a,b,c,sex,question,last);
    if (a)    {
        if ((b==0)||(c==0)) { // если b и с равны 0 то сказать шестьсот, сестисотый, шестисотая
            switch (question)    {
//------------------------------------------------------            
        	    case how_much:  //сколько  когда дальше нету цифр например 100 200...
        		switch (sex) { //пол 		
        		    case male: //мужчина
    				say_file("digits/%d00.wav", a); //сто
				if (what=="thousand")   {
        			    say_file("digits/thousands.wav");//тысяч
        	    		}
				else if (what=="million")   {
        			    say_file("digits/millions.wav");//миллионов
        	    		}
				//-------------    	        	        	    
			        break;
			          
        		    case   female:   //женщина
    				say_file("digits/%d00.wav", a);//сто
				if (what=="thousand")   {
        			    say_file("digits/thousands.wav");//тысяч
        	    		}
				else if (what=="million")   {
        			    say_file("digits/millions.wav");//миллионов
        	    		}
        		    	break;
        		    	//-------------
        		    case     it:   //оно
        			say_file("digits/%d00.wav", a);//сто
				if (what=="thousand")   {
        			    say_file("digits/thousands.wav"); //тысяч
        	    		}
				else if (what=="million")   {
        			    say_file("digits/millions.wav");//миллионов
        	    		}
        		        break;
        		        //-------------
        		}
        	        break;	
//------------------------------------------------------
	            case what_:	//какой/я/ое
        		switch (sex) { //пол 		
        		    case   male:   //мужчина
				if (what=="thousand")   {
				    if (last==0) {
					say_file("digits/h-%d00xx.wav", a);//двухсот
        				say_file("digits/h-millionx.wav");//тысячный
				    }
				    else {
					say_file("digits/%d00.wav", a);//двести
        				say_file("digits/thousands.wav");//тысяч
				    }
				}
				else if (what=="million")   {
				    if (last==0) {
					say_file("digits/h-%d00xx.wav", a);//двухсот
        				say_file("digits/h-millionm.wav");//Миллионный
				    }
				    else {
					say_file("digits/%d00.wav", a);//двести
        				say_file("digits/millions.wav");//миллионов
				    }
        	    		}
        	    		else {
        	    		    say_file("digits/h-%d00m.wav", a);//сотый двухсотый
        	    		}
        			break;
        			   
        		    case female: //женщина
				if (what=="thousand")   {
				    if (last==0) {
					say_file("digits/h-%d00xx.wav", a);//двухсот
        				say_file("digits/h-millionf.wav");//тысячная
				    }
				    else {
					say_file("digits/%d00.wav", a);//двести
        				say_file("digits/thousands.wav");//тысяч
				    }
				}
				else if (what=="million")   {
				    if (last==0) {
					say_file("digits/h-%d00xx.wav", a);//двухсот
        				say_file("digits/h-millionf.wav");//Миллионная
				    }
				    else {
					say_file("digits/%d00.wav", a);//двести
        				say_file("digits/millions.wav");//миллионов
				    }
        	    		}
        	    		else {
        	    		    say_file("digits/h-%d00f.wav", a);//сотая двухсотая
        	    		}
        		    	break;
        		    case     it:   //оно
				if (what=="thousand")   {
				    if (last==0) {
					say_file("digits/h-%d00xx.wav", a);//двухсот
        				say_file("digits/h-millionn.wav");//тысячное
				    }
				    else {
					say_file("digits/%d00.wav", a);//двести
        				say_file("digits/thousands.wav");//тысяч
				    }
				}
				else if (what=="million")   {
				    if (last==0) {
					say_file("digits/h-%d00xx.wav", a);//двухсот
        				say_file("digits/h-millionn.wav");//Миллионное
				    }
				    else {
					say_file("digits/%d00.wav", a);//двести
        				say_file("digits/millions.wav");//миллионов
				    }
        	    		}
        	    		else {
        	    		    say_file("digits/h-%d00n.wav", a);//сотого
        	    		}
        		    	break;
    		    }
    		    break;
//-------------------------------------------------------
        	    case     when:	//какого - когда  без пола
				if (what=="thousand")   {
				    if (last==0) {
					say_file("digits/h-%d00xx.wav", a);//двухсот
        				say_file("digits/h-millionx.wav");//тысячного
				    }
				    else {
					say_file("digits/%d00.wav", a);//двести
        				say_file("digits/thousands.wav");//тысяч
				    }
				}
				else if (what=="million")   {
				    if (last==0) {
					say_file("digits/h-%d00xx.wav", a);//двухсот
        				say_file("digits/h-millionx.wav");//Миллионного
				    }
				    else {
					say_file("digits/%d00.wav", a);//двести
        				say_file("digits/millions.wav");//миллионов
				    }
        	    		}
        	    		else {
        	    		    say_file("digits/h-%d00x.wav", a);//сотого
        	    		}
        	           break;
    	    } //end switch (question)
    	}//end if ((b==0)||(c==0))
    	else // если есть ещё цифры
    	{
    	    switch (question) {
    		case how_much:
    		    say_file("digits/%d00.wav", a); //просто сто и тд
    		    break;
    		case what_:
    		    if (last==0)  {
    			say_file("digits/h-%d00xx.wav", a);
    		    }
    		    else   {
    			say_file("digits/%d00.wav", a); //сто
    		    }
    		    break;
    		case when:
    		    if (last==0)  {
    			say_file("digits/h-%d00xx.wav", a);
    		    }
    		    else   {
    			say_file("digits/%d00.wav", a); //сто
    		    }
    		    break;
    	    }
    	}
    }//end if (a)
    if (b) // если b больше 0
    {
        if (b > 1)  {   //если цифры больше 19
    	    if (c==0)  {  // если c == нолю 20-30-40-50
    		switch (question)    {
//------------------------------------------------------            
        	    case how_much:  //сколько  когда дальше нету цифр например 10 20...
        		switch (sex) { //пол 		
        		    case male: //мужчина
    				say_file("digits/%d0.wav", b); //двадцать
        			if (what=="thousand")  {
        	    	    	    say_file("digits/thousands.wav", b); //тысяч
        	    	        }
        			else if (what=="millon")  {
        	    	    	    say_file("digits/millions.wav", b); //миллионов
        	    	        }
				//-------------    	        	        	    
			        break;
			          
        		    case   female:   //женщина
    				say_file("digits/%d0.wav", b);//двадцать
        			if (what=="thousand")  {
        	    	    	    say_file("digits/thousands.wav", b); //тысяч
        	    	        }
        			else if (what=="millon")  {
        	    	    	    say_file("digits/millions.wav", b); //миллионов
        	    	        }
        		    	break;
        		    	//-------------
        		    case     it:   //оно
        			say_file("digits/%d0.wav", b);// двадцать
        			if (what=="thousand")  {
        	    	    	    say_file("digits/thousands.wav", b); //тысяч
        	    	        }
        			else if (what=="millon")  {
        	    	    	    say_file("digits/millions.wav", b); //миллионов
        	    	        }
        		        break;
        		        //-------------
        		}
        	        break;	
//------------------------------------------------------
	            case what_:	//какой/я/ое >19 и c==0 20-30-40
        		switch (sex) { //пол 		
        		    case   male:   //мужчина
        			if (what=="thousand")  {
        			    if (last==0) {
        				say_file("digits/h-%d0xx.wav", b);//двадцати
        			        say_file("digits/h-thousandm.wav", b); //тысячный
        			    }
        			    else {
        				say_file("digits/%d0.wav", b);//двадцать
        			        say_file("digits/h-thousand.wav", b); //тысяч
        			    }
        	    	        }
        			else if (what=="million")  {
        			    if (last==0) {
        				say_file("digits/h-%d0xx.wav", b);//двадцати
        			        say_file("digits/h-millionm.wav", b); //миллионный
        			    }
        			    else {
        				say_file("digits/%d0.wav", b);//двадцать
        			        say_file("digits/h-thousand.wav", b); //миллионов
        			    }
        	    	        }
        	    	        else { //без миллионов и тысяч
        	    	    	    if (last==0)  {
        	    	    		 say_file("digits/h-%d0m.wav", b);//двадцатый
        	    	    	    }
        	    	    	    else {
        	    	    		 say_file("digits/%d0.wav", b);//двадцать
        	    	    	    }
        	    	        }
				break;
        		    case female: //женщина
        			if (what=="thousand")  {
        			    if (last==0) {
        				say_file("digits/h-%d0xx.wav", b);//двадцати
        			        say_file("digits/h-thousandf.wav", b); //тысячная
        			    }
        			    else {
        				say_file("digits/%d0.wav", b);//двадцать
        			        say_file("digits/h-thousand.wav", b); //тысяч
        			    }
        	    	        }
        			else if (what=="million")  {
        			    if (last==0) {
        				say_file("digits/h-%d0xx.wav", b);//двадцати
        			        say_file("digits/h-millionf.wav", b); //миллионная
        			    }
        			    else {
        				say_file("digits/%d0.wav", b);//двадцать
        			        say_file("digits/h-thousand.wav", b); //миллионов
        			    }
        	    	        }
        	    	        else { //без миллионов и тысяч
        	    	    	    if (last==0)  {
        	    	    		 say_file("digits/h-%d0f.wav", b);//двадцатая
        	    	    	    }
        	    	    	    else {
        	    	    		 say_file("digits/%d0.wav", b);//двадцать
        	    	    	    }
        	    	        }
        		    	break;
        		    case     it:   //оно
        			if (what=="thousand")  {
        			    if (last==0) {
        				say_file("digits/h-%d0xx.wav", b);//двадцати
        			        say_file("digits/h-thousandn.wav", b); //тысячное
        			    }
        			    else {
        				say_file("digits/%d0.wav", b);//двадцать
        			        say_file("digits/h-thousand.wav", b); //тысяч
        			    }
        	    	        }
        			else if (what=="million")  {
        			    if (last==0) {
        				say_file("digits/h-%d0xx.wav", b);//двадцати
        			        say_file("digits/h-millionn.wav", b); //миллионное
        			    }
        			    else {
        				say_file("digits/%d0.wav", b);//двадцать
        			        say_file("digits/h-thousand.wav", b); //миллионов
        			    }
        	    	        }
        	    	        else { //без миллионов и тысяч
        	    	    	    if (last==0)  {
        	    	    		 say_file("digits/h-%d0n.wav", b);//двадцатое
        	    	    	    }
        	    	    	    else {
        	    	    		 say_file("digits/%d0.wav", b);//двадцать
        	    	    	    }
        	    	        }
        		    	break;
        		}
        		break;
//-------------------------------------------------------
        	    case     when:	//какого - когда  без пола
        			if (what=="thousand")  {
        			    if (last==0) {
        				say_file("digits/h-%d0xx.wav", b);//двадцати
        			        say_file("digits/h-thousandx.wav", b); //тысячного
        			    }
        			    else {
        				say_file("digits/%d0.wav", b);//двадцать
        			        say_file("digits/h-thousand.wav", b); //тысяч
        			    }
        	    	        }
        			else if (what=="million")  {
        			    if (last==0) {
        				say_file("digits/h-%d0xx.wav", b);//двадцати
        			        say_file("digits/h-millionx.wav", b); //миллионного
        			    }
        			    else {
        				say_file("digits/%d0.wav", b);//двадцать
        			        say_file("digits/h-thousand.wav", b); //миллионов
        			    }
        	    	        }
        	    	        else { //без миллионов и тысяч
        	    	    	    if (last==0)  {
        	    	    		 say_file("digits/h-%d0x.wav", b);//двадцатого
        	    	    	    }
        	    	    	    else {
        	    	    		 say_file("digits/%d0.wav", b);//двадцать
        	    	    	    }
        	    	        }
        	        	break;
        	}
    	    }//конец если c == нолю
    	    else 
    	    {
    		say_file("digits/%d0.wav", b); // иначе просто двадцать .. и тд
    	    }
	}//конец если больше 19
	else { //если цифры меньше 20
	    switch (question)    {
//------------------------------------------------------            
    		case how_much:  //сколько  когда от 10 до 19
    		    switch (sex) { //пол 		
    			case male: //мужчина
			    say_file("digits/%d%d.wav",b ,c); //девятнадцать
        		    if (what=="thousand")  {
        	    	        say_file("digits/thousands.wav"); //тысяч
        	    	    }
        		    else if (what=="million")  {
        	    	        say_file("digits/millions.wav"); //миллионов
        	    	    }
			    //-------------    	        	        	    
		    	    break;
    			case   female:   //женщина
			    say_file("digits/%d%d.wav",b ,c);//девятнадцать
        		    if (what=="thousand")  {
        	    	        say_file("digits/thousands.wav"); //тысяч
        	    	    }
        		    else if (what=="million")  {
        	    	        say_file("digits/millions.wav"); //миллионов
        	    	    }
    		    	    break;
    		    	    //-------------
    			case     it:   //оно
    			    say_file("digits/%d%d.wav",b ,c);// девятнадцать
        		    if (what=="thousand")  {
        	    	        say_file("digits/thousands.wav"); //тысяч
        	    	    }
        		    else if (what=="million")  {
        	    	        say_file("digits/millions.wav"); //миллионов
        	    	    }
    		    	    break;
    		    	    //-------------
    		    }
    		    break;
//------------------------------------------------------
        	case what_:	//какой/я/ое
    		    switch (sex) { //пол 		
    			case male: //женщина
        			if (what=="thousand")  {
        			    if (last==0) {
        				say_file("digits/h-%d%dxx.wav", b,c);//десяти, пятнадцати ..
        			        say_file("digits/h-thousandm.wav"); //тысячный
        			    }
        			    else {
        				say_file("digits/%d%d.wav", b,c);//девятнацать
        			        say_file("digits/h-thousand.wav"); //тысяч
        			    }
        	    	        }
        			else if (what=="million")  {
        			    if (last==0) {
        				say_file("digits/h-%d%dxx.wav", b,c);//десяти, пятнадцати ..
        			        say_file("digits/h-millionm.wav"); //миллионный
        			    }
        			    else {
        				say_file("digits/%d%d.wav", b,c);//девятнадцать
        			        say_file("digits/h-thousand.wav"); //миллионов
        			    }
        	    	        }
        	    	        else { //без миллионов и тысяч
        	    	    	    if (last==0)  {
        	    	    		 say_file("digits/h-%d%dm.wav", b,c);//девятнадцатый
        	    	    	    }
        	    	    	    else {
        	    	    		 say_file("digits/%d%d.wav");//девятнадцать
        	    	    	    }
        	    	        }
    			case female: //женщина
        			if (what=="thousand")  {
        			    if (last==0) {
        				say_file("digits/h-%d%dxx.wav", b,c);//десяти, пятнадцати ..
        			        say_file("digits/h-thousandf.wav"); //тысячная
        			    }
        			    else {
        				say_file("digits/%d%d.wav", b,c);//девятнацать
        			        say_file("digits/h-thousand.wav"); //тысяч
        			    }
        	    	        }
        			else if (what=="million")  {
        			    if (last==0) {
        				say_file("digits/h-%d%dxx.wav", b,c);//десяти, пятнадцати ..
        			        say_file("digits/h-millionf.wav"); //миллионная
        			    }
        			    else {
        				say_file("digits/%d%d.wav", b,c);//девятнадцать
        			        say_file("digits/h-thousand.wav"); //миллионов
        			    }
        	    	        }
        	    	        else { //без миллионов и тысяч
        	    	    	    if (last==0)  {
        	    	    		 say_file("digits/h-%d%df.wav", b,c);//девятнадцатая
        	    	    	    }
        	    	    	    else {
        	    	    		 say_file("digits/%d%d.wav");//девятнадцать
        	    	    	    }
        	    	        }
    		    	    break;
    			case     it:   //оно
        			if (what=="thousand")  {
        			    if (last==0) {
        				say_file("digits/h-%d%dxx.wav", b,c);//десяти, пятнадцати ..
        			        say_file("digits/h-thousandn.wav"); //тысячное
        			    }
        			    else {
        				say_file("digits/%d%d.wav", b,c);//девятнацать
        			        say_file("digits/h-thousand.wav"); //тысяч
        			    }
        	    	        }
        			else if (what=="million")  {
        			    if (last==0) {
        				say_file("digits/h-%d%dxx.wav", b,c);//десяти, пятнадцати ..
        			        say_file("digits/h-millionn.wav"); //миллионное
        			    }
        			    else {
        				say_file("digits/%d%d.wav", b,c);//девятнадцать
        			        say_file("digits/h-thousand.wav"); //миллионов
        			    }
        	    	        }
        	    	        else { //без миллионов и тысяч
        	    	    	    if (last==0)  {
        	    	    		 say_file("digits/h-%d%dn.wav", b,c);//девятнадцатое
        	    	    	    }
        	    	    	    else {
        	    	    		 say_file("digits/%d%d.wav");//девятнадцать
        	    	    	    }
        	    	        }
    		    	    break;
    		    }
    		    break;
//-------------------------------------------------------
        	case    when:	//какого - когда  без пола
        			if (what=="thousand")  {
        			    if (last==0) {
        				say_file("digits/h-%d%dxx.wav", b,c);//десяти, пятнадцати ..
        			        say_file("digits/h-thousandx.wav"); //тысячного
        			    }
        			    else {
        				say_file("digits/%d%d.wav", b,c);//девятнацать
        			        say_file("digits/h-thousand.wav"); //тысяч
        			    }
        	    	        }
        			else if (what=="million")  {
        			    if (last==0) {
        				say_file("digits/h-%d%dxx.wav", b,c);//десяти, пятнадцати ..
        			        say_file("digits/h-millionx.wav"); //миллионного
        			    }
        			    else {
        				say_file("digits/%d%d.wav", b,c);//девятнадцать
        			        say_file("digits/h-thousand.wav"); //миллионов
        			    }
        	    	        }
        	    	        else { //без миллионов и тысяч
        	    	    	    if (last==0)  {
        	    	    		 say_file("digits/h-%d%dx.wav", b,c);//девятнадцатого
        	    	    	    }
        	    	    	    else {
        	    	    		 say_file("digits/%d%d.wav");//девятнадцать
        	    	    	    }
        	    	        }

        	    		break;
    	    }//конец если c == нолю
    	    c=0; //что бы не проговаривать c
	}// конец //если цифры меньше 20
    }//конец if (b)
    
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "play group  c \n");
    

                                                                                                 
    if ((c)||(what=="zero")) {// последняя цифра (самая сложная) или проговорить ноль , для случает когда первые цифры нули
	if (c>2||c==0) {//0 и 3-9
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "play group  c 1\n");

    	    switch (question)    
    	    {
//------------------------------------------------------            
    		case how_much:  //сколько  3-9
    		    switch (sex) 
    		    { //пол 		
    			case male: //мужчина
    			    if (what=="thousand")  { //тысяч
    				if ((c>2)&&(c<5)) {
    				    say_file("digits/%d.wav", c);// три - четыре
    				    say_file("digits/thousands-i.wav"); //тысячи
    				} 
    				else{
    				    say_file("digits/%d.wav", c);// пять .. девять
    				    say_file("digits/thousands.wav"); //тысяч
    				}
        	    	    }
        	    	    else if (what=="million") { //миллионов
    				if ((c>2)&&(c<5)) {
    				    say_file("digits/%d.wav", c);// три четыре
    				    say_file("digits/million-a.wav"); //миллиона
    				} 
    				else{
    				    say_file("digits/%d.wav", c);// пять .. девять
    				    say_file("digits/millions.wav"); //миллионов
    				}
    			    }
    			    else {
				say_file("digits/%d.wav", c);// три девять ноль
			    }
		    	    break;
		    	    
    			case   female:   //женщина
    			    if (what=="thousand")  { //тысяч
    				if ((c>2)&&(c<5)) {
    				    say_file("digits/%d.wav", c);// три - четыре
    				    say_file("digits/thousands-i.wav"); //тысячи
    				} 
    				else{
    				    say_file("digits/%d.wav", c);// пять .. девять
    				    say_file("digits/thousands.wav"); //тысяч
    				}
        	    	    }
        	    	    else if (what=="million") { //миллионов
    				if ((c>2)&&(c<5)) {
    				    say_file("digits/%d.wav", c);// три четыре
    				    say_file("digits/million-a.wav"); //миллиона
    				} 
    				else{
    				    say_file("digits/%d.wav", c);// пять .. девять
    				    say_file("digits/millions.wav"); //миллионов
    				}
    			    }
    			    else {
				say_file("digits/%d.wav", c);// три девять ноль
			    }
    		    	    break;

    		    	    //-------------
    			case     it:   //оно
    			    if (what=="thousand")  { //тысяч
    				if ((c>2)&&(c<5)) {
    				    say_file("digits/%d.wav", c);// три - четыре
    				    say_file("digits/thousands-i.wav"); //тысячи
    				} 
    				else{
    				    say_file("digits/%d.wav", c);// пять .. девять
    				    say_file("digits/thousands.wav"); //тысяч
    				}
        	    	    }
        	    	    else if (what=="million") { //миллионов
    				if ((c>2)&&(c<5)) {
    				    say_file("digits/%d.wav", c);// три четыре
    				    say_file("digits/million-a.wav"); //миллиона
    				} 
    				else{
    				    say_file("digits/%d.wav", c);// пять .. девять
    				    say_file("digits/millions.wav"); //миллионов
    				}
    			    }
    			    else {
				say_file("digits/%d.wav", c);// три девять ноль
			    }
    		    	    break;
		    }		
		    break;
//------------------------------------------------------
    		case what_:	//какой/я/ое
    		    switch (sex) 
    		    { //пол 		
    			case   male:   //мужчина
    			    if (what=="thousand")  {
    				if (last==0) {// если дальше нету цифр значит говорить двух, 3-х, 4-х ...
    				    say_file("digits/h-%dxx.wav", c);//одна, двух, трёх
        	    	    	    say_file("digits/h-thousandm.wav"); //тысячный
        	    	    	}
        	    	    	else { //если есть дальше цифры меньше 1000
    				    say_file("digits/%d.wav", c);//три четыре ..
        	    	    	    say_file("digits/thousand.wav"); //тысячи
        	    	    	}
        	    	    }
    			    else if (what=="million")  {
    				if (last==0) {// если дальше нету цифр значит говорить двух, 3-х, 4-х ...
    				    say_file("digits/h-%dxx.wav", c);//одна, двух, трёх
        	    	    	    say_file("digits/h-millionm.wav"); //миллионный
        	    	    	}
        	    	    	else { //если есть дальше цифры меньше 1000
    				    say_file("digits/%d.wav", c);//три четыре ..
        	    	    	    say_file("digits/thousand.wav"); //миллиона
        	    	    	}
        	    	    }//просто цифры без тысяч
        	    	    else{
    				    say_file("digits/h-%dm.wav", c);//третий нулевой ..
        	    	    }
    			    break;
    			case female: //женщина
    			    if (what=="thousand")  {
    				if (last==0) {// если дальше нету цифр значит говорить двух, 3-х, 4-х ...
    				    say_file("digits/h-%dxx.wav", c);//одна, двух, трёх
        	    	    	    say_file("digits/h-thousandf.wav"); //тысячная
        	    	    	}
        	    	    	else { //если есть дальше цифры меньше 1000
    				    say_file("digits/%d.wav", c);//три четыре ..
        	    	    	    say_file("digits/thousand.wav"); //тысячи
        	    	    	}
        	    	    }
    			    else if (what=="million")  {
    				if (last==0) {// если дальше нету цифр значит говорить двух, 3-х, 4-х ...
    				    say_file("digits/h-%dxx.wav", c);//одна, двух, трёх
        	    	    	    say_file("digits/h-millionf.wav"); //миллионная
        	    	    	}
        	    	    	else { //если есть дальше цифры меньше 1000
    				    say_file("digits/%d.wav", c);//три четыре ..
        	    	    	    say_file("digits/thousand.wav"); //миллиона
        	    	    	}
        	    	    }//просто цифры без тысяч
        	    	    else{
    				    say_file("digits/h-%df.wav", c);//третья нулевая ..
        	    	    }
    		    	    break;
    			case it: //оно
    			    if (what=="thousand")  {
    				if (last==0) {// если дальше нету цифр значит говорить двух, 3-х, 4-х ...
    				    say_file("digits/h-%dxx.wav", c);//одна, двух, трёх
        	    	    	    say_file("digits/h-thousandn.wav"); //тысячное
        	    	    	}
        	    	    	else { //если есть дальше цифры меньше 1000
    				    say_file("digits/%d.wav", c);//три четыре ..
        	    	    	    say_file("digits/thousand.wav"); //тысячи
        	    	    	}
        	    	    }
    			    else if (what=="million")  {
    				if (last==0) {// если дальше нету цифр значит говорить двух, 3-х, 4-х ...
    				    say_file("digits/h-%dxx.wav", c);//одна, двух, трёх
        	    	    	    say_file("digits/h-millionn.wav"); //миллионное
        	    	    	}
        	    	    	else { //если есть дальше цифры меньше 1000
    				    say_file("digits/%d.wav", c);//три четыре ..
        	    	    	    say_file("digits/thousand.wav"); //миллиона
        	    	    	}
        	    	    }//просто цифры без тысяч
        	    	    else{
    				    say_file("digits/h-%dn.wav", c);//третье нулевое ..
        	    	    }
    		    	    break;
    		    }
    		    break;
//-------------------------------------------------------
        	case     when:	//какого - когда  без пола
    			    if (what=="thousand")  {
    				if (last==0) {// если дальше нету цифр значит говорить двух, 3-х, 4-х ...
    				    say_file("digits/h-%dxx.wav", c);//одна, двух, трёх
        	    	    	    say_file("digits/h-thousandx.wav"); //тысячного
        	    	    	}
        	    	    	else { //если есть дальше цифры меньше 1000
    				    say_file("digits/%d.wav", c);//три четыре ..
        	    	    	    say_file("digits/thousand.wav"); //тысячи
        	    	    	}
        	    	    }
    			    else if (what=="million")  {
    				if (last==0) {// если дальше нету цифр значит говорить двух, 3-х, 4-х ...
    				    say_file("digits/h-%dxx.wav", c);//одна, двух, трёх
        	    	    	    say_file("digits/h-millionx.wav"); //миллионого
        	    	    	}
        	    	    	else { //если есть дальше цифры меньше 1000
    				    say_file("digits/%d.wav", c);//три четыре ..
        	    	    	    say_file("digits/thousand.wav"); //миллиона
        	    	    	}
        	    	    }//просто цифры без тысяч
        	    	    else{
    				    say_file("digits/h-%dx.wav", c);//третьего нулевого ..
        	    	    }
    		    	    break;
    	    }//конец switch (question)	
    	} //конец //0 и 3-9
	else if ((c==2)||(c==1)) {  //1 2	
    	    switch (question)    {
//------------------------------------------------------            
    		case how_much:  //
    		    switch (sex) 
    		    { //пол 		
    			case male: //мужчина
    			    if (what=="thousand")  {
    				    if (c==1)  {    				    
    					say_file("digits/%df.wav", c); // одна две
    					say_file("digits/thousand.wav"); //тысяча
    				    }
    				    else{
    					say_file("digits/%df.wav", c); // одна две
    					say_file("digits/thousands-i.wav"); //тысячи
    				    }
        	    	    }
    			    else if (what=="million")  {
			    	    say_file("digits/%d.wav", c); // один два
    				    if (c==1) {
    					say_file("digits/million.wav", c); //миллион
    				    }
    				    else   { //один два
    					say_file("digits/million-a.wav"); // миллиона
    				    }
    			    } 
			    else   { //просто один два 
    				say_file("digits/%d.wav", c); // один два
    			    }
		    	    break;
			    //-------------    	        	        	    
    			case   female:   //женщина
    			    if (what=="thousand")  {
    				    if (c==1)  {    				    
    					say_file("digits/%df.wav", c); // одна две
    					say_file("digits/thousand.wav"); //тысяча
    				    }
    				    else{
    					say_file("digits/%df.wav", c); // одна две
    					say_file("digits/thousands-i.wav"); //тысячи
    				    }
        	    	    }
    			    else if (what=="million")  {
			    	    say_file("digits/%d.wav", c); // один два
    				    if (c==1) {
    					say_file("digits/million.wav"); //миллион
    				    }
    				    else   { //один два
    					say_file("digits/million-a.wav"); // миллиона
    				    }
    			    } 
			    else   { //просто одна две
    				say_file("digits/%df.wav", c); // одна две
    			    }
    			    break;
    		    	    //-------------
    			case     it:   //оно
    			    if (what=="thousand")  {
    				    if (c==1)  {    				    
    					say_file("digits/%df.wav", c); // одна две
    					say_file("digits/thousand.wav"); //тысяча
    				    }
    				    else{
    					say_file("digits/%df.wav", c); // одна две
    					say_file("digits/thousands-i.wav"); //тысячи
    				    }
        	    	    }
    			    else if (what=="million")  {
			    	    say_file("digits/%d.wav", c); // один два
    				    if (c==1) {
    					say_file("digits/million.wav"); //миллион
    				    }
    				    else   { //один два
    					say_file("digits/million-a.wav"); // миллиона
    				    }
    			    } 
			    else   { //просто одно две
    				say_file("digits/%dn.wav", c); // одна две
    			    }
    		    	    break;
		    }
		    break;		
//------------------------------------------------------
    		case what_:	//какой/я/ое
    		    switch (sex) 
    		    { //пол 		
    			case male: //мужчина
    			    if (what=="thousand")  {
    				if (last==0) {// если дальше нету цифр значит говорить двух,одна 2-х, ...
    				    if (c!=1) { //не произность одна тысячный
    					say_file("digits/h-%dxx.wav", c);//одна, двух
    				    }
        	    	    	    say_file("digits/h-thousandm.wav"); //тысячный
        	    	    	}
        	    	    	else { //если есть дальше цифры меньше 1000
    				    say_file("digits/%df.wav", c);//одна две
        	    	    	    say_file("digits/thousands-i.wav"); //тысячи
        	    	    	}
        	    	    }
    			    else if (what=="million")  {
    				if (last==0) {// если дальше нету цифр значит говорить двух, 3-х, 4-х ...
    				    if (c!=1)  {
    					say_file("digits/h-%dxx.wav", c);//одна, двух, трёх
    				    }
        	    	    	    say_file("digits/h-millionm.wav"); //миллионный
        	    	    	}
        	    	    	else { //если есть дальше цифры меньше 1000
    				    say_file("digits/%d.wav", c);//три четыре ..
        	    	    	    say_file("digits/thousand-a.wav"); //миллиона
        	    	    	}
        	    	    }//просто цифры без тысяч
        	    	    else{
    				    say_file("digits/h-%dm.wav", c);//третий нулевой ..
        	    	    }
    		    	    break;
    			case female: //женщина
    			    if (what=="thousand")  {
    				if (last==0) {// если дальше нету цифр значит говорить двух, 1-х, 2-х ...
    				    if (c!=1) { //не произность одна тысячная
    					say_file("digits/h-%dxx.wav", c);//одна, двух
    				    }
        	    	    	    say_file("digits/h-thousandf.wav"); //тысячная
        	    	    	}
        	    	    	else { //если есть дальше цифры меньше 1000
    				    say_file("digits/%df.wav", c);//две четыре ..
        	    	    	    say_file("digits/thousands-i.wav"); //тысячи
        	    	    	}
        	    	    }
    			    else if (what=="million")  {
    				if (last==0) {// если дальше нету цифр значит говорить двух, 3-х, 4-х ...
    				    if (c!=1)  {
    					say_file("digits/h-%dxx.wav", c);//одна, двух, трёх
    				    }
        	    	    	    say_file("digits/h-millionf.wav"); //миллионная
        	    	    	}
        	    	    	else { //если есть дальше цифры меньше 1000
    				    say_file("digits/%d.wav", c);//три четыре ..
        	    	    	    say_file("digits/thousand-a.wav"); //миллиона
        	    	    	}
        	    	    }//просто цифры без тысяч
        	    	    else{
    				    say_file("digits/h-%df.wav", c);//третья нулевая ..
        	    	    }
    		    	    break;
    			case     it:   //оно
    			    if (what=="thousand")  {
    				if (last==0) {// если дальше нету цифр значит говорить двух, 1-х, 2-х ...
    				    if (c!=1) { //не произность одна тысячное
    					say_file("digits/h-%dxx.wav", c);//одна, двух
    				    }
        	    	    	    say_file("digits/h-thousandn.wav"); //тысячное
        	    	    	}
        	    	    	else { //если есть дальше цифры меньше 1000
    				    say_file("digits/%df.wav", c);//две ..
        	    	    	    say_file("digits/thousands-i.wav"); //тысячи
        	    	    	}
        	    	    }
    			    else if (what=="million")  {
    				if (last==0) {// если дальше нету цифр значит говорить двух, 3-х, 4-х ...
    				    if (c!=1)  {
    					say_file("digits/h-%dxx.wav", c);//одна, двух
    				    }
        	    	    	    say_file("digits/h-millionn.wav"); //миллионное
        	    	    	}
        	    	    	else { //если есть дальше цифры меньше 1000
    				    say_file("digits/%d.wav", c);//три четыре ..
        	    	    	    say_file("digits/thousand-a.wav"); //миллиона
        	    	    	}
        	    	    }//просто цифры без тысяч
        	    	    else{
    				    say_file("digits/h-%dn.wav", c);//третье нулевое ..
        	    	    }
    		    	    break;
    		}
    		break;
//-------------------------------------------------------
        	case     when:	//какого - когда  без пола
    			    if (what=="thousand")  {
    				if (last==0) {// если дальше нету цифр значит говорить двух, 3-х, 4-х ...
    				    if (c!=1) { //не произность одна тысячного
    					say_file("digits/h-%dxx.wav", c);//одна, двух
    				    }
        	    	    	    say_file("digits/h-thousandx.wav"); //тысячного
        	    	    	}
        	    	    	else { //если есть дальше цифры меньше 1000
    				    say_file("digits/%df.wav", c);//две четыре ..
        	    	    	    say_file("digits/thousands-i.wav"); //тысячи
        	    	    	}
        	    	    }
    			    else if (what=="million")  {
    				if (last==0) {// если дальше нету цифр значит говорить двух, 3-х, 4-х ...
    				    if (c!=1)  {
    					say_file("digits/h-%dxx.wav", c);//одна, двух, трёх
    				    }
        	    	    	    say_file("digits/h-millionx.wav"); //миллионного
        	    	    	}
        	    	    	else { //если есть дальше цифры меньше 1000
    				    say_file("digits/%d.wav", c);//три четыре ..
        	    	    	    say_file("digits/thousand-a.wav"); //миллиона
        	    	    	}
        	    	    }//просто цифры без тысяч
        	    	    else{
    				    say_file("digits/h-%dx.wav", c);//третьего нулевого ..
        	    	    }
    		    	    break;
    	    }	
    	}//else if ((c==2)||(c==1)) {  //1 2
	
    }//конец if ((c)||(what=="zero"))
    return SWITCH_STATUS_SUCCESS;
}                             
               



static char *strip_commas(char *in, char *out, switch_size_t len)
{
	char *p = in, *q = out;
	char *ret = out;
	switch_size_t x = 0;

	for (; p && *p; p++) {
		if ((*p > 47 && *p < 58)) {
			*q++ = *p;
		} else if (*p != ',') {
			ret = NULL;
			break;
		}

		if (++x > len) {
			ret = NULL;
			break;
		}
	}

	return ret;
}

static char *strip_nonnumerics(char *in, char *out, switch_size_t len)
{
	char *p = in, *q = out;
	char *ret = out;
	switch_size_t x = 0;
	// valid are 0 - 9, period (.), minus (-), and plus (+) - remove all others
	for (; p && *p; p++) {
		if ((*p > 47 && *p < 58) || *p == '.' || *p == '-' || *p == '+') {
			*q++ = *p;
		}

		if (++x > len) {
			ret = NULL;
			break;
		}
	}

	return ret;
}


static switch_status_t ru_say_count(switch_core_session_t *session,
    char *tosay, sex_t sex, question_t question, switch_input_args_t *args)
{
    	int in;
	int x = 0;
	int places[9] = { 0 };
	char sbuf[13] = "";
	
	switch_status_t status;

        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ru_say_count %d %d %d other!\n", places[2], places[1], places[0]);
	
	if (!(tosay = strip_commas(tosay, sbuf, sizeof(sbuf))) || strlen(tosay) > 9) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
		return SWITCH_STATUS_GENERR;
	}
	
	in = atoi(tosay);
	int in_ = in;
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "int in=%d!\n", in);
	if (in != 0) {
		for (x = 8; x >= 0; x--) {
			int num = (int) pow(10, x);
			if ((places[(uint32_t) x] = in / num)) {
				in -= places[(uint32_t) x] * num;
			}
		}
    		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "int in=%d \n", in);

			if ((status = play_group(sex,question, places[8], places[7], places[6], "million",in%1000000, session, args)) != SWITCH_STATUS_SUCCESS) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "play group %d %d %d million!\n", places[8], places[7], places[6]);
				return status;
			}
			if ((status = play_group(sex,question, places[5], places[4], places[3], "thousand",in_%1000,session, args)) != SWITCH_STATUS_SUCCESS) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "play group %d %d %d thousand!\n", places[5], places[4], places[3]);
				return status;
			}
			if ((status = play_group(sex,question, places[2], places[1], places[0], NULL, 0,session, args)) != SWITCH_STATUS_SUCCESS) 
			{
	                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "play group %d %d %d other!\n", places[2], places[1], places[0]);
			    return status;
			}
	} else { //если ноль что бы и его проговаривать в правильных падежах
	    if ((status = play_group(sex,question, places[2], places[1], places[0], "zero",0, session, args)) != SWITCH_STATUS_SUCCESS) 
	    {
	        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "play group %d %d %d other!\n", places[2], places[1], places[0]);
	        return status;
	    }
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t ru_say_general_count(switch_core_session_t *session,
        char *tosay, switch_say_type_t type, switch_say_method_t method, switch_input_args_t *args)
{
	switch_status_t status;
	sex_t sex;
	question_t question;

        if (type== SST_MESSAGES)
        {
	    sex=it;
	    question=how_much;
        }
        else if (type== SST_NUMBER||type==SST_PERSONS)
        {
            sex=male;
            question=how_much;
        }
        else if (type== SST_ITEMS)
        {
            sex=male;
            question=what_;
        }
        else 
        {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown Say type=[%d]\n", type);
    	    return SWITCH_STATUS_FALSE;
        }    
	status=ru_say_count(session,tosay,sex,question,args);	
	return SWITCH_STATUS_SUCCESS;
}



static switch_status_t ru_say_money(switch_core_session_t *session, char *tosay, switch_say_type_t type,
                switch_say_method_t method,switch_input_args_t *args)
{
        char sbuf[16] = "";                    
        char *dollars = NULL;
        char *cents = NULL;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " ru_say_money %s\n",tosay );
        if (strlen(tosay) > 15 || !(tosay = strip_nonnumerics(tosay, sbuf, sizeof(sbuf)))) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
                return SWITCH_STATUS_GENERR;
        }
        dollars = sbuf;

        if ((cents = strchr(sbuf, '.'))) {
                *cents++ = '\0';
                if (strlen(cents) > 2) {
                        cents[2] = '\0';
                }
        }
        if (sbuf[0] == '+') {
                dollars++;
        }

        if (sbuf[0] == '-') {
                say_file("currency/minus.wav");
                dollars++;
        }
	    ru_say_count(session,dollars ,male,how_much,args);
    	    int idollars = atoi(dollars)%100;
    	    int idollar = atoi(dollars)%10;
    	    if (idollars == 1 || (idollars > 20 && idollar == 1)) {/* рубль */
		say_file("currency/dollar.wav");
    	    } 
    	    else if ((idollars > 1 && idollars < 5) || (idollars > 20 && idollar > 1 && idollar < 5))  {  /*рубля */
        	say_file("currency/dollar1.wav");
    	    } 
    	    else  {    /*рублей */
    		say_file("currency/dollar2.wav");
    	    }
        /* Say cents */
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " %s\n",cents );
	    ru_say_count(session,cents ,female,how_much,args);
            int icents = atoi(cents)%100;
            int icent = atoi(cents)%10;
            if (icents == 1 || (icents > 20 && icent == 1)) 
            {
                /* копейка */
                say_file("currency/cent.wav");
            } 
            else if ((icents > 1 && icents < 5) || (icents > 20 && icent > 1 && icent < 5)) 
            {
        	/* копейки */
                say_file("currency/cent1.wav");
            }
            else 
            {
        	/* копеек */
                say_file("currency/cents.wav");
            }
    return SWITCH_STATUS_SUCCESS;
}


static switch_status_t ru_say_time(switch_core_session_t *session, char *tosay, switch_say_type_t type, switch_say_method_t method,
								   switch_input_args_t *args)
{
	int32_t t;
	char tmp[80];
	switch_time_t target = 0, target_now = 0;
	switch_time_exp_t tm, tm_now;
	uint8_t say_date = 0, say_time = 0, say_year = 0, say_month = 0, say_dow = 0, say_day = 0, say_yesterday = 0, say_today = 0;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *tz = switch_channel_get_variable(channel, "timezone");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " ru_say_time %s  type=%d method=%d\n",tosay, type,method );
	
	if (type == SST_TIME_MEASUREMENT) { 
		int64_t hours = 0;
		int64_t minutes = 0;
		int64_t seconds = 0;
		int64_t r = 0;

		if (strchr(tosay, ':')) {
			char *tme = switch_core_session_strdup(session, tosay);
			char *p;

			if ((p = strrchr(tme, ':'))) {
				*p++ = '\0';
				seconds = atoi(p);
				if ((p = strchr(tme, ':'))) {
					*p++ = '\0';
					minutes = atoi(p);
					if (tme) {
						hours = atoi(tme);
					}
				} else {
					minutes = atoi(tme);
				}
			}
		} else {
			if ((seconds = atol(tosay)) <= 0) {
				seconds = (int64_t) switch_epoch_time_now(NULL);
			}

			if (seconds >= 60) {
				minutes = seconds / 60;
				r = seconds % 60;
				seconds = r;
			}

			if (minutes >= 60) {
				hours = minutes / 60;
				r = minutes % 60;
				minutes = r;
			}
		}

		switch_snprintf(tmp, sizeof(tmp), "%u", (unsigned)hours);
		ru_say_count(session,tmp ,male,how_much,args);
        	if (((hours%10) == 1) && (hours!=11)) {
                /* час */
		    say_file("time/hour.wav");
		} 
		else if (((hours%10>1)&&(hours%10<5)) &&((hours<12)||(hours>14))) {
		    say_file("time/hours-a.wav");  /* часа */
        	} 
        	else {
		    say_file("time/hours.wav"); /* часов*/
		}

                switch_snprintf(tmp, sizeof(tmp), "%u", (unsigned)minutes); //перевести минуты в *char
                ru_say_count(session,tmp ,female,how_much,args);
		if (((minutes%10) == 1) && (minutes!=11)) {
		    say_file("time/minute.wav"); //минута
		}
		else if (((minutes%10>1)&&(minutes%10<5))&&((minutes<12)||(minutes>14))){
		    say_file("time/minutes-i.wav"); // минуты
		}
		else {
		    say_file("time/minutes.wav"); //минут
		}
		
		if (seconds!=0) {
            	    switch_snprintf(tmp, sizeof(tmp), "%u", (unsigned)seconds);
            	    ru_say_count(session,tmp ,female,how_much,args);
		    if (((seconds%10) == 1) && (seconds!=11)) {
			say_file("time/second.wav"); // секунда
		    } 
		    else if (((seconds%10>1)&&(seconds%10<5))&&((seconds<12)||(seconds>14))) {
			say_file("time/seconds-i.wav"); // секуны
		    }
		    else {
			say_file("time/seconds.wav"); //секунд
		    }
		}

		return SWITCH_STATUS_SUCCESS;
	}

	if ((t = atol(tosay)) > 0) {
		target = switch_time_make(t, 0);
		target_now = switch_micro_time_now();
	} else {
		target = switch_micro_time_now();
		target_now = switch_micro_time_now();
	}
	
	if (tz) {
		int check = atoi(tz);
		if (check) {
			switch_time_exp_tz(&tm, target, check);
			switch_time_exp_tz(&tm_now, target_now, check);
		} else {
			switch_time_exp_tz_name(tz, &tm, target);
			switch_time_exp_tz_name(tz, &tm_now, target_now);
		}
	} else {
		switch_time_exp_lt(&tm, target);
		switch_time_exp_lt(&tm_now, target_now);
	}
	switch (type) {
	case SST_CURRENT_DATE_TIME:
		say_date = say_time = 1;
		break;
	case SST_CURRENT_DATE:
		say_date = 1;
		break;
	case SST_CURRENT_TIME:
		say_time = 1;
		break;
	case SST_SHORT_DATE_TIME:
		say_time = 1;
		if (tm.tm_year != tm_now.tm_year) {
			say_date = 1;
			break;
		}
		if (tm.tm_yday == tm_now.tm_yday) {
			say_today = 1;
			break;
		}
		if (tm.tm_yday == tm_now.tm_yday - 1) {
			say_yesterday = 1;
			break;
		}
		if (tm.tm_yday >= tm_now.tm_yday - 5) {
			say_dow = 1;
			break;
		}
		if (tm.tm_mon != tm_now.tm_mon) {
			say_month = say_day = say_dow = 1;
			break;
		}

		say_month = say_day = say_dow = 1;
		
		break;
	default:
		break;
	}

	if (say_today) {
		say_file("time/today.wav");
	}
	if (say_yesterday) {
		say_file("time/yesterday.wav");
	}
	if (say_dow) {
		say_file("time/day-%d.wav", tm.tm_wday);
	}
	if (say_date) {
		say_year = say_month = say_day = say_dow = 1;
		say_today = say_yesterday = 0;
	}
	if (say_day) {
		switch_snprintf(tmp, sizeof(tmp), "%u", (unsigned)tm.tm_mday);
		ru_say_count(session,tmp ,male,when,args);
	}
	if (say_month) {
		say_file("time/mon-%d.wav", tm.tm_mon);
	}
	if (say_year) {
		switch_snprintf(tmp, sizeof(tmp), "%u", (unsigned)(tm.tm_year + 1900));
		ru_say_count(session,tmp ,male,when,args);
		say_file("time/h-year.wav");
	}
	if (say_month||say_year||say_date||say_dow)
	{
	    say_file("time/at.wav");
	}
	if (say_time) {
		switch_snprintf(tmp, sizeof(tmp), "%d:%d:%d",tm.tm_hour+1,tm.tm_min,tm.tm_sec);
		ru_say_time(session, tmp, SST_TIME_MEASUREMENT, method, args);
	}
	return SWITCH_STATUS_SUCCESS;
}


        
static switch_status_t ru_say(switch_core_session_t *session, char *tosay, switch_say_type_t type, switch_say_method_t method, switch_input_args_t *args)
{
    switch_say_callback_t say_cb = NULL;
                
                    
    switch (type) {
            case SST_NUMBER:
            case SST_ITEMS:
            case SST_PERSONS:
            case SST_MESSAGES:
                say_cb = ru_say_general_count;
                break;
    	    case SST_TIME_MEASUREMENT:
                say_cb = ru_say_time;
                break;

            case SST_CURRENT_DATE:
                say_cb = ru_say_time;
                break;

            case SST_CURRENT_TIME:
                say_cb = ru_say_time;
                break;

            case SST_CURRENT_DATE_TIME:
                say_cb = ru_say_time;
                break;
            case SST_IP_ADDRESS:
//                say_cb = ru_ip;
                break;
            case SST_NAME_SPELLED:
            case SST_NAME_PHONETIC:
                say_cb = ru_spell;
                break;
            case SST_CURRENCY:
                say_cb = ru_say_money;
                break;
            default:
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown Say type=[%d]\n", type);
            break;
        }
        if (say_cb) {
	    return say_cb(session, tosay, type, method, args);
        }
        return SWITCH_STATUS_FALSE;
}
                                                                                                                                                


SWITCH_MODULE_LOAD_FUNCTION(mod_say_ru_load)
{
	switch_say_interface_t *say_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	say_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_SAY_INTERFACE);
	say_interface->interface_name = "ru";
	say_interface->say_function = ru_say;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
