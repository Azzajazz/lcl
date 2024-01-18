@echo off

pushd D:\Dev\C\lcl
if not exist build (
    mkdir build
)
cl -Zi -Fe:build\ -Fo:build\ -Fd:build\ lcom.c
popd
