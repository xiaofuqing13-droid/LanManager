@echo off
chcp 65001 >nul
setlocal

:: =============================================
:: 测试安装程序 - 用于测试局域网远程分发功能
:: 支持参数: /S /silent /quiet (静默安装)
:: =============================================

set "INSTALL_DIR=%ProgramFiles%\TestApp"
set "LOG_FILE=%TEMP%\TestInstaller.log"
set "SILENT=0"

:: 解析命令行参数
:parse_args
if "%~1"=="" goto main
if /i "%~1"=="/S" set "SILENT=1"
if /i "%~1"=="/silent" set "SILENT=1"
if /i "%~1"=="/quiet" set "SILENT=1"
if /i "%~1"=="/?" goto show_help
if /i "%~1"=="/h" goto show_help
shift
goto parse_args

:show_help
echo.
echo 测试安装程序 v1.0
echo 用法: TestInstaller.bat [选项]
echo.
echo 选项:
echo   /S, /silent, /quiet  静默安装模式
echo   /?, /h               显示此帮助信息
echo.
exit /b 0

:main
:: 记录开始时间
echo [%date% %time%] 安装开始 >> "%LOG_FILE%"
echo [%date% %time%] 静默模式: %SILENT% >> "%LOG_FILE%"
echo [%date% %time%] 安装目录: %INSTALL_DIR% >> "%LOG_FILE%"

if "%SILENT%"=="0" (
    echo.
    echo ========================================
    echo     测试安装程序 v1.0
    echo ========================================
    echo.
    echo 安装目录: %INSTALL_DIR%
    echo.
    echo 正在安装，请稍候...
    echo.
)

:: 模拟安装过程
if "%SILENT%"=="0" echo [1/5] 检查系统环境...
ping -n 2 127.0.0.1 >nul

if "%SILENT%"=="0" echo [2/5] 创建安装目录...
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%" 2>nul

if "%SILENT%"=="0" echo [3/5] 复制文件...
ping -n 2 127.0.0.1 >nul

:: 创建测试文件
echo TestApp v1.0 > "%INSTALL_DIR%\TestApp.txt"
echo 安装时间: %date% %time% >> "%INSTALL_DIR%\TestApp.txt"
echo 安装来源: 局域网远程分发测试 >> "%INSTALL_DIR%\TestApp.txt"

if "%SILENT%"=="0" echo [4/5] 写入注册表...
ping -n 2 127.0.0.1 >nul

:: 写入卸载信息到注册表
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\TestApp" /v "DisplayName" /t REG_SZ /d "测试应用程序" /f >nul 2>&1
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\TestApp" /v "DisplayVersion" /t REG_SZ /d "1.0.0" /f >nul 2>&1
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\TestApp" /v "Publisher" /t REG_SZ /d "测试发布者" /f >nul 2>&1
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\TestApp" /v "InstallLocation" /t REG_SZ /d "%INSTALL_DIR%" /f >nul 2>&1
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\TestApp" /v "UninstallString" /t REG_SZ /d "\"%INSTALL_DIR%\Uninstall.bat\"" /f >nul 2>&1
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\TestApp" /v "InstallDate" /t REG_SZ /d "%date:~0,4%%date:~5,2%%date:~8,2%" /f >nul 2>&1

:: 创建卸载脚本
(
echo @echo off
echo chcp 65001 ^>nul
echo setlocal
echo set "SILENT=0"
echo if /i "%%~1"=="/S" set "SILENT=1"
echo if /i "%%~1"=="/silent" set "SILENT=1"
echo if /i "%%~1"=="/quiet" set "SILENT=1"
echo if "%%SILENT%%"=="0" ^(
echo     echo.
echo     echo 正在卸载测试应用程序...
echo     echo.
echo ^)
echo reg delete "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\TestApp" /f ^>nul 2^>^&1
echo rd /s /q "%INSTALL_DIR%" 2^>nul
echo if "%%SILENT%%"=="0" ^(
echo     echo 卸载完成！
echo     pause
echo ^)
) > "%INSTALL_DIR%\Uninstall.bat"

if "%SILENT%"=="0" echo [5/5] 完成安装...
ping -n 1 127.0.0.1 >nul

:: 记录完成
echo [%date% %time%] 安装完成 >> "%LOG_FILE%"

if "%SILENT%"=="0" (
    echo.
    echo ========================================
    echo     安装完成！
    echo ========================================
    echo.
    echo 安装位置: %INSTALL_DIR%
    echo.
    pause
)

exit /b 0
