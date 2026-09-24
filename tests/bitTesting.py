
import cProfile

def main():
    mask = 0
    print(f"Initial mask: {mask:b}")
    mask = addCharToSet(mask, 'a')
    mask = addCharToSet(mask, 'b')
    mask = addCharToSet(mask, 'z')
    mask = addCharToSet(mask, 'm')
    mask = addCharToSet(mask, 'j')
    mask = addCharToSet(mask, 'a')
    print(f"Final mask: {mask:b}")

    print(f"Characters in mask: {getCharSetFromMask(mask)}")

    print(f"Is 'a' in mask? {charInSet(mask, 'a')}")
    print(f"Is 'b' in mask? {charInSet(mask, 'b')}")
    print(f"Is 'c' in mask? {charInSet(mask, 'c')}")

def addCharToSet(mask, char):
    index = ord(char) - ord('a')
    return mask | (1 << index)

def getCharSetFromMask(mask):
    char_set = set()
    for i in range(26):
        if mask & (1 << i):
            char_set.add(chr(i + ord('a')))
    return char_set

def charInSet(mask, char):
    index = ord(char) - ord('a')
    return (mask & (1 << index)) != 0

if __name__ == "__main__":
    cProfile.run('main()', sort = 'cumtime')