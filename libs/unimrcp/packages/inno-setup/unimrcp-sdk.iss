[Setup]
#include "setup.iss"
OutputBaseFilename=unimrcp-sdk-{#= uni_version}

[Types]
Name: "full"; Description: "Full installation"
Name: "sdk"; Description: "SDK installation"
Name: "docs"; Description: "Documentation installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "sdk"; Description: "UniMRCP SDK (client, server and plugin development)"; Types: full sdk
Name: "docs"; Description: "UniMRCP documentation"; Types: full docs
Name: "docs\design"; Description: "Design concepts"; Types: full docs
Name: "docs\api"; Description: "API"; Types: full docs

[Files]
Source: "..\..\libs\apr\include\*.h"; DestDir: "{app}\include"; Components: sdk
Source: "..\..\libs\apr-toolkit\include\*.h"; DestDir: "{app}\include"; Components: sdk
Source: "..\..\libs\mpf\include\*.h"; DestDir: "{app}\include"; Components: sdk
Source: "..\..\libs\mrcp\include\*.h"; DestDir: "{app}\include"; Components: sdk
Source: "..\..\libs\mrcp\message\include\*.h"; DestDir: "{app}\include"; Components: sdk
Source: "..\..\libs\mrcp\control\include\*.h"; DestDir: "{app}\include"; Components: sdk
Source: "..\..\libs\mrcp\resources\include\*.h"; DestDir: "{app}\include"; Components: sdk
Source: "..\..\libs\mrcp-engine\include\*.h"; DestDir: "{app}\include"; Components: sdk
Source: "..\..\libs\mrcp-signaling\include\*.h"; DestDir: "{app}\include"; Components: sdk
Source: "..\..\libs\mrcpv2-transport\include\*.h"; DestDir: "{app}\include"; Components: sdk
Source: "..\..\libs\mrcp-client\include\*.h"; DestDir: "{app}\include"; Components: sdk
Source: "..\..\libs\mrcp-server\include\*.h"; DestDir: "{app}\include"; Components: sdk
Source: "..\..\platforms\libunimrcp-client\include\*.h"; DestDir: "{app}\include"; Components: sdk
Source: "..\..\platforms\libunimrcp-server\include\*.h"; DestDir: "{app}\include"; Components: sdk
Source: "..\..\Release\bin\*.lib"; DestDir: "{app}\lib"; Components: sdk
Source: "..\..\libs\apr\Release\*.lib"; DestDir: "{app}\lib"; Components: sdk
Source: "..\..\libs\apr-util\Release\*.lib"; DestDir: "{app}\lib"; Components: sdk
Source: "..\..\libs\sofia-sip\win32\libsofia-sip-ua\Release\*.lib"; DestDir: "{app}\lib"; Components: sdk
Source: "..\..\build\vsprops\sdk\*.vsprops"; DestDir: "{app}\vsprops"; Components: sdk; AfterInstall: SetProjectPath()
Source: "..\..\docs\ea\*"; DestDir: "{app}\doc\ea"; Components: docs/design; Flags: recursesubdirs
Source: "..\..\docs\dox\*"; DestDir: "{app}\doc\dox"; Components: docs/api; Flags: recursesubdirs

[Icons]
Name: "{group}\UniMRCP Docs\Design concepts"; Filename: "{app}\doc\ea\index.htm"; Components: docs\design
Name: "{group}\UniMRCP Docs\API"; Filename: "{app}\doc\dox\html\index.html"; Components: docs\api
Name: "{group}\Uninstall"; Filename: "{uninstallexe}"

[Code]
procedure SetProjectPath();
var
  VspropsFile: String;
  Content: String;
begin
  VspropsFile := ExpandConstant('{app}\vsprops\unimrcpsdk.vsprops');
  LoadStringFromFile (VspropsFile, Content);
  StringChange (Content, 'Value="C:\Program Files\UniMRCP"', ExpandConstant('Value="{app}"'));
  SaveStringToFile (VspropsFile, Content, False);
end;

