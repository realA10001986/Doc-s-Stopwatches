
There are two PCBs: Seiko and Citizen. They are identical feature-wise, the only difference is the PCB layout. The replicas planned back in 2024 required different layouts due to battery placement.

Buttons 1 and 2 need to be momentary contacts; on/off requires a maintained contact. If you connect the positive from the power supply to "D+" instead of "S+", the switch is not required, but you need to disconnect power to disable the clock.

![Seiko](../img/schematics_seiko.jpg)

![Citizen](../img/schematics_citi.jpg)

The display is a 0.4" 4-digit LED Display, red, clock style (colon in center), common cathode. It is soldered directly onto the PCB.

![Tube](../img/tube.png)

Power supply: 3V. Rechargable CR123A battery recommended. One charge lasts about 5 hours, depending on use. 

