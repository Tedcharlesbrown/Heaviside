"""Bake the same LUTs with PyOpenColorIO, for compare.mjs to check the WebAssembly build against.

    python test/reference.py out.json [config ...]

Configs default to the built-in studio config. For each config it bakes a few camera log
spaces through the first ACES 2.0 view of an SDR and an HDR display, when the config has them.
"""
import json
import sys

import PyOpenColorIO as OCIO

SIZE = 33
SOURCES = ['S-Log3 S-Gamut3', 'ARRI LogC4', 'Nikon N-Log', 'Apple Log']
DISPLAYS = ['Rec.1886 Rec.709 - Display', 'ST2084-P3-D65 - Display']


def grid(n):
    step = 1 / (n - 1)
    return [c for b in range(n) for g in range(n) for r in range(n) for c in (r * step, g * step, b * step)]


def main(out, configs):
    print('PyOpenColorIO', OCIO.GetVersion())
    result = []
    for path in configs:
        config = OCIO.Config.CreateFromFile(path)
        spaces = {cs.getName() for cs in config.getColorSpaces()}
        bakes = []
        for source in (s for s in SOURCES if s in spaces):
            for display in (d for d in DISPLAYS if d in config.getDisplays()):
                view = next((v for v in config.getViews(display) if v.startswith('ACES 2.0')), None)
                if not view:
                    continue
                cpu = config.getProcessor(source, display, view, OCIO.TRANSFORM_DIR_FORWARD).getDefaultCPUProcessor()
                values = cpu.applyRGB(grid(SIZE))  # returns a new list for list input
                bakes.append({'source': source, 'display': display, 'view': view, 'size': SIZE, 'values': values})
        print(f'{path}: {len(bakes)} bakes')
        result.append({'config': path, 'bakes': bakes})
    with open(out, 'w') as f:
        json.dump(result, f)


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2:] or ['ocio://studio-config-latest'])
