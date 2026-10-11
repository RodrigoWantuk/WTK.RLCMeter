@echo off
rem Use an activated environment, or set WTK_PYTHON to its python.exe.
if not defined WTK_PYTHON set "WTK_PYTHON=python"
"%WTK_PYTHON%" "%~dp0pc_osl_workbench.py" %*
