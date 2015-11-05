#ifndef BARCODE_H
#define BARCODE_H
#include<QString>

#include "barcodepart.h"
#include <vector>
class Barcode
{
public:
    Barcode( QString barcode, QString contig, int position, int strand = 0, int nodeLength = 10000000, int readlength = 100):m_position(position), m_barcode(barcode), m_contig(contig), m_strand(strand) {

        m_nodeStartFraction = double(m_position - 1) / nodeLength;
        m_nodeEndFraction = double(m_position - 1 + readlength) / nodeLength;

    }
    std::vector<BarcodePart> getBarcodeParts(bool reverse, double scaledNodeLength);
//private:
    int m_position;
    int m_strand;
    QString m_barcode;
    QString m_contig;

    double m_nodeStartFraction;
    double m_nodeEndFraction;
    QColor m_color;

    void setColor(QColor color){m_color = color;}

//signals:

//public slots:
};

#endif // BARCODE_H
