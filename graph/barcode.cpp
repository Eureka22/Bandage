#include "barcode.h"
#include <vector>
std::vector<BarcodePart> Barcode::getBarcodeParts(bool reverse, double scaledNodeLength)
{
    std::vector<BarcodePart> returnVector;

    //If the colour scheme is Blast solid, then this function generates only one
    //BlastHitPart with a colour dependent on the Blast query.

    if (reverse)
        returnVector.push_back(BarcodePart(m_color ,  1.0 - m_nodeStartFraction, 1.0 - m_nodeEndFraction));
    else
        returnVector.push_back(BarcodePart(m_color,  m_nodeStartFraction, m_nodeEndFraction));

    return returnVector;
}
