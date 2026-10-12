# Script mode: cmake -DINPUT=... -DOUTPUT=... -DSYMBOL=... -P EmbedBinaryScript.cmake
file(READ "${INPUT}" hex HEX)
string(LENGTH "${hex}" hex_length)
math(EXPR size "${hex_length} / 2")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
string(REGEX REPLACE "(0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,)" "\\1\n" bytes "${bytes}")
file(WRITE "${OUTPUT}"
"// Generated from ${INPUT} - do not edit.\n#include <cstddef>\n"
"extern const unsigned char ${SYMBOL}[] = {\n${bytes}0x00};\n"
"extern const std::size_t ${SYMBOL}_size = ${size};\n")
