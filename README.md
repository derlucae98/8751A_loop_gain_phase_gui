# HP 8751A Suite
Qt based GUI for transfer function measurements as well as universal impedance measurements using a HP 8751A network analyzer via GPIB.

# Capabilities

- Remote control the HP 8751A via GPIB using a Prologix GPIB-Ethernet adapter or compatible device. 
- Initialize the instrument with basic measurement parameters for transfer function or impedance measurements
- Preview of the measured data
- Export measured data as CSV or image

# Additional requirements

- For impedance measurements, a low frequency four-port (dual) directional coupler is needed
- For transfer function measurements on power supplies an injection transformer and two input amplifiers are needed. The amplifiers convert the 50Ω input impedance of the instrument to 1MΩ. Aditionally, the amplifiers provide protection against overloading the inputs. A BUF802 based pre-amplifier is used here.

# Screenshots

![Impedance measurement](https://github.com/derlucae98/8751A_loop_gain_phase_gui/blob/939ebeb1e4a35a27011c9ce74297129ae5231c88/documentation/impedance.png "Impedance measurement")

