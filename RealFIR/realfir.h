/*
 * RealFIR
 * Low/high/band-pass FIR filter, designed from a Hamming window.
 * Copyright 2011 Nolan Waite
 */


#include <default_gui_model.h>
#include <vector>
#include <boost/circular_buffer.hpp>


class RealFIR : public DefaultGUIModel
{

public:

    RealFIR(void);
    virtual ~RealFIR(void);

    void execute(void);

protected:

    void update(DefaultGUIModel::update_flags_t);

private:

    std::vector<double> coefficients;
    std::pair<double, double> passband;
    boost::circular_buffer<double> buffer;
    double samplingRate;
    double lastSample;
    double dt_s;
    double elapsedTime;
    double accum;
    size_t i;

};
