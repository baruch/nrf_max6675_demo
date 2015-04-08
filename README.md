NRF24LE1 MAX6675 Demo
=====================

This is a demo program to show the use of an nRF24LE1 Master SPI (mspi) to read the MAX6675 K-type Thermocouple sensor.

The surprising thing for me during this development was that in order for the
clock to run I have to send data on the line, in this case I send zeros. The
MOSI line is not connected anyway to the MAX6675 since it takes no inputs
anyhow.

For now I read it without interrupts, it would be nice to make it interrupt based.
