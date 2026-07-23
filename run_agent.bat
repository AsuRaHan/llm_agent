@echo off
setlocal

set "project_path=E:\my_proj\test_ai"
echo Starting agent for: %project_path%
:: Замените "path/to/your/agent.exe" на реальный путь к вашему .exe файлу
"E:\my_proj\llm_agent\Agent.exe" "%project_path%"
pause
