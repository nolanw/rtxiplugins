/*
 * Ramp
 * Apply a semirandomly increasing voltage until a spike.
 * Copyright 2011 Nolan Waite
 */

/*
 * This plugin is meant to be connected to an instance of the SpikeDetect
 * plugin. It will choose a random ramp-up rate between the minimum and maximum 
 * the user specifies, then increase voltage from Vstart at this rate until a 
 * spike happens (or we would output at least Vmax). After the cell depolarizes, 
 * wait a random amount of time between the minimum and maximum the user 
 * specifies until starting all over again.
 */

#include <default_gui_model.h>
#include <boost/random.hpp>

using namespace boost;
typedef variate_generator<mt19937&, uniform_real<double> > UniformGenerator;

class Ramp : public DefaultGUIModel
{

public:

    Ramp(void);
    virtual ~Ramp(void);

    void execute(void);

protected:

    void update(DefaultGUIModel::update_flags_t);

private:

    // milliseconds per realtime period
    double dt;
    // scratch for calculating output
    double Vout;
    // stop this ramp when this threshold is hit on input
    double Vthresh;
    // ramps increase at a random rate
    double chosenRate;
    // ramps start after a random interval
    double chosenInterval;
    // trigger a threshold at this output regardless of input
    double Vmax;
    // track how long we've been running
    double age;
    // when we cut the output (threshold or max was hit)
    double cutOff;
    
    // State machine for plugin.
    enum State {
      // Pick an interval and rate and start ramping.
      START,
      // Continue ramping unless we've hit the voltage threshold.
      RAMP,
      // The cell has crossed the threshold going up; wait for it to depolarize.
      WAITFORCELL,
      // Wait until the chosenRate has elapsed, then start a new ramp.
      WAIT,
    };
    State state;
    
    // Random number generator. A new one is created whenever paramters change.
    mt19937 rng;
    UniformGenerator *rateGenerator;
    void setRateGenerator(double min, double max);
    UniformGenerator *intervalGenerator;
    void setIntervalGenerator(double min, double max);
    void setGenerator(UniformGenerator **generator, double min, double max);

};
