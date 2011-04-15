RTXI Plugins
============

I wrote a few plugins for [RTXI](http://rtxi.org). Here they are for the world to see.

  * **Istep**
    A stepping voltage (presumably put through an amplifier), plotted below 
    the return voltage, so you can see if your cells might die.
    
    Requires [Qwt](http://qwt.sourceforge.net/).
  
  * **mux**
    Combine multiple plugins' inputs in useful ways.
  
  * **noise**
    Make random noise.
  
  * **oscilloscope**
    Very slightly modified from the original so there's no windup time.
  
  * **ramp**
    A signal that ramps up controllably randomly.
  
  * **RealFIR**
    A finite impulse response filter that doesn't crash.
    
    Requires [boost circular_buffer](http://www.boost.org/doc/libs/1_40_0/libs/circular_buffer/doc/circular_buffer.html)
  
  * **sample_player**
    Play back some crazy signal you made elsewhere without breaking realtime.
  
  * **sine**
    Sine wave generator.
  
  * **square**
    Square wave generator.
  
  * **variancer**
    Calculate and display the variance ratio between a sine wave and the 
    resultant signal.
