@echo off

pushd D:\Dev\C\lcl
if not exist build (
    mkdir build
)
cl -Zi -W4 -WX -wd4201 -wd4701 -wd4703 -wd4715 -Fe:build\ -Fo:build\ -Fd:build\ lcom.c
popd
