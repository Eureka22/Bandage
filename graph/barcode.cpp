#include "barcode.h"
#include <vector>
std::vector<BarcodePart> Barcode::getBarcodeParts(bool reverse, double scaledNodeLength)
{
    std::vector<BarcodePart> returnVector;

    //If the colour scheme is Blast rainbow, then this function generates lots
    //of BlastHitParts - each small and with a different colour of the rainbow.
    /*if (g_settings->nodeColourScheme == BLAST_HITS_RAINBOW_COLOUR)
    {
        double scaledHitLength = (m_nodeEndFraction - m_nodeStartFraction) * scaledNodeLength;

        int partCount = ceil(g_settings->blastRainbowPartsPerQuery * fabs(m_queryStartFraction - m_queryEndFraction));

        //If there are way more parts than the scaled hit length, that means
        //that a single part will be much less than a pixel in length.  This
        //isn't desirable, so reduce the partCount in these cases.
        if (partCount > scaledHitLength * 2.0)
            partCount = int(scaledHitLength * 2.0);

        double nodeSpacing = (m_nodeEndFraction - m_nodeStartFraction) / partCount;
        double querySpacing = (m_queryEndFraction - m_queryStartFraction) / partCount;

        double nodeFraction = m_nodeStartFraction;
        double queryFraction = m_queryStartFraction;

        for (int i = 0; i < partCount; ++i)
        {
            QColor dotColour;
            dotColour.setHsvF(queryFraction * 0.9, 1.0, 1.0);  //times 0.9 to keep the colour from getting too close to red, as that could confuse the end with the start

            double nextFraction = nodeFraction + nodeSpacing;

            if (reverse)
                returnVector.push_back(BlastHitPart(dotColour, 1.0 - nodeFraction, 1.0 - nextFraction));
            else
                returnVector.push_back(BlastHitPart(dotColour, nodeFraction, nextFraction));

            nodeFraction = nextFraction;
            queryFraction += querySpacing;
        }
    }
    */
    //If the colour scheme is Blast solid, then this function generates only one
    //BlastHitPart with a colour dependent on the Blast query.
    //else
    double m_nodeStartFraction = 0.5;
    double m_nodeEndFraction = 0.5;

        if (reverse)
            returnVector.push_back( BarcodePart(QColor(),  1.0 - m_nodeStartFraction, 1.0 - m_nodeEndFraction));
        else
            returnVector.push_back(BarcodePart(QColor(),  m_nodeStartFraction, m_nodeEndFraction));

    return returnVector;
}
