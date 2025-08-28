This is meant to run with wsl

Usage is:
srch driveLetter substringToMatch maxFolderDepth (up to 999)

drives should be as listed on /mnt

Needs make and gcc (likely already installed)

Build the executable with make by typing make

MAY need admin AND sudo rights when launching depending on wsl

To make accessible from any brand new termincal:
	After building the program, run the following in your terminal:

sudo cp srch /usr/bin/