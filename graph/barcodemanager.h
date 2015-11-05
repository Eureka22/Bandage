#ifndef BARCODEMANAGER_H
#define BARCODEMANAGER_H


#include "graph/barcode.h"
#include "graph/barcodepart.h"
#include <QMap>
#include <QString>
#include <vector>
#include <QColor>
#include <QList>
#include "graph/barcodesetting.h"


class BarcodeManager
{
public:

    BarcodeManager();
    ~BarcodeManager(){
    }

    QStringList barcode_selected;

    QList<QList<QString> >barcode_contigs;
    //QStringList barcode_contigs;
    //std::vector<BarcodeSetting*> barcode_settings;
    QMap<QString, BarcodeSetting*> barcode_settings;

    QMap<QString, std::vector<Barcode* > > barocde_map;
    std::vector<QColor> presetColours;


    void add_barcode(QString barcode);

    int get_barcode_count(QString barcode);

    void createPresetColour();

};
#endif // BARCODEMANAGER_H
