# ESP32-MSGEQ7


This M5Stack/MSGEQ7 Audio spectrum Visualizer is an interpretation of jollyFactory's example code provided with the MSGEQ7 Audio spectrum Visualizer Kit:

  - https://www.tindie.com/products/nick64/jf-audio-spectrum-visualizer-board-basic-kit/

It was first adapted to work with ESP32-Wrover-Kit instead of Arduino Nano.

The proto shield idea came after observing the dimensions similarities with the M5Stack proto boards.

<img width=512 src="https://raw.githubusercontent.com/tobozo/ESP32-MSGEQ7/master/doc/tindie-kit.png">

Et voilà 

<img width=512 src="https://raw.githubusercontent.com/tobozo/ESP32-MSGEQ7/master/doc/m5-proto-hat.jpeg">


Proto board Wiring
------------------

<img width=512 src="https://raw.githubusercontent.com/tobozo/ESP32-MSGEQ7/master/doc/wiring.png">

  - Reset and Strobe are connected to OUTPUT pins on the ESP32.
  - Left band and Right band are INPUT pins on the ESP32
  
Important note: The example code uses pins 25 and 26 and assumes the internal speaker is removed or had its wires cut.

![image](https://user-images.githubusercontent.com/1893754/76168734-aa53b500-6172-11ea-80fa-c167b6052f17.png)

If you aren't planning to do so, just pick two other pins :-)


Resources
---------
  - https://mix-sig.com/index.php/msgeq7-seven-band-graphic-equalizer-display-filter
  - [jollyFactory MSGEQ7 Kit](https://www.tindie.com/products/nick64/jf-audio-spectrum-visualizer-board-basic-kit/) 

Credits
-------
  - [@justcallmecoco](https://github.com/justcallmecoco) for providing motivation with the [Music visualizer](https://www.tindie.com/products/justcallmekoko/music-visualizer/) 
  - https://github.com/debsahu/ESP32_FFT_Audio_LEDs for confirming the ESP32 compatibility
  - http://www.instructables.com/id/Arduino-Based-Mini-Audio-Spectrum-Visualizer/ for providing the inspirational code
