data = open("tramp.bin", "rb").read()
with open("lib/tramp_blob.h", "w") as f:
    f.write("#ifndef TRAMP_BLOB_H\n#define TRAMP_BLOB_H\n\n")
    f.write("static const unsigned char tramp_bin[] = {\n")
    for i in range(0, len(data), 16):
        f.write("  " + ",".join(str(b) for b in data[i:i+16]) + ",\n")
    f.write("};\n")
    f.write(f"static const unsigned int tramp_bin_len = {len(data)};\n\n")
    f.write("#endif\n")
