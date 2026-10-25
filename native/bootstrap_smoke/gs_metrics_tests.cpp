#include "gs_production_metrics.h"
#include <iostream>

int main() {
    drakengard::GsProductionCounters c;
    c.ui(10); c.ui(10); c.ui(13); c.ui(12); c.ui(14);
    if (c.uiCalls != 5 || c.observedTicks != 5 || c.tickRegressions != 1) return 1;
    drakengard::GsObservedRgb frames;
    std::vector<uint8_t> a{1, 2, 3, 255, 4, 5, 6, 128};
    frames.observe(c, a, 2, 1); frames.observe(c, a, 2, 1);
    a[3] = 0; frames.observe(c, a, 2, 1);
    if (c.presentations != 3 || c.firstImages != 1 || c.rgbChanges) return 2;
    a[6] ^= 1; frames.observe(c, a, 2, 1);
    if (c.rgbChanges != 1) return 3;
    frames.observe(c, a, 1, 2);
    if (c.rgbChanges != 2) return 4;
    frames.observe(c, a, UINT32_MAX, UINT32_MAX); frames.observe(c, {}, 0, 0);
    if (c.invalidImages != 2 || c.presentations != 5) return 5;
    for (unsigned i = 0; i != 65; ++i) {
        drakengard::GsDrawKey key; key.words[0] = i; c.draw(key, i);
    }
    drakengard::GsDrawKey first; c.draw(first, 3);
    if (c.binCount != 64 || c.overflowDraws != 1 || c.overflowWallNs != 64 ||
        c.draws != 66 || c.drawWallNs != 2083 || c.bins[0].draws != 2) return 6;
    c.clearBins();
    if (c.binCount || c.overflowDraws || c.draws != 66 || c.drawWallNs != 2083) return 7;
    std::cout << "Independent UI/tick/RGB/draw counters and bounded state overflow passed\n";
}
