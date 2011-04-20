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

#include <noise.h>
#include <math.h>

extern "C" Plugin::Object *createRTXIPlugin(void) {
    return new Noise();
}

#define PARAM_HALF_AMPLITUDE "Half amplitude (V)"
#define PARAM_OFFSET "Offset (V)"
#define PARAM_OUTPUT_RATE "Output rate (Hz)"

static DefaultGUIModel::variable_t vars[] = {
    {
        "Vout",
        "Output noise",
        DefaultGUIModel::OUTPUT,
    },
    {
        PARAM_HALF_AMPLITUDE,
        "Half the amplitude of the noise (i.e. the mean)",
        DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
    },
    {
        PARAM_OFFSET,
        "Offset the entire output by this much",
        DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
    },
    {
        PARAM_OUTPUT_RATE,
        "How often to change output to a new random voltage",
        DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
    },
};

static size_t num_vars = sizeof(vars)/sizeof(DefaultGUIModel::variable_t);

Noise::Noise(void)
    : DefaultGUIModel("Noise",::vars,::num_vars) {
    /*
     * Initialize Parameters & Variables
     */
    update(PERIOD);
    
    halfAmplitude = 0.5; setParameter(PARAM_HALF_AMPLITUDE, halfAmplitude);
    offset = 0.0; setParameter(PARAM_OFFSET, offset);
    period = 1.0; setParameter(PARAM_OUTPUT_RATE, 1000.0 / period);
    lastChange = 0.0;
    age = 0.0;
    update(MODIFY);
    
    srand(time(0));

    refresh();
}

Noise::~Noise(void) {}

void Noise::execute(void) {
    age += dt_ms;
    if (age - lastChange >= period) {
        output(0) = ((double)rand() * (halfAmplitude * 2)) / 
                    (double)RAND_MAX - halfAmplitude + offset;
        lastChange = age;
    }
}

void Noise::update(DefaultGUIModel::update_flags_t flag) {
    switch (flag) {
        case MODIFY:
            halfAmplitude = getParameter(PARAM_HALF_AMPLITUDE).toDouble();
            offset = getParameter(PARAM_OFFSET).toDouble();
            period = 1000.0 / getParameter(PARAM_OUTPUT_RATE).toDouble();
            break;
        case PAUSE:
            output(0) = 0;
            break;
        case PERIOD:
            dt_ms = RT::System::getInstance()->getPeriod() * 1e-6;
            break;
        default:
            break;
    }
}
