[Setup]
AppId={{A1B2C3D4-E5F6-7890-ABCD-SERVER123456}
AppName=局域网远程管理-服务端
AppVersion=1.0.0
AppPublisher=LanManager
DefaultDirName={autopf}\LanManager_Server
DefaultGroupName=局域网远程管理
OutputDir=C:\Users\Administrator\Desktop\juyuwangces
OutputBaseFilename=LanManager_Server_Setup
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=admin
UninstallDisplayIcon={app}\LanServer.exe

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "bin_server\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs

[Icons]
Name: "{group}\服务端"; Filename: "{app}\LanServer.exe"
Name: "{group}\卸载服务端"; Filename: "{uninstallexe}"
Name: "{commondesktop}\局域网管理-服务端"; Filename: "{app}\LanServer.exe"

[Run]
Filename: "netsh"; Parameters: "advfirewall firewall add rule name=""LanManager Server"" dir=in action=allow program=""{app}\LanServer.exe"" enable=yes"; Flags: runhidden
Filename: "netsh"; Parameters: "advfirewall firewall add rule name=""LanManager Server"" dir=out action=allow program=""{app}\LanServer.exe"" enable=yes"; Flags: runhidden
Filename: "{app}\LanServer.exe"; Description: "启动服务端"; Flags: nowait postinstall skipifsilent unchecked

[UninstallRun]
Filename: "netsh"; Parameters: "advfirewall firewall delete rule name=""LanManager Server"""; Flags: runhidden

[Code]
const
  APP_ID = '{A1B2C3D4-E5F6-7890-ABCD-SERVER123456}_is1';

function GetUninstallString(): String;
var
  sUnInstPath: String;
  sUnInstallString: String;
begin
  sUnInstPath := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\' + APP_ID;
  sUnInstallString := '';
  if not RegQueryStringValue(HKLM, sUnInstPath, 'UninstallString', sUnInstallString) then
    RegQueryStringValue(HKCU, sUnInstPath, 'UninstallString', sUnInstallString);
  Result := sUnInstallString;
end;

function IsAppInstalled(): Boolean;
begin
  Result := (GetUninstallString() <> '');
end;

function InitializeSetup(): Boolean;
var
  ResultCode: Integer;
  Choice: Integer;
  UninstallString: String;
begin
  Result := True;
  if IsAppInstalled() then
  begin
    Choice := MsgBox('检测到程序已安装，请选择操作：' + #13#10 + #13#10 +
      '点击 [是] 卸载程序' + #13#10 +
      '点击 [否] 重新安装' + #13#10 +
      '点击 [取消] 退出',
      mbConfirmation, MB_YESNOCANCEL);
    
    case Choice of
      IDYES:
      begin
        UninstallString := GetUninstallString();
        UninstallString := RemoveQuotes(UninstallString);
        Exec(UninstallString, '/SILENT', '', SW_SHOW, ewWaitUntilTerminated, ResultCode);
        Result := False;
      end;
      IDNO:
        Result := True;
      IDCANCEL:
        Result := False;
    end;
  end;
end;
