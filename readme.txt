clipMon v1.0.1 - A Logitech G15 LCD Clipboard Monitoring program. (06/06/2006 6:00pm)

1.0 Features
	- Display of both copied text and file names of copied files
	- Optional word wrapping, clipboard alerting, and lcd display mode (black on white / white on black) configurable in LCD manager. (Located in Control panel)
	- Unicode and plain Ascii text support. (Unicode uses two bytes (16 bits) per character to allow for the wide array of characters used in some languages.
	- x86-64 binaries included and tested on Microsoft Windows XP x64 Edition.
	- Complete Source code included for personal use.

1.1 Installation
	- No installer is included at the present time, if there is sufficient demand it will be included as an optional package.
	- To install simply copy the clipmon.exe of your choosing to the directory you wish for it to reside. I suggest using "C:\Program Files\Logitech\G-Series Software\Applets" as it is home to Logitech's stock applications.
	- After running the file for the first time it will register itself in the lcd manager and automatically start with Windows. If you wish to change this behavior you may do so from the LCD manager located in Control Panel.

1.2 Usage
	- clipMon will automatically update the lcd display without direct interaction, the leftmost button will close the application when it is in focus.
	- Four options exist in the configuration page accessible through the LCD manager.
		Negate LCD will negate the image displayed on the lcd switching text from black to white and the background from white to black.
		Clip Alert toggles the three second popup event when text or files are placed into the clipboard. This feature will not hide the application after three seconds until the next Logitech driver release and it is recommended that you leave it off.
		Word Wrap will toggle word wrapping of text displayed on the lcd.
		The Font button will allow you to select any currently installed windows font, further configuration of the fonts specifics is available by directly changing values stored within the clipMon.ini.
		
	- If changes are made to the applications default settings they will be stored in clipMon.ini located either in documents and settings\<your user name> or in the folder you installed the application depending on which version of logitech's LCD subsystem is running.

1.3 Removal
	- To remove clipMon, simply close the application as described in the usage section and delete clipMon.exe and clipMon.ini (if it exists).
	- To remove clipMon from the LCD manager's list of applications simply uncheck the enable checkbox after deleting the application. It will then be removed automatically upon next logon.

-Change Log-
Changes in this clipMon v1.0.1
	- Added x64 manifest file for xp style client area controls
	- Includes silently released fix for stupid mistake (forgot to update lcd after printing "Clipboard Empty" when doing last minute changes)

-Old readme-
I could make an installer for this, but personally hate using add/remove programs. ;o)

Uh... I usually put my exe in the logitech applet folder "C:\Program Files\Logitech\G-Series Software\Applets"

It will start with default options and make a clipMon.ini file to store those options in its folder. You can configure it through lcd manager. To close the program press the first button. Source code is included feel free to take a looksy, if you want to point out something I did thats wrong or can generally be approved send an email to clipmon@gotomy.com (I'll register that email account later, still havent registered that email yet ;o)

This readme sucks and I know it, I may revise it with a minor revision of the program if I or some one points out something worth fixing.

-Lord of Shadows
06/05/2006 8:30pm