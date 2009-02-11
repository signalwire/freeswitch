::
:: Build sources on win32 for Open C
::

@setlocal
@if x%AWK%==x set AWK=mawk
@set CHECK=@IF errorlevel 1 GOTO failed

:: Check that we really have awk
@%AWK% "{ exit(0); }" < NUL >NUL
@if not errorlevel 9009 goto have_awk
@echo *** install %AWK% (mawk or GNU awk) into your PATH ***
@echo *** see http://gnuwin32.sourceforge.net/packages/mawk.htm ***
@goto failed
:have_awk

@set MSG_AWK=%AWK% -v BINMODE=rw -f ../libsofia-sip-ua/msg/msg_parser.awk
:: in Win32 exit 0; from gawk 3.1.3 gets converted to errorlevel 1
:: If you have gawk 3.1.3 uncomment the following line
:: @set MSG_AWK=%AWK% -v BINMODE=rw -f ../libsofia-sip-ua/msg/msg_parser.awk success=-1
@set TAG_AWK=%AWK% -f ../libsofia-sip-ua/su/tag_dll.awk BINMODE=rw

@set IN=../libsofia-sip-ua/msg/test_class.h
@set PR=../libsofia-sip-ua/msg/test_protos.h
@set PT=../libsofia-sip-ua/msg/test_table.c

%MSG_AWK% module=msg_test NO_MIDDLE=1 NO_LAST=1 ^
  PR=%PR% %IN% < NUL
%CHECK%
%MSG_AWK% module=msg_test prefix=msg MC_HASH_SIZE=127 multipart=msg_multipart ^
  PT=%PT% %IN% < NUL
%CHECK%

@set IN=../libsofia-sip-ua/msg/sofia-sip/msg_mime.h
@set PR=../libsofia-sip-ua/msg/sofia-sip/msg_protos.h
@set PR2=../libsofia-sip-ua/msg/sofia-sip/msg_mime_protos.h
@set PT=../libsofia-sip-ua/msg/msg_mime_table.c

%MSG_AWK% module=msg NO_FIRST=1 NO_MIDDLE=1 PR=%PR% %IN% < NUL
%CHECK%
%MSG_AWK% module=msg NO_FIRST=1 NO_LAST=1 PR=%PR2% %IN% < NUL
%CHECK%
%MSG_AWK% module=msg_multipart tprefix=msg prefix=mp MC_HASH_SIZE=127 ^
  PT=%PT% %IN% < NUL
%CHECK%

@set AWK_SIP_AWK=%MSG_AWK% module=sip

@set IN=../libsofia-sip-ua/sip/sofia-sip/sip.h
@set PR=../libsofia-sip-ua/sip/sip_tag.c
@set PR2=../libsofia-sip-ua/sip/sofia-sip/sip_hclasses.h
@set PR3=../libsofia-sip-ua/sip/sofia-sip/sip_protos.h
@set PR4=../libsofia-sip-ua/sip/sofia-sip/sip_tag.h
@set PR5=../libsofia-sip-ua/sip/sofia-sip/sip_extra.h
@set SIPEXTRA=../libsofia-sip-ua/sip/sip_extra_headers.txt
@set PT=../libsofia-sip-ua/sip/sip_parser_table.c

%AWK_SIP_AWK% PR=%PR% %IN% %SIPEXTRA% < NUL
%CHECK%
%AWK_SIP_AWK% PR=%PR2% %IN% < NUL
%CHECK%
%AWK_SIP_AWK% PR=%PR3% %IN% < NUL
%CHECK%
%AWK_SIP_AWK% PR=%PR4% %IN% < NUL
%CHECK%
%AWK_SIP_AWK% PR=%PR5% NO_FIRST=1 NO_LAST=1 ^
   TEMPLATE1=%PR2%.in ^
   TEMPLATE2=%PR3%.in ^
   TEMPLATE3=%PR4%.in ^
   TEMPLATE=%PR5%.in %SIPEXTRA% < NUL
%CHECK%

%AWK_SIP_AWK% PT=%PT% TEMPLATE=%PT%.in ^
  FLAGFILE=../libsofia-sip-ua/sip/sip_bad_mask ^
  MC_HASH_SIZE=127 MC_SHORT_SIZE=26 ^
  %IN% %SIPEXTRA% < NUL
%CHECK%

@set IN=../libsofia-sip-ua/http/sofia-sip/http.h
@set PR=../libsofia-sip-ua/http/http_tag.c
@set PR2=../libsofia-sip-ua/http/sofia-sip/http_protos.h
@set PR3=../libsofia-sip-ua/http/sofia-sip/http_tag.h
@set PT=../libsofia-sip-ua/http/http_parser_table.c

%MSG_AWK% module=http PR=%PR% %IN%  < NUL
%CHECK%
%MSG_AWK% module=http PR=%PR2% %IN% < NUL
%CHECK%
%MSG_AWK% module=http PR=%PR3% %IN% < NUL
%CHECK%
%MSG_AWK% module=http MC_HASH_SIZE=127 PT=%PT% %IN% < NUL
%CHECK%

@set P=../libsofia-sip-ua

%TAG_AWK% NO_DLL=1 %P%/http/http_tag.c  < NUL
%CHECK%
%TAG_AWK% NO_DLL=1 %P%/iptsec/auth_tag.c  < NUL
%CHECK%
%TAG_AWK% NO_DLL=1 %P%/msg/msg_tag.c  < NUL
%CHECK%
%TAG_AWK% NO_DLL=1 %P%/nea/nea_tag.c  < NUL
%CHECK%
%TAG_AWK% NO_DLL=1 LIST=nta_tag_list %P%/nta/nta_tag.c  < NUL
%CHECK%
%TAG_AWK% NO_DLL=1 %P%/nth/nth_tag.c  < NUL
%CHECK%
%TAG_AWK% NO_DLL=1 LIST=nua_tag_list %P%/nua/nua_tag.c  < NUL
%CHECK%
%TAG_AWK% NO_DLL=1 %P%/sdp/sdp_tag.c  < NUL
%CHECK%
%TAG_AWK% NO_DLL=1 %P%/sip/sip_tag.c  < NUL
%CHECK%
%TAG_AWK% NO_DLL=1 LIST=soa_tag_list %P%/soa/soa_tag.c  < NUL
%CHECK%
%TAG_AWK% NO_DLL=1 %P%/su/su_tag.c  < NUL
%CHECK%
%TAG_AWK% NO_DLL=1 LIST=stun_tag_list %P%/stun/stun_tag.c  < NUL
%CHECK%
%TAG_AWK% NO_DLL=1 %P%/tport/tport_tag.c  < NUL
%CHECK%
%TAG_AWK% NO_DLL=1 %P%/url/url_tag.c  < NUL
%CHECK%

@GOTO end
:failed
@ECHO *** FAILED ***
:end
@endlocal
