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

// This gives your plugin all the graphical interface magic with a lot less 
// hassle than if you did it yourself.
#include <default_gui_model.h>

// You can set your plugin name here and it'll be copied into all the boring 
// boilerplate places it's needed.
#define PLUGIN_NAME Square

// Leave all of this boilerplate in.
class PLUGIN_NAME : public DefaultGUIModel
{

public:

    PLUGIN_NAME(void);
    virtual ~PLUGIN_NAME(void);

    void execute(void);

// Leave this in too.
protected:

    void update(DefaultGUIModel::update_flags_t);

private:

    // Here's where you put the variables you want to keep track of.

    // These first three are parameters that the user can set within RTXI.
    // `Vmin` is the low voltage, `Vmax` is the high voltage, and `period` is 
    // how often to switch between the two.
    double Vmin;
    double Vmax;
    double period;

    // Here we track the passage of time. `dt` is the real-time period in 
    // milliseconds, while `age` is how long this plugin has run unpaused in its
    // lifetime.
    double dt;
    double age;

    // The last time we flipped voltage.
    double lastFlip;

    // `high` is `true` when we're emitting the high voltage value.
    bool high;

    // If you think it makes sense to put some helper functions in here, now's
    // the time.
};
