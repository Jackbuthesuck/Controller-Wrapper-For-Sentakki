\tools\devcon\i386\devcon.exe remove djpnewton\vmulti 
cd .. 
cmd /c buildme.bat 
cd bin 
cmd /c install_driver.bat 
echo After keypress, launching testvmulti 
pause 
testvmulti /joystick 
