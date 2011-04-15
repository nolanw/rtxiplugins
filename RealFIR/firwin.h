#ifndef FIRWIN_H
#define FIRWIN_H

#include <vector>

using namespace std;

// Fill up |coefficients| with some coefficients you can convolve with an 
// input signal to attenuate frequencies outside of |passband|. |passband| 
// should be normalized between zero and one (inclusive), where one is the 
// Nyquist frequency of your signal.
// Note that a lowpass filter with cutoff 'f' could give passband as (0, f), 
// and a highpass filter could give passband as (f, 1).
// You're on your own for bandstop. (Or just use two filters.)
// The size of |coefficients| is taken as the number of taps, and should be odd.
void firwin(vector<double> &coefficients, pair<double, double> &passband);

#endif /* end of include guard: FIRWIN_H */
