"""Validate the single-texture BC3/HWYX variant supported by this mod."""
import struct


def validate(texture, layout, expanded=False):
    height = 2048 if expanded else 1024
    count = 65 if expanded else 61
    size = 56+2048*height
    if len(texture) != size or len(layout) != 32+count*16:
        raise ValueError('Unsupported texture or layout size')
    # All header fields are fixed for this narrow variant except the derived
    # file size, height exponent and layout record count. Payload is BC3, one
    # texture, base level only, with a56-byte payload offset.
    g = (0x47315447, 0x30303630, size, 32, 1, 10, 0, 0, 4,
         0x5b10 | (0xbb if expanded else 0xab) << 16,
         0x10211000, 12, 0, 0x01000000)
    h = (0x58595748, 32, 0, 0, 16, count, 16, 4)
    if struct.unpack_from('<14I', texture) != g:
        raise ValueError('Unsupported G1T header/version/offset/mipmap variant')
    if struct.unpack_from('<8I', layout) != h:
        raise ValueError('Unsupported HWYX header/version/stride variant')
    for i in range(count):
        x,y,w,h = struct.unpack_from('<4I', layout, 32+i*16)
        if x>2048 or y>height or w>2048-x or h>height-y:
            raise ValueError(f'Layout record{i} exceeds texture bounds')
        if i>=61 and (w,h)!=(348,60):
            raise ValueError(f'New label record{i} has unsupported size')
