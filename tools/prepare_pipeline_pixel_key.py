"""Share pixel pipelines when a programmable texture stage is disabled after lowering."""


def adapt_shaders(shaders, replace_once):
    return replace_once(shaders,
        "        k.compare_mode = d[42];\n"
        "        return k;\n",
        "        k.compare_mode = d[42];\n"
        "        // The finalized mode, including missing-texture lowering, decides whether\n"
        "        // this stage samples. Stale disabled bindings must not split pipeline keys.\n"
        "        for (int i = 0; i < 4; ++i) {\n"
        "            if (((k.tex_modes >> (i * 5)) & 0x1F) == 0) {\n"
        "                k.tex[i] = {};\n"
        "                k.alphakill &= ~(1u << i);\n"
        "            }\n"
        "        }\n"
        "        return k;\n")
