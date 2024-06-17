The ZIP file inside the Gerber folder can be used to order PCBs from manufacturers like JLCPCB (cheapest) or PCBWAY.

The Schematics are not in sync with the layout of the board as I changed some footprints, added a second relay header and modified names.

Tested until now:
* Rst-Button
* Mode-Switch
* Display
* Arduino
* Relay
* Both DHT22 sensors (fixed silkscreen between 0.3 and 0.4)

Untested:
* DS1307 -> did not work. Module seems to be brokan as it does not work with a test setup
* SD card slot 