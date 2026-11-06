"""Converts FH3's ANNA textures to FH2's .swatchbin, into assets/ (embedded by src/resources.rc).

    python tools/fh3_textures.py "D:\\Games\\FH2\\Opus_1.0.125.0_x64__8wekyb3d8bbwe"

FH3 and FH2 store these textures the same way (the HCXT block, its format, the pixels); only the
outer header differs: FH3's ("burG" 1.1) has one more u32 after the file's size, so its BCXT
block, and everything after it, is 4 bytes later. FH2's is "burG" 1.0.1, 0x53 bytes of header.
"""
import os
import struct
import sys
import zipfile

NAMES = ['HUD/' + n + '.swatchbin' for n in (
    'ANNA_Glow_On', 'ANNA_Glow_ANNAOnly', 'ANNA_Logo', 'VerticalGradient', 'Minimap_OpacityMask',
    'Minimap_OpacityMask_WithDestination', 'Minimap_OpacityMask_MapOnly', 'Minimap_OpacityMask_MapOnly_WithDestination')]


def fh3_to_fh2(d):
    if d[:4] != b'burG' or d[4:8] != b'\x01\x01\x00\x00':
        raise ValueError('not an FH3 swatchbin')
    header = struct.unpack_from('<I', d, 8)[0]
    if header != 0x57 or d[0x14:0x18] != b'BCXT' or d[0x2C:0x30] != b'HCXT':
        raise ValueError('unexpected FH3 header')
    texture = d[0x2C:0x57]   # the HCXT block (0x2B bytes), as FH2 has it
    pixels = d[0x57:]
    size = len(pixels)
    out = bytearray()
    out += b'burG\x01\x00\x01\x00'
    out += struct.pack('<II', 0x53, 0x53 + size)
    out += b'BCXT\x02\x00\x01\x00'
    out += struct.pack('<IIII', 0x28, 0x53, size, size)
    out += texture
    assert len(out) == 0x53
    return bytes(out + pixels)


def main():
    fh3 = sys.argv[1] if len(sys.argv) > 1 else r'D:\Games\FH2\Opus_1.0.125.0_x64__8wekyb3d8bbwe'
    zf = zipfile.ZipFile(os.path.join(fh3, 'Mount', 'media', 'ui', 'textures', 'Anthem.zip'))
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.makedirs(os.path.join(here, 'assets'), exist_ok=True)
    for name in NAMES:
        data = fh3_to_fh2(zf.read(name))
        path = os.path.join(here, 'assets', os.path.basename(name))
        with open(path, 'wb') as f:
            f.write(data)
        print(path, len(data))


if __name__ == '__main__':
    main()
