#
# build and install 
# shelfman tools.

DEST_BIN=/usr/local/bin/
DEST_MAN=/usr/share/man/
SUDO_INSTALL=sudo		# INSTALL=	# writing into $DESTBIN may work with or without sudo 


shelfman-qrcode: shelfman-qrcode.py ptouch-print 
	@echo "\nNow run make ptouch-print-install, so that ptouch-print can access your USB properly."
	@echo "Or run a full make install, so that everything is in the PATH."


install: ptouch-print-install
	$(SUDO_INSTALL) install shelfman-qrcode.py $(DEST_BIN)/shelfman-qrcode


.PHONY: ptouch-print
ptouch-print-install: ptouch-print /etc/udev/rules.d/20-usb-ptouch-permissions.rules
	sudo install ptouch-print/build/ptouch-print $(DEST_BIN)/ptouch-print
	sudo install -m 644 ptouch-print/ptouch-print.1 $(DEST_MAN)/man1/ptouch-print.1
	sudo gzip -f /$(DEST_MAN)/man1/ptouch-print.1
	@echo "\nNow disconnect and reconnect USB cable, then try: ptouch-print --info"


ptouch-print: ptouch-print/build/ptouch-print


/etc/udev/rules.d/20-usb-ptouch-permissions.rules:
	sudo cp ptouch-print/udev/20-usb-ptouch-permissions.rules /etc/udev/rules.d/
	sudo udevadm control --reload-rules
	

ptouch-print/build/ptouch-print:
	git submodule update --init --recursive	# it is a submodule
	cd ptouch-print && ./compile.sh


clean:
	rm -rf ptouch-print/build
	cd ptouch-print && git restore po/ptouch.pot	# git reset --hard for one file.
	@echo "\nYou can also do: make uninstall"


uninstall:
	sudo rm -f /etc/udev/rules.d/20-usb-ptouch-permissions.rules
	sudo rm -f $(DEST_MAN)/man1/ptouch-print.1*
	sudo rm -f $(DEST_BIN)/ptouch-print
	sudo rm -f $(DEST_BIN)/shelfman-qrcode
