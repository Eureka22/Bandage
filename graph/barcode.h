#ifndef BARCODE_H
#define BARCODE_H
#include<QString>

#include "barcodepart.h"
#include <vector>
class Barcode
{
public:
    Barcode( QString barcode, QString contig, int position, int strand = 0):m_position(position), m_barcode(barcode), m_contig(contig), m_strand(strand) {}
    std::vector<BarcodePart> getBarcodeParts(bool reverse, double scaledNodeLength);
//private:
    int m_position;
    int m_strand;
    QString m_barcode;
    QString m_contig;

//signals:

//public slots:
};

#endif // BARCODE_H
