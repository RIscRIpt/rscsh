@echo off
git describe --tags --long > version.ver
set /p VER=<version.ver
echo #define VERSION ^"%VER%^" > version.ver
