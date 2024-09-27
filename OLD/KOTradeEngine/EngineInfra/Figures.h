#ifndef Figures_H
#define Figures_H

#include "KOEpochTime.h"
#include <boost/date_time/posix_time/posix_time.hpp>

using std::string;

namespace KO
{
    struct FigureAction
    {
        enum options
        {
            NO_FIGURE,
            HITTING,
            HALT_TRADING,
            PATIENT,
            LIM_LIQ,
            FAST_LIQ
        };
    };

    struct Figure
    {
        string                              sFigureDay;
        KOEpochTime                         cFigureTime;
        string                              sFigureName;
    };

    struct FigureCall
    {
        string                              _sFigureName;
        KOEpochTime                         _cCallTime;
        KOEpochTime                         _cFigureTime;
        FigureAction::options               _eFigureAction;
    };

    typedef boost::shared_ptr<FigureCall> FigureCallPtr;
};

#endif /* Figures_H */
