# invoke SourceDir generated makefile for app_ble.pem3
app_ble.pem3: .libraries,app_ble.pem3
.libraries,app_ble.pem3: package/cfg/app_ble_pem3.xdl
	$(MAKE) -f /home/spendice/Documents/F91_Kepler/Firmware/f91_kepler_app/TOOLS/src/makefile.libs

clean::
	$(MAKE) -f /home/spendice/Documents/F91_Kepler/Firmware/f91_kepler_app/TOOLS/src/makefile.libs clean

