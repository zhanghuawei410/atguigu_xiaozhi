@echo off
REM ESP-IDF 环境激活脚本

echo 正在激活 ESP-IDF 环境...

REM 设置 ESP-IDF 路径
set IDF_PATH=E:\esp\Espressif\frameworks\esp-idf-v5.4.2
set IDF_TOOLS_PATH=E:\esp\Espressif\tools

REM 激活环境
call %IDF_PATH%\export.bat

REM 切换到项目目录
cd /d %~dp0

echo.
echo ESP-IDF 环境已激活！
echo 项目目录: %CD%
echo.
echo 您现在可以运行 idf.py 命令了，例如:
echo   idf.py build
echo   idf.py -p COM3 flash monitor
echo.

cmd /k
