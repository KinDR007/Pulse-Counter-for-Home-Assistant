# Mysensors Pulse Counter for Home Assistant
Pulse counter to calculate Watt and Kwh from S0 

Fix V_Var1 after first run

Fix watt report if no pulse counted

connection: 
    use resistor between D2 pin and GND (on Arduino Nano ) and connect +5Vcc from Arduino Nano to S0+ and D2 pin to S0-
   
    Radio connection    https://www.mysensors.org/build/connect_radio
    Gateway script      https://www.mysensors.org/build/serial_gateway
