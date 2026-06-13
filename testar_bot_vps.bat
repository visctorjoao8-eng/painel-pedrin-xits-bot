@echo off
setlocal

set "BOT_HOST=15.228.83.81"
set "BOT_PORT=3001"
set "BASE_URL=http://%BOT_HOST%:%BOT_PORT%"

echo.
echo Testando bot em %BASE_URL%
echo.

echo [1/2] Testando /health...
powershell -NoProfile -ExecutionPolicy Bypass -Command "try { $r = Invoke-RestMethod -Uri '%BASE_URL%/health' -TimeoutSec 10; Write-Host 'OK /health:' ($r | ConvertTo-Json -Compress) } catch { Write-Host 'ERRO /health:' $_.Exception.Message; exit 1 }"
if errorlevel 1 goto erro

echo.
echo [2/2] Enviando mensagem de teste para /alert...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$body = @{ content = 'Painel iniciado - teste manual via BAT'; sessionId = 'teste-bat'; browsers = @() } | ConvertTo-Json -Compress; try { $r = Invoke-RestMethod -Method Post -Uri '%BASE_URL%/alert' -ContentType 'application/json' -Body $body -TimeoutSec 15; Write-Host 'OK /alert:' ($r | ConvertTo-Json -Compress) } catch { Write-Host 'ERRO /alert:' $_.Exception.Message; try { $reader = New-Object System.IO.StreamReader($_.Exception.Response.GetResponseStream()); $text = $reader.ReadToEnd(); if ($text) { Write-Host 'Resposta:' $text } } catch {}; if ($_.ErrorDetails.Message) { Write-Host $_.ErrorDetails.Message }; exit 1 }"
if errorlevel 1 goto erro

echo.
echo Teste finalizado. Confira se a mensagem chegou no Discord.
pause
exit /b 0

:erro
echo.
echo Falhou. Verifique se:
echo - o bot esta aberto na VPS;
echo - a porta %BOT_PORT% esta liberada no firewall;
echo - o IP/porta estao corretos neste arquivo.
pause
exit /b 1
