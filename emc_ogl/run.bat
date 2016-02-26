@ECHO OFF
call build.bat
if NOT ERRORLEVEL 0 goto end
start main.html
:end
rem python tests/runner.py browser.test_cubegeom_pre_vao
