@echo off
cd /d "%~dp0.."
dotnet run --project "%~dp0leveleditor_clean.csproj" --configuration Release
if errorlevel 1 pause
