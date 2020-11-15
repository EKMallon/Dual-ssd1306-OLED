<img src="https://github.com/EKMallon/The_Cave_Pearl_Project_CURRENT_codebuilds/blob/master/images/CavePearlProjectBanner_130x850px.jpg">

# Adding Dual ssd1306 OLED displays to your Arduino logger

This is an mash up of different code concepts for Arduino projects using two 0.96" I2C screens to display text and graphic information simultaneously.
It is primarily intended to provide extension exerciseses for students working with our 2020 'Classroom Logger' @  
https://thecavepearlproject.org/2020/10/22/pro-mini-classroom-datalogger-2020-update/ 

It builds upon Julian Iletts shift-out method for controlling SPI screens: https://www.youtube.com/watch?v=RAlZ1DHw03g&list=PLjzGSu1yGFjXWp5F4BPJg4ZJFbJcWeRzk&ab_channel=JulianIlett  which I've addpted to store fonts in the internal eeprom & updated to work with standard wire.h library. 

The function plotter is by David Johnson-Davies http://www.technoblogy.com/show?2CFT and I've tweaked that slightly to add background lead lines. 

The horizontal progress bar function is by Edward Mallon.

<img src="https://github.com/EKMallon/Dual-ssd1306-OLED/blob/main/images/2020_dualOLEDscreens_ClassroomLoggerBuild_640px.jpg" height="372" width="640">
