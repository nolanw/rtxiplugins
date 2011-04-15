
#include <realfir.h>
#include "firwin.h"

extern "C" Plugin::Object *createRTXIPlugin(void) {
    return new RealFIR();
}

#define PARAM_PASSBAND_LOW "Low end of passband (Hz)"
#define PARAM_PASSBAND_HIGH "High end of passband (Hz)"
#define PARAM_SAMPLING_RATE "Sampling rate (Hz)"
#define PARAM_FILTER_TAPS "Number of filter taps"

#define INITIAL_PASSBAND_LOW 5
#define INITIAL_PASSBAND_HIGH 25
#define INITIAL_SAMPLING_RATE 1000.0
#define INITIAL_FILTER_TAPS 61

static DefaultGUIModel::variable_t vars[] = {
  {
    "Vin",
    "Input signal",
    DefaultGUIModel::INPUT,
  },
  {
    "Vout (filtered)",
    "Filtered output signal",
    DefaultGUIModel::OUTPUT,
  },
  {
    PARAM_PASSBAND_LOW,
    "Attenuate frequencies below this frequency",
    DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
  },
  {
    PARAM_PASSBAND_HIGH,
    "Attenuate frequencies above this frequency",
    DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
  },
  {
    PARAM_SAMPLING_RATE,
    "Sample the input signal this often (Hz). Make this at least double the "
    "highest frequency you wish to filter",
    DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
  },
  {
    PARAM_FILTER_TAPS,
    "Number of taps (coefficients) in the filter. More taps means sharper "
    "cutoff but slower execution",
    DefaultGUIModel::PARAMETER | DefaultGUIModel::UINTEGER,
  },
};

static size_t num_vars = sizeof(vars)/sizeof(DefaultGUIModel::variable_t);

RealFIR::RealFIR(void) : DefaultGUIModel("RealFIR", ::vars, ::num_vars) {
  // Set defaults for each parameter.
  setParameter(PARAM_PASSBAND_LOW, INITIAL_PASSBAND_LOW);
  setParameter(PARAM_PASSBAND_HIGH, INITIAL_PASSBAND_HIGH);
  setParameter(PARAM_SAMPLING_RATE, INITIAL_SAMPLING_RATE);
  setParameter(PARAM_FILTER_TAPS, INITIAL_FILTER_TAPS);
  
  // Get the coefficients for the defaults, and the realtime period.
  update(MODIFY);
  update(PERIOD);
  
  refresh();
}

RealFIR::~RealFIR(void) {}

void RealFIR::execute(void) {
  elapsedTime += dt_s;
  
  // Grab a sample if we need it.
  if (elapsedTime - lastSample >= 1.0 / samplingRate)
  {
    buffer.push_back(input(0));
    lastSample = elapsedTime;
  }
  
  // Convolve.
  accum = 0;
  for (i = 0; i < buffer.size(); i++)
    accum += buffer[i] * coefficients[i];
  output(0) = accum;
}

void RealFIR::update(DefaultGUIModel::update_flags_t flag) {
  size_t taps;
  switch(flag) {
    // Cut output on pause; it'll restart in execute() on unpause.
    case PAUSE:
      output(0) = 0;
      break;
    
    // Grab the new parameters and make a new filter.
    case MODIFY:
      samplingRate = getParameter(PARAM_SAMPLING_RATE).toDouble();
      passband = std::make_pair(getParameter(PARAM_PASSBAND_LOW).toDouble(), 
                                getParameter(PARAM_PASSBAND_HIGH).toDouble());
      passband.first /= samplingRate / 2.0;
      passband.second /= samplingRate / 2.0;
      taps = getParameter(PARAM_FILTER_TAPS).toUInt();
      // Ensure an odd number of taps.
      coefficients.resize(taps + ((taps % 2 == 0) ? 1 : 0));
      firwin(coefficients, passband);
      // Size and zero the buffer.
      buffer.clear();
      buffer.resize(coefficients.size());
      lastSample = elapsedTime = 0.0;
      break;
    
    // Grab the realtime period in seconds.
    case PERIOD:
      dt_s = RT::System::getInstance()->getPeriod() * 1e-9;
      break;
    
    default:
      break;
  }
}
