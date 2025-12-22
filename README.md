# Shelfman
Inventory Management with QR Codes and Photos

I am using a brother P-Touch D410 label printer to generate QR codes.
The printer can use tape 18mm wide, this allows for nice QR codes size 116x116 pixels.

The python script shelfman-qrcode.py generates UUID4 based QR codes and sends them to the printer using the ptouch tool
by Dominic Radermacher available from https://git.familie-radermacher.ch/linux/ptouch-print.git
