[Setup]
; include either setup-sdk-win32.txt or setup-sdk-x64.txt
#include "setup-sdk-win32.txt"
;#include "setup-sdk-x64.txt"

[Types]
Name: full; Description: Full installation
Name: sdk; Description: SDK installation
Name: docs; Description: Documentation installation
Name: custom; Description: Custom installation; Flags: iscustom

[Components]
Name: sdk; Description: UniMRCP SDK (client, server and plugin development); Types: full sdk
Name: docs; Description: UniMRCP documentation; Types: full docs
Name: docs\design; Description: Design concepts; Types: full docs
Name: docs\api; Description: API reference; Types: full docs

[Files]
Source: {#= uni_src}\libs\apr\include\*.h; DestDir: {app}\include; Components: sdk
Source: {#= uni_src}\libs\apr-toolkit\include\*.h; DestDir: {app}\include; Components: sdk
Source: {#= uni_src}\libs\mpf\include\*.h; DestDir: {app}\include; Components: sdk
Source: {#= uni_src}\libs\mrcp\include\*.h; DestDir: {app}\include; Components: sdk
Source: {#= uni_src}\libs\mrcp\message\include\*.h; DestDir: {app}\include; Components: sdk
Source: {#= uni_src}\libs\mrcp\control\include\*.h; DestDir: {app}\include; Components: sdk
Source: {#= uni_src}\libs\mrcp\resources\include\*.h; DestDir: {app}\include; Components: sdk
Source: {#= uni_src}\libs\mrcp-engine\include\*.h; DestDir: {app}\include; Components: sdk
Source: {#= uni_src}\libs\mrcp-signaling\include\*.h; DestDir: {app}\include; Components: sdk
Source: {#= uni_src}\libs\mrcpv2-transport\include\*.h; DestDir: {app}\include; Components: sdk
Source: {#= uni_src}\libs\mrcp-client\include\*.h; DestDir: {app}\include; Components: sdk
Source: {#= uni_src}\libs\mrcp-server\include\*.h; DestDir: {app}\include; Components: sdk
Source: {#= uni_src}\platforms\libunimrcp-client\include\*.h; DestDir: {app}\include; Components: sdk
Source: {#= uni_src}\platforms\libunimrcp-server\include\*.h; DestDir: {app}\include; Components: sdk
Source: {#= uni_src}\{#= release_dir}\lib\*.lib; DestDir: {app}\lib; Components: sdk
Source: {#= uni_src}\libs\apr\{#= release_dir}\*.lib; DestDir: {app}\lib; Components: sdk
Source: {#= uni_src}\libs\apr-util\{#= release_dir}\*.lib; DestDir: {app}\lib; Components: sdk
Source: {#= uni_src}\libs\sofia-sip\win32\libsofia-sip-ua\{#= release_dir}\*.lib; DestDir: {app}\lib; Components: sdk
Source: {#= uni_src}\build\*.h; DestDir: {app}\include; Components: sdk
Source: {#= uni_src}\build\props\sdk\*.props; DestDir: {app}\props; Components: sdk; AfterInstall: SetProjectPath(ExpandConstant('{app}\props\unimrcpsdk.props'))
Source: {#= uni_src}\build\vsprops\sdk\*.vsprops; DestDir: {app}\vsprops; Components: sdk; AfterInstall: SetProjectPath(ExpandConstant('{app}\vsprops\unimrcpsdk.vsprops'))
Source: {#= uni_src}\docs\ea\*; DestDir: {app}\doc\ea; Components: docs/design; Flags: recursesubdirs
Source: {#= uni_src}\docs\dox\*; DestDir: {app}\doc\dox; Components: docs/api; Flags: recursesubdirs

[Icons]
Name: {group}\UniMRCP Docs\Design concepts; Filename: {app}\doc\ea\index.htm; Components: docs\design
Name: {group}\UniMRCP Docs\API; Filename: {app}\doc\dox\html\index.html; Components: docs\api
Name: {group}\Uninstall; Filename: {uninstallexe}

[Code]
procedure SetProjectPath(PropertySheetFile: String);
var
  Content: String;
begin
  LoadStringFromFile (PropertySheetFile, Content);
  StringChange (Content, 'C:\Program Files\UniMRCP', ExpandConstant('{app}'));
  SaveStringToFile (PropertySheetFile, Content, False);
end;
