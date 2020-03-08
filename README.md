# ESP32-MSGEQ7


This M5Stack/MSGEQ7 Audio spectrum Visualizer is an interpretation of jollyFactory's example code provided with the MSGEQ7 Audio spectrum Visualizer Kit:

  - https://www.tindie.com/products/nick64/jf-audio-spectrum-visualizer-board-basic-kit/

It was first adapted to work with ESP32-Wrover-Kit instead of Arduino Nano.

The proto shield idea came after observing the dimensions similarities with the M5Stack proto boards.

<img width=512 src=tindie-kit.png>

Et voilà 

<img width=512 src=m5-proto-hat.jpeg>

Proto board Wiring
------------------

<img width=512 src=wiring.png>

  - Reset and Strobe are connected to OUTPUT pins on the ESP32.
  - Left band and Right band are INPUT pins on the ESP32

Resources
---------
  - https://mix-sig.com/index.php/msgeq7-seven-band-graphic-equalizer-display-filter
  - [jollyFactory MSGEQ7 Kit](https://www.tindie.com/products/nick64/jf-audio-spectrum-visualizer-board-basic-kit/) 

Credits
-------
  - [@justcallmecoco](https://github.com/justcallmecoco) for providing motivation with the [Music visualizer](https://www.tindie.com/products/justcallmekoko/music-visualizer/) 
  - https://github.com/debsahu/ESP32_FFT_Audio_LEDs for confirming the ESP32 compatibility
  - http://www.instructables.com/id/Arduino-Based-Mini-Audio-Spectrum-Visualizer/ for providing the inspirational code
