# ESP-IDF 环境激活脚本 (PowerShell)

Write-Host "正在激活 ESP-IDF 环境..." -ForegroundColor Cyan

# 设置 ESP-IDF 路径
$env:IDF_PATH = "E:\esp\Espressif\frameworks\esp-idf-v5.4.2"
$env:IDF_TOOLS_PATH = "E:\esp\Espressif\tools"

# 激活环境
& "$env:IDF_PATH\export.ps1"

# 切换到项目目录
Set-Location $PSScriptRoot

Write-Host "`nESP-IDF 环境已激活！" -ForegroundColor Green
Write-Host "项目目录: $PWD" -ForegroundColor Yellow
Write-Host "`n您现在可以运行 idf.py 命令了，例如:" -ForegroundColor Cyan
Write-Host "  idf.py build" -ForegroundColor White
Write-Host "  idf.py -p COM3 flash monitor" -ForegroundColor White
Write-Host ""
