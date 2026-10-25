@echo off
rem Restores the PC Settings graphics values in runtime\user\launcher_config.json (see PC_SETTINGS.md).
"%~dp0build\CTRPort-gpu\ctrport.exe" --settings-reset-graphics "%~dp0runtime\user\launcher_config.json"
if errorlevel 1 (echo Reset failed. The original file is kept.) else (echo Graphics settings restored. Start the launcher again.)
pause
