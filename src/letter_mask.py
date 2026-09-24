"""
Bit layout:
    bits 0-25 : 'A'-'Z'
    bit  26   : '^'
"""

_BIT_FOR_CHAR = {chr(ord('A') + i): 1 << i for i in range(26)}
_BIT_FOR_CHAR['^'] = 1 << 26

def bit_for(ch: str):
    return _BIT_FOR_CHAR[ch]

def add_char(mask: int, ch: str):
    return mask | _BIT_FOR_CHAR[ch]

def has_char(mask: int, ch: str):
    return bool(mask & _BIT_FOR_CHAR[ch])

def to_char_set(mask: int):
    out = {c for c, b in _BIT_FOR_CHAR.items() if mask & b}
    return out

def format_mask(mask: int):
    if mask is None:
        return "None"
    return str(to_char_set(mask))