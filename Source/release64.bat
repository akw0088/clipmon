REM Must use x64 cl.exe!
@echo off
del manifest.xml
copy manifest64.xml manifest.xml
rc resource.rc
cl clipMon.c /Ox /GA /favor:AMD64 /Zc:forScope,wchar_t /link resource.res kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ComCtl32.Lib lgLcd.lib