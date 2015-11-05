#ifndef BARCODESETTING_H
#define BARCODESETTING_H

#include <QString>
#include <QColor>
#include <QObject>
#include <QDebug>

class BarcodeSetting: public QObject
{
    Q_OBJECT

public:
    BarcodeSetting(){};
    BarcodeSetting(QString barcode, QColor color, bool active);
    QString m_barcode;
    QColor m_color;
    bool m_active;

signals:

public slots:
    void setColour(QColor newColour) {m_color = newColour;
      //qDebug()<<"set color";
                                     }
    void setShown(bool newShown) {m_active = newShown;
                                 //qDebug()<<"toggle"<<m_active;
                                m_color.setAlpha(255*int(m_active));
                                 }
};

#endif // BARCODESETTING_H
