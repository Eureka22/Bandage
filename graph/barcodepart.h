#ifndef BARCODEPART_H
#define BARCODEPART_H

#include <QColor>

class BarcodePart
{
public:
    BarcodePart() {}
    BarcodePart(QColor colour, double nodeFractionStart, double nodeFractionEnd) :
        m_colour(colour), m_nodeFractionStart(nodeFractionStart), m_nodeFractionEnd(nodeFractionEnd) {}

    QColor m_colour;
    double m_nodeFractionStart;
    double m_nodeFractionEnd;
};

#endif // BLASTHITPART_H
