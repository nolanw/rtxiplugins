#include <vector>
#include <cmath>
#include <iostream>

using namespace std;

// NaN-free sinc.
inline double sinc(double x)
{
  return (x == 0.0 ? 1.0 : sin(M_PI * x) / (M_PI * x));
}

// Snipped from scipy 0.9.
void firwin(vector<double> &coefficients, pair<double, double> &passband)
{
  size_t numtaps = coefficients.size();
  double left    = passband.first, 
         right   = passband.second;
  double alpha   = 0.5 * (numtaps - 1);
  double scale   = 0.5 * (left + right);
  double accum   = 0.0;
  
  for (size_t i = 0; i < numtaps; i++) {
    // Idealized coefficient.
    double m = i - alpha;
    double h1 = right * sinc(right * m);
    double h2 = left * sinc(left * m);
    
    // Windowed.
    double w = 0.54 - 0.46 * cos(2.0 * M_PI * i / (numtaps - 1));
    coefficients[i] = (h1 - h2) * w;
    
    // Bookkeeping for scaling.
    accum += cos(M_PI * m * scale) * coefficients[i];
  }
  
  // Scaling.
  for (size_t i = 0; i < numtaps; i++) {
    coefficients[i] /= accum;
  }
}
