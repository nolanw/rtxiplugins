/*
 * Copyright (C) 2004 Boston University
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

// Boilerplate for typical RTXI plugins.
#include <ramp.h>

extern "C" Plugin::Object *createRTXIPlugin(void) {
    return new Ramp();
}

// All the variables that RTXI needs to know about. Inputs and outputs will 
// appear in the Connector Panel and should be designated 
// `DefaultGUIModel::INPUT` or `DefaultGUIModel::OUTPUT` as needed.

#define PARAM_RATE_MIN "Rate min (mV/ms)"
#define PARAM_RATE_MAX "Rate max (mV/ms)"
#define PARAM_INTERVAL_MIN "Interval min (ms)"
#define PARAM_INTERVAL_MAX "Interval max (ms)"
#define PARAM_V_MAX "Vmax"

#define INITIAL_RATE_MIN 1.0
#define INITIAL_RATE_MAX 20.0
#define INITIAL_INTERVAL_MIN 500.0
#define INITIAL_INTERVAL_MAX 2000.0

static DefaultGUIModel::variable_t vars[] = {
  {
    "SpikeDetect state",
    "Connect this to a SpikeDetect's output so we can cut our output",
    DefaultGUIModel::INPUT,
  },
  {
    "Vout",
    "Output of this ramp",
    DefaultGUIModel::OUTPUT,
  },
  {
    PARAM_RATE_MIN,
    "Increase output by at least this much voltage each millisecond (mV/ms)",
    DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
  },
  {
    PARAM_RATE_MAX,
    "Increase output by at most this much voltage each millisecond (mV/ms)",
    DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
  },
  {
    PARAM_INTERVAL_MIN,
    "Wait at least this long before starting another ramp (ms)",
    DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
  },
  {
    PARAM_INTERVAL_MAX,
    "Wait at most this long before starting another ramp (ms)",
    DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
  },
  {
    PARAM_V_MAX,
    "Cut output if it would exceed this voltage",
    DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
  },
};

static size_t num_vars = sizeof(vars)/sizeof(DefaultGUIModel::variable_t);

Ramp::Ramp(void)
  : DefaultGUIModel("Ramp",::vars,::num_vars), 
    state(START),
    rateGenerator(NULL), intervalGenerator(NULL) {
  /*
   * Initialize Parameters & Variables
   */
  Vout    = 0.0;
  Vmax    = 9.0;  setParameter(PARAM_V_MAX, Vmax);
  age     = 0.0;
  
  setParameter(PARAM_RATE_MIN, INITIAL_RATE_MIN);
  setParameter(PARAM_RATE_MAX, INITIAL_RATE_MAX);
  setRateGenerator(INITIAL_RATE_MIN, INITIAL_RATE_MAX);
  setParameter(PARAM_INTERVAL_MIN, INITIAL_INTERVAL_MIN);
  setParameter(PARAM_INTERVAL_MAX, INITIAL_INTERVAL_MAX);
  setIntervalGenerator(INITIAL_INTERVAL_MIN, INITIAL_INTERVAL_MAX);

  update(PERIOD);

  refresh();
}

Ramp::~Ramp(void) {
  delete rateGenerator, rateGenerator = NULL;
  delete intervalGenerator, intervalGenerator = NULL;
}

void Ramp::execute(void) {
  age += dt;
  Vout = 0;
  
  switch (state) {
    
    case START:
      chosenRate = (*rateGenerator)();
      chosenInterval = (*intervalGenerator)();
      age = 0;
      state = RAMP;
    break;
    
    case RAMP:
      // Convert mV/ms to V/ms.
      Vout += (chosenRate / 1000.0) * age;
      // Cut off output when the spike detector fires.
      if (input(0) >= 1) {
        state = WAITFORCELL;
        Vout = 0;
      } else if (Vout > Vmax) {
        Vout = 0;
        cutOff = age;
        state = WAIT;
      }
    break;
    
    case WAITFORCELL:
      if (input(0) < 1) {
        cutOff = age;
        state = WAIT;
      }
    break;
    
    case WAIT:
      if (age - cutOff > chosenInterval) {
        state = START;
      }
    break;
  }
  
  output(0) = Vout;
}

void Ramp::update(DefaultGUIModel::update_flags_t flag) {
  switch (flag) {
    case MODIFY:
      setRateGenerator(getParameter(PARAM_RATE_MIN).toDouble(), 
                       getParameter(PARAM_RATE_MAX).toDouble());
      setIntervalGenerator(getParameter(PARAM_INTERVAL_MIN).toDouble(), 
                           getParameter(PARAM_INTERVAL_MAX).toDouble());
      Vmax = getParameter("Vmax").toDouble();
      break;
    case PAUSE:
      output(0) = 0;
      break;
    case PERIOD:
      dt = RT::System::getInstance()->getPeriod() * 1e-6;
      break;
    default:
      break;
  }
}

void Ramp::setRateGenerator(double min, double max) {
  setGenerator(&rateGenerator, min, max);
}

void Ramp::setIntervalGenerator(double min, double max) {
  setGenerator(&intervalGenerator, min, max);
}

void Ramp::setGenerator(UniformGenerator **generator, double min, double max) {
  if (*generator != NULL) {
    delete *generator;
  }
  using namespace boost;
  *generator = new UniformGenerator(rng, uniform_real<double>(min, max));
}
