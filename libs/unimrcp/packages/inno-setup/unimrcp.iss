[Setup]
; include either setup-win32.txt or setup-x64.txt
#include "setup-win32.txt"
;#include "setup-x64.txt"

[Types]
Name: full; Description: Full installation
Name: server; Description: Server installation
Name: client; Description: Client installation
Name: custom; Description: Custom installation; Flags: iscustom

[Components]
Name: server; Description: UniMRCP server; Types: full server
Name: server\recorder; Description: Recorder plugin; Types: full server
Name: server\demosynth; Description: Demo synthesizer plugin; Types: full server
Name: server\demorecog; Description: Demo recognizer plugin; Types: full server
Name: server\demoverifier; Description: Demo verifier plugin; Types: full server
Name: client; Description: UniMRCP client (sample applications); Types: full client

[Dirs]
Name: {app}\data; Permissions: everyone-full
Name: {app}\log; Permissions: everyone-full

[Files]
Source: {#= uni_outdir}\bin\unimrcpserver.exe; DestDir: {app}\bin; Components: server
Source: {#= uni_outdir}\bin\unimrcpservice.exe; DestDir: {app}\bin; Components: server
Source: {#= uni_outdir}\bin\unimrcpclient.exe; DestDir: {app}\bin; Components: client
Source: {#= uni_outdir}\bin\umc.exe; DestDir: {app}\bin; Components: client
Source: {#= uni_outdir}\bin\*.dll; DestDir: {app}\bin; Components: server client
Source: {#= uni_outdir}\plugin\mrcprecorder.dll; DestDir: {app}\plugin; Components: server/recorder
Source: {#= uni_outdir}\plugin\demosynth.dll; DestDir: {app}\plugin; Components: server/demosynth
Source: {#= uni_outdir}\plugin\demorecog.dll; DestDir: {app}\plugin; Components: server/demorecog
Source: {#= uni_outdir}\plugin\demoverifier.dll; DestDir: {app}\plugin; Components: server/demoverifier
Source: {#= uni_outdir}\conf\unimrcpserver.xml; DestDir: {app}\conf; Components: server
Source: {#= uni_outdir}\conf\unimrcpclient.xml; DestDir: {app}\conf; Components: client
Source: {#= uni_outdir}\conf\client-profiles\*.xml; DestDir: {app}\conf\client-profiles; Components: client
Source: {#= uni_outdir}\conf\umcscenarios.xml; DestDir: {app}\conf; Components: client
Source: {#= uni_outdir}\data\*.pcm; DestDir: {app}\data; Components: server client
Source: {#= uni_outdir}\data\*.xml; DestDir: {app}\data; Components: server client
Source: {#= uni_outdir}\data\*.txt; DestDir: {app}\data; Components: server client

[Icons]
Name: {group}\UniMRCP Server Console; Filename: {app}\bin\unimrcpserver.exe; Parameters: "--root-dir ""{app}"""; Components: server
Name: {group}\UniMRCP Client Console; Filename: {app}\bin\unimrcpclient.exe; Parameters: "--root-dir ""{app}"""; Components: client
Name: {group}\UniMRCP Service\Start Server; Filename: {app}\bin\unimrcpservice.exe; Parameters: --start; Components: server
Name: {group}\UniMRCP Service\Stop Server; Filename: {app}\bin\unimrcpservice.exe; Parameters: --stop; Components: server
Name: {group}\Uninstall; Filename: {uninstallexe}

[Run]
Filename: {app}\bin\unimrcpservice.exe; Description: Register service; Parameters: "--register ""{app}"""; Components: server

[UninstallRun]
Filename: {app}\bin\unimrcpservice.exe; Parameters: --unregister; Components: server

[Code]
var
  Content: String;

procedure ModifyPluginConf(PluginName: String; Enable: Boolean);
var
  TextFrom: String;
  TextTo: String;
begin
  if Enable = True then
  begin
    TextFrom := 'class="' + PluginName + '" enable="0"';
    TextTo := 'class="' + PluginName + '" enable="1"';
  end
  else
  begin
    TextFrom := 'class="' + PluginName + '" enable="1"';
    TextTo := 'class="' + PluginName + '" enable="0"';
  end
  StringChange (Content, TextFrom, TextTo);
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  CfgFile: String;
begin
  if CurStep = ssPostInstall then
  begin
    CfgFile := ExpandConstant('{app}\conf\unimrcpserver.xml');
    LoadStringFromFile (CfgFile, Content);
    ModifyPluginConf ('mrcprecorder', IsComponentSelected('server\recorder'));
    ModifyPluginConf ('demosynth', IsComponentSelected('server\demosynth'));
    ModifyPluginConf ('demorecog', IsComponentSelected('server\demorecog'));
    ModifyPluginConf ('demoverifier', IsComponentSelected('server\demoverifier'));
    SaveStringToFile (CfgFile, Content, False);
  end
end;
