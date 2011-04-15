// Square is an RTXI plugin that emits a square wave over its sole output. It
// has parameters for the low and high voltage, as well as the period between 
// changes.
//
// This plugin is based off of some example plugins for RTXI, hence this long 
// copyright and license notice.

/*
 * Copyright (C) 2004 Boston University
 * Copyright (C) 2010 Nolan Waite
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

// If you're copying square to make a new plugin, remember to change `square.h`
// here.
#include <square.h>

// Some boilerplate.
extern "C" Plugin::Object *createRTXIPlugin(void) {
    return new PLUGIN_NAME();
}

// These are all of the inputs, outputs, parameters, and states for your
// plugin that can be used in RTXI (by the user or by other plugins).
// Basically each thing you can use in RTXI with your plugin needs three 
// entries here.
static DefaultGUIModel::variable_t vars[] = {
    {
        // The first entry is the name of this item as shown in RTXI. For this 
        // output, "Vout" will appear in the connector and the oscilliscope.
        "Vout",
        
        // Entry two is a one-line description. It's not clear to me where this 
        // appears for inputs and outputs, but it can't hurt to put it in.
        "Output of this square wave",
        
        // Finally, indicate whether this is an input, output, parameter, or 
        // state.
        DefaultGUIModel::OUTPUT,
    },
    {
        // For parameters, the name is also used to refer to this parameter in 
        // the getParameter and setParameter functions. You'll be typing it in 
        // a few more times, but that shouldn't stop you from being descriptive. 
        "Vmin",
        
        // For parameters, this one-line description appears in a tooltip when 
        // the mouse pointer rests on this field in RTXI.
        "Minimum output of this square wave",
        
        // When indicating a parameter, you must also indicate what type of 
        // number it is. Other choices are `INTEGER` and `UINTEGER`.
        DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
    },
    {
        "Vmax",
        "Maximum output of this square wave",
        DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
    },
    {
        // I like to put units right in the parameter's name. It makes the name 
        // a bit longer but making the units obvious is worth it.
        "Period (ms)",
        "How often to switch between 0 and Vmax (ms)",
        DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
    },
};

// Boilerplate.
static size_t num_vars = sizeof(vars)/sizeof(DefaultGUIModel::variable_t);

PLUGIN_NAME::PLUGIN_NAME(void)
    : DefaultGUIModel("Square",::vars,::num_vars) {
    
    // Here is where you initialize the variables you set up in the .h file, 
    // all of the ones in the `private:` section.
    // For each variable that is also a parameter one can set within RTXI, 
    // set the initial value in RTXI. Note that `setParameter` requires 
    // the parameter's name in RTXI as the first argument, the name you set 
    // above.
    Vmin     =  0.0;  setParameter("Vmin", Vmin);
    Vmax     =  1.0;  setParameter("Vmax", Vmax);
    period   = 50.0;  setParameter("Period (ms)", period);
    age      =  0.0;
    lastFlip =  0.0;
    high     = false;
    
    // Normally `update` is invoked by RTXI when an important event happens. I 
    // call it here to initialize the `dt` variable.
    update(PERIOD);

    // `refresh` updates the user interface. This is the only time you need to
    // call it.
    refresh();
}

// Boilerplate.
PLUGIN_NAME::~PLUGIN_NAME(void) {}

// Where the magic happens. Replace the code in this function with whatever you 
// want your plugin to do once per real-time period. Make sure to keep this 
// code tight, as it can run very frequently. The RTXI guys recommend you do 
// not instantiate variables here; use instance variables or static variables.
void PLUGIN_NAME::execute(void) {
    
    // We're one period older. Flip the voltage if we've been at the current
    // voltage for long enough.
    age += dt;
    if (age - lastFlip >= period) {
        high = !high;
        lastFlip += period;
    }
    
    // Sets this plugin's first output to the high or low voltage as indicated 
    // by `high`. Plugins connected to this one in RTXI will immediately see 
    // and use this new value.
    // Set this every step, even if it appears unchanged, in case we've just
    // been unpaused.
    output(0) = high ? Vmax : Vmin;
}

// RTXI tells your plugin about certain interesting happenings.
void PLUGIN_NAME::update(DefaultGUIModel::update_flags_t flag) {
    switch (flag) {
        
        // When an instance of this plugin (and there can be more than one) is 
        // loaded by the Plugin Loader (in the Control menu) in RTXI, the `INIT`
        // flag is sent. For square, we don't need to do any further 
        // initialization. 
        case INIT:
            break;
        
        // When someone presses the Modify button on your plugin's window in 
        // RTXI, the `MODIFY` flag comes down.
        case MODIFY:
            
            // Make sure to get all parameters that someone could have modified.
            // Since we're not sure which were modified, grab them all. Note 
            // that you must use the name declared way at the top of this file 
            // to access the parameters.
            double newVmin, newVmax;
            newVmin = getParameter("Vmin").toDouble();
            newVmax = getParameter("Vmax").toDouble();
            period  = getParameter("Period (ms)").toDouble();
            
            // Validate the setting for period given by the user. If the desired 
            // period is less than the current real-time period for RTXI, odd 
            // things will happen, so we set the real-time period as a minimum.
            // Also update the graphical interface so the user knows what we 
            // did.
            if (period < dt) {
                period = dt;
                setParameter("Period (ms)", period);
            }
            
            // Validate the new high and low voltages. Reverse the attempted 
            // change if the maximum is less than the minimum, because that 
            // makes no sense.
            if (newVmax < newVmin) {
                setParameter("Vmin", Vmin);
                setParameter("Vmax", Vmax);
            } else {
                Vmin = newVmin;
                Vmin = newVmax;
            }
            
            break;
        
        // When someone presses the Pause button on your plugin's window in 
        // RTXI. `execute` will not get called until someone unpauses the 
        // plugin. For square, we just turn the output off while paused.
        case PAUSE:
            output(0) = 0;
            break;
        
        // If the real-time period changes, this is how your plugin finds out.
        // We'll save the new value, converted to milliseconds.
        case PERIOD:
            dt = RT::System::getInstance()->getPeriod() * 1e-6;
            break;
        
        default:
            break;
    }
}
