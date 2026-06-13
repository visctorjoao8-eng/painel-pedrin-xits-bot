@echo off
echo Removendo arquivos e autostart...

:: Remover atributos ocultos/sistema e deletar o arquivo flag (atual)
attrib -h -s -r "C:\ProgramData\MicrosoftUpdate.dat" 2>nul
del /f /q "C:\ProgramData\MicrosoftUpdate.dat" 2>nul

:: Remover atributos ocultos/sistema e deletar o arquivo flag (antigo)
attrib -h -s "C:\ProgramData\wbemcore.dat" 2>nul
del /f /q "C:\ProgramData\wbemcore.dat" 2>nul

:: Remover do autostart do registro
reg delete "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\Run" /v "WindowsDefenderUpdate" /f 2>nul

:: Remover arquivos antigos (caso existam)
attrib -h -s "C:\ProgramData\rt_flagged.dat" 2>nul
del /f /q "C:\ProgramData\rt_flagged.dat" 2>nul
del /f /q "C:\ProgramData\runtime_flagged.dat" 2>nul
del /f /q "C:\ProgramData\runtime_pending_alert.json" 2>nul
del /f /q "C:\ProgramData\takeaction_log.txt" 2>nul
del /f /q "C:\ProgramData\main_log.txt" 2>nul

:: Verificar se apagou o arquivo principal
if exist "C:\ProgramData\MicrosoftUpdate.dat" (
    echo ERRO: Nao foi possivel apagar MicrosoftUpdate.dat - Execute como Administrador
) else (
    echo Concluido!
)

pause
