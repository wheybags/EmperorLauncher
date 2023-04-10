with open("patch_data.txt", "r") as f:
    lines = f.readlines()

lines = [x.strip() for x in lines[1:]]

print("PatchElement patches[] =\n{")

for line in lines:
    split = line.split()
    address = int(split[0], 16)
    length = int(split[1][2:])
    
    orig = []
    i = 2
    for j in range(length):
        orig.append(split[i])
        i = i + 1

    patched = []
    for j in range(length):
        patched.append(split[i])
        i = i + 1

    #print(hex(address), length, orig, patched)
    for i in range(length):
        print("  PatchElement{" + hex(address + i) + ", 0x" + orig[i] + ", 0x" + patched[i] + "},")

print("};")


