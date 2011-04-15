/*
 * Mux
 * Combine voltage from multiple inputs.
 * Copyright 2011 Nolan Waite
 */

/*
 * This plugin aggregates voltage from all of its inputs. You can also change 
 * a scaling factor and/or an offset, which are applied before output.
 */

#include <default_gui_model.h>


// Output the sum of all input voltages, optionally amplified and offset.
class Mux : public DefaultGUIModel
{

public:

    Mux(void);
    virtual ~Mux(void);

    void execute(void);

protected:

    void update(DefaultGUIModel::update_flags_t);

private:

    double Vmax;
    double factor;
    double offset;
    double Vout;

};
