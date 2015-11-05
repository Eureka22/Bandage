#include "barcodesetting.h"

BarcodeSetting::BarcodeSetting(QString barcode, QColor qcolor, bool active):
    m_barcode(barcode), m_color(qcolor), m_active(active)
{};
