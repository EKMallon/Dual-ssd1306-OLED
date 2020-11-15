<img src="https://github.com/EKMallon/The_Cave_Pearl_Project_CURRENT_codebuilds/blob/master/images/CavePearlProjectBanner_130x850px.jpg">

# Adding Dual ssd1306 OLED displays to your Arduino logger

See blog post at: https://thecavepearlproject.org/2020/11/15/adding-two-oled-displays-to-your-arduino-logger-with-no-library/

<img align="left" src="https://github.com/EKMallon/Dual-ssd1306-OLED/blob/main/images/DualOLEDscreens_1_300px.jpg" height="171" width="300">
This is a mash up of different code concepts for Arduino projects using two 0.96" I2C screens simultaneously. It builds upon Julian Iletts shift-out method for displaying text on SPI displays: https://www.youtube.com/watch?v=RAlZ1DHw03g&list=PLjzGSu1yGFjXWp5F4BPJg4ZJFbJcWeRzk&ab_channel=JulianIlett  which and I've adapted that to store font bitmaps in the internal eeprom & updated to work with the standard wire.h library.   The function plotter is by David Johnson-Davies http://www.technoblogy.com/show?2CFT tweaked slightly to add dashed lead lines in the background.  The horizontal progress bar function is by Edward Mallon.

&nbsp;

This code is not optimized, and has much duplication that could be eliminated for efficiency. 
It is intended primarily to provide extension activities for students working with our '2020 Classroom Logger' @  
https://thecavepearlproject.org/2020/10/22/pro-mini-classroom-datalogger-2020-update/ 

<img src="https://github.com/EKMallon/Dual-ssd1306-OLED/blob/main/images/2020_dualOLEDscreens_ClassroomLoggerBuild_640px.jpg" height="372" width="640">
