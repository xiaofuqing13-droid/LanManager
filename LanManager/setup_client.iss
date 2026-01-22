[Setup]
AppId={{B2C3D4E5-F6A7-8901-BCDE-CLIENT789012}
AppName=局域网远程管理-客户端
AppVersion=1.0.0
AppPublisher=LanManager
DefaultDirName={autopf}\LanManager_Client
DefaultGroupName=局域网远程管理
OutputDir=C:\Users\Administrator\Desktop\juyuwangces
OutputBaseFilename=LanManager_Client_Setup
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=admin
UninstallDisplayIcon={app}\LanClient.exe

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "bin_client\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs

[Icons]
Name: "{group}\客户端"; Filename: "{app}\LanClient.exe"
Name: "{group}\卸载客户端"; Filename: "{uninstallexe}"
Name: "{commondesktop}\局域网管理-客户端"; Filename: "{app}\LanClient.exe"

[Run]
Filename: "netsh"; Parameters: "advfirewall firewall add rule name=""LanManager Client"" dir=in action=allow program=""{app}\LanClient.exe"" enable=yes"; Flags: runhidden
Filename: "netsh"; Parameters: "advfirewall firewall add rule name=""LanManager Client"" dir=out action=allow program=""{app}\LanClient.exe"" enable=yes"; Flags: runhidden
Filename: "{app}\LanClient.exe"; Description: "启动客户端"; Flags: nowait postinstall skipifsilent unchecked

[UninstallRun]
Filename: "netsh"; Parameters: "advfirewall firewall delete rule name=""LanManager Client"""; Flags: runhidden

[Code]
const
  APP_ID = '{B2C3D4E5-F6A7-8901-BCDE-CLIENT789012}_is1';

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
