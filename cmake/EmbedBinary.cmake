# gta20_embed_binary(<target> <symbol> <input-file> [GENERATED])
# Compiles <input-file> into the target as:
#   extern const unsigned char <symbol>[]; extern const std::size_t <symbol>_size;
set(GTA20_EMBED_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/EmbedBinaryScript.cmake)

function(gta20_embed_binary target symbol input)
    set(output ${CMAKE_CURRENT_BINARY_DIR}/embedded/${symbol}.cpp)
    add_custom_command(
        OUTPUT ${output}
        COMMAND ${CMAKE_COMMAND} -DINPUT=${input} -DOUTPUT=${output} -DSYMBOL=${symbol} -P ${GTA20_EMBED_SCRIPT}
        DEPENDS ${input} ${GTA20_EMBED_SCRIPT}
        COMMENT "Embedding ${symbol}"
        VERBATIM)
    target_sources(${target} PRIVATE ${output})
endfunction()
