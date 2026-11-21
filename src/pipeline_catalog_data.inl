// Generated render-state hints. Original shader programs come from the installed game.
// Regenerate with tools/generate_pipeline_catalog.py.

static_assert(sizeof(gpu::PixelKey) == 344);

#ifdef _WIN32
constexpr uint64_t kD3D12CatalogLayout = 0xd3d12000000000c0ull;
constexpr CatalogEntry kD3D12Catalog[] = {
    {
        .rooms = {0x40, 0, 0, 0xb, 0x85, 8, 0, 1},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0x63baf2651d1df27full, .ps_source = 0x0676027f80e8ff24ull,
        .vertex = {.program = 0xfd446e65656abb89, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x1d1df27f, 0x63baf265, 0x80e8ff24, 0x676027f, 0x78c, 2, 1, 1, 0, 0,
            0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x10, 0, 0x200},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x62c593a3ab37d57full, .ps_source = 0x0fc0247125952abfull,
        .vertex = {.program = 0x697df3438a77f65f, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xab37d57f, 0x62c593a3, 0x25952abf, 0xfc02471, 0xf8c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x431a117f, 0x7200, 0x1af, 0x41ff, 6, 0x2fd1e99, 0xe9, 1},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb0921fcf5c4bf28dull, .ps_source = 0x8eddd92961468648ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x5c4bf28d, 0xb0921fcf, 0x61468648, 0x8eddd929, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 0, 0x203, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0xa8130, 0x10200, 0x49f, 0x30, 0x8110, 0x108000, 3},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x3e578683210c9781ull, .ps_source = 0x0fc0247125952abfull,
        .vertex = {.program = 0x5f10fffdb91e8bfd, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x210c9781, 0x3e578683, 0x25952abf, 0xfc02471, 0xf8c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0x508, 0, 0, 0, 1},
        .pixel_program = 0x5d2b1cfd399ca389ull,
        .vs_source = 0x4de1b7ed3d920e0full, .ps_source = 0x49ce1b8ae8ae3b8cull,
        .vertex = {.program = 0x2df7f99d6e7ba7, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x401, .tex = {{.kind = 1}, {}, {.kind = 1}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x3d920e0f, 0x4de1b7ed, 0xe8ae3b8c, 0x49ce1b8a, 0x78c, 2, 1, 1, 0, 0,
            0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0x80},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xd805f68f6727788full, .ps_source = 0xa9dd569ea9c5c309ull,
        .vertex = {.program = 0xbcd19491676d91cb, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}, .alphakill = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x6727788f, 0xd805f68f, 0xa9c5c309, 0xa9dd569e, 0x78c, 2, 1, 1, 0, 0,
            0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x431a90a2, 0x10200, 0x41a, 0x8c4, 0x100, 3, 0, 0xa},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0x9d818be217c867c7ull, .ps_source = 0x1bc359fadc88c8ebull,
        .vertex = {.program = 0xef9a4b7428e4ed33, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x17c867c7, 0x9d818be2, 0xdc88c8eb, 0x1bc359fa, 0x38c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0x80},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x9ae6aac62fcf97c1ull, .ps_source = 0xa9dd569ea9c5c309ull,
        .vertex = {.program = 0x4f9eb762132ae1f1, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}, .alphakill = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x2fcf97c1, 0x9ae6aac6, 0xa9c5c309, 0xa9dd569e, 0x38c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0xb, 0x85, 8, 0, 1},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0x5d2bd4c755d1c746ull, .ps_source = 0x0676027f80e8ff24ull,
        .vertex = {.program = 0x94e5de1ac8188969, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x55d1c746, 0x5d2bd4c7, 0x80e8ff24, 0x676027f, 0xf8c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x80100},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0xac56f5ca68812b03ull, .ps_source = 0x1bc359fadc88c8ebull,
        .vertex = {.program = 0xad3f9bf87748856d, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x68812b03, 0xac56f5ca, 0xdc88c8eb, 0x1bc359fa, 0x38c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x80100},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0x3d4f6844ef1cc87dull, .ps_source = 0x1bc359fadc88c8ebull,
        .vertex = {.program = 0x8ab3fe497032abb9, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xef1cc87d, 0x3d4f6844, 0xdc88c8eb, 0x1bc359fa, 0xf8c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0, 8},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x9ae6aac62fcf97c1ull, .ps_source = 0x1d173e9659f54b1eull,
        .vertex = {.program = 0x4f9eb762132ae1f1, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x2fcf97c1, 0x9ae6aac6, 0x59f54b1e, 0x1d173e96, 0x38c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0x200, 0, 0, 0, 0x44},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0x9bdb593d872c4244ull, .ps_source = 0x1bc359fadc88c8ebull,
        .vertex = {.program = 0x3719ba15bc9c76c7, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x872c4244, 0x9bdb593d, 0xdc88c8eb, 0x1bc359fa, 0x38c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x30a91f2, 0x18200, 0x61a, 0xdd4, 0x100, 0xc0000267, 0x19, 0xb},
        .pixel_program = 0xc1ad79eb34696ff9ull,
        .vs_source = 0xd876198aa691bf32ull, .ps_source = 0x36456f9db9076dfbull,
        .vertex = {.program = 0x47aaf22d90dc2f39, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 0x11044, .tex = {{.kind = 1}, {.kind = 1, .rect = 1, .shadow = 1, .flags = 8}, {}, {.kind = 1, .rect = 1, .shadow = 1, .flags = 8}}, .alphatest = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xa691bf32, 0xd876198a, 0xb9076dfb, 0x36456f9d, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0, 0x301, 0x8006, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x202908c, 0x10002, 0x100, 0x580, 0x10, 0x80080, 0xd, 0x75c},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb0921fcf5c4bf28dull, .ps_source = 0x8eddd92961468648ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x5c4bf28d, 0xb0921fcf, 0x61468648, 0x8eddd929, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 4},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0xb0c65e8bbd09767eull, .ps_source = 0x1bc359fadc88c8ebull,
        .vertex = {.program = 0xebe0a2b6413609b9, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xbd09767e, 0xb0c65e8b, 0xdc88c8eb, 0x1bc359fa, 0x78c, 2, 1, 1, 0, 0,
            0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x431a91bf, 0x1f202, 0x7bf, 0x48f4, 0x9c110, 0x9ff6ff77, 0xff, 0xfee},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x049b3121cc1fd149ull, .ps_source = 0x0fc0247125952abfull,
        .vertex = {.program = 0xb3b57c5b9a7450af, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xcc1fd149, 0x49b3121, 0x25952abf, 0xfc02471, 0xd, 2, 1, 1, 2, 0,
            0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x431a91bf, 0x1f202, 0x7bf, 0x48f4, 0x9c110, 0x9ff6fff7, 0xff, 0xfee},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x049b3121cc1fd149ull, .ps_source = 0x0fc0247125952abfull,
        .vertex = {.program = 0xb3b57c5b9a7450af, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x28, 0xcc1fd149, 0x49b3121, 0x25952abf, 0xfc02471, 0xd, 2, 1, 1, 2, 0,
            0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0x40e00000, 0x57, 0
        },
    },
    {
        .rooms = {0x100},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0x9bdb593d872c4244ull, .ps_source = 0x1bc359fadc88c8ebull,
        .vertex = {.program = 0x3719ba15bc9c76c7, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x872c4244, 0x9bdb593d, 0xdc88c8eb, 0x1bc359fa, 0x38c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x431a91ff, 0x1f202, 0x7bf, 0x5dff, 0x9c397, 0xdfffffff, 0xff, 0xfff},
        .pixel_program = 0x08dcef845cc8aa5dull,
        .vs_source = 0x2b32c1b9db7207a5ull, .ps_source = 0xf1e67a050fb749d2ull,
        .vertex = {.program = 0x6b2a205984c073cd, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 0x1041, .tex = {{.kind = 1}, {.kind = 1, .rect = 1, .shadow = 1, .flags = 8}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xdb7207a5, 0x2b32c1b9, 0xfb749d2, 0xf1e67a05, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x8000},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x7f8aa52a5d21f5bdull, .ps_source = 0x0fc0247125952abfull,
        .vertex = {.program = 0x22ca82cb24abc905, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x5d21f5bd, 0x7f8aa52a, 0x25952abf, 0xfc02471, 0xf8c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x180112, 0, 0x200, 0, 0, 0x200, 4},
        .pixel_program = 0x14c4769017b92a89ull,
        .vs_source = 0xbb3a6278c2e7ac01ull, .ps_source = 0x2db08b5d0a67f2f5ull,
        .vertex = {.program = 0x8d4f80ddb4f99031, .regs = 0x200},
        .pixel = {.programmable = 1},
        .key_size = 192,
        .key = {
            0, 0x28, 0xc2e7ac01, 0xbb3a6278, 0xa67f2f5, 0x2db08b5d, 0x200, 2, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0x40e00000, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x800},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xe1ee4965a28a8ab3ull, .ps_source = 0x8eddd92961468648ull,
        .vertex = {.program = 0x7ac2d148e0075b3b, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xa28a8ab3, 0xe1ee4965, 0x61468648, 0x8eddd929, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0x500, 0, 0, 0, 1},
        .pixel_program = 0x5d2b1cfd399ca389ull,
        .vs_source = 0x0b6e9d475e06ade4ull, .ps_source = 0x49ce1b8ae8ae3b8cull,
        .vertex = {.program = 0x6b238c678ed7f83, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x401, .tex = {{.kind = 1}, {}, {.kind = 1}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x5e06ade4, 0xb6e9d47, 0xe8ae3b8c, 0x49ce1b8a, 0x38c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0x200},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0xc5bb1a1e0c934f69ull, .ps_source = 0xac5a867766049fbdull,
        .vertex = {.program = 0x1b707112de27a197, .decode = 0x4000, .regs = 0xb},
        .pixel = {.ff = {{5, 2, 0, 1, 5, 2, 0, 1, 1}, {2, 1, 1, 1, 1, 2, 1, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1, .proj = 7, .alphatest = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xc934f69, 0xc5bb1a1e, 0x66049fbd, 0xac5a8677, 0xb, 2, 1, 1, 6, 0xc0057,
            0, 0x10002a, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 0, 0x203, 1, 0x205,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e00, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x420a01ae, 0x1f202, 0x680, 0x5da5, 0x40297, 0xc9fedc5b, 0xff, 0x5db},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb0921fcf5c4bf28dull, .ps_source = 0xe1f1cd2f1a51cea2ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0, 0x5c4bf28d, 0xb0921fcf, 0x1a51cea2, 0xe1f1cd2f, 0xb, 2, 1, 0, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x101010, 0, 0, 0, 0, 0x100, 0x60},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb0921fcf5c4bf28dull, .ps_source = 0x8eddd92961468648ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x5c4bf28d, 0xb0921fcf, 0x61468648, 0x8eddd929, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 1, 0x800b, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0x1501, 0x40203, 0x40090008, 0, 0x11},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0xb5cbb2dfa8de3c07ull, .ps_source = 0x0676027f80e8ff24ull,
        .vertex = {.program = 0x512c33a8e938e3fb, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xa8de3c07, 0xb5cbb2df, 0x80e8ff24, 0x676027f, 0x78c, 2, 1, 1, 0, 0,
            0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0, 8},
        .pixel_program = 0x371532d23dfd51e5ull,
        .vs_source = 0xa5a62315d8939534ull, .ps_source = 0x25fff679e416cbe4ull,
        .vertex = {.program = 0x3c917fd1f5c3b869, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}, .alphakill = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xd8939534, 0xa5a62315, 0xe416cbe4, 0x25fff679, 0xd, 2, 1, 1, 2, 0,
            0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 8},
        .pixel_program = 0x5d2b1cfd399ca389ull,
        .vs_source = 0x574b64685e719d48ull, .ps_source = 0x49ce1b8ae8ae3b8cull,
        .vertex = {.program = 0xa9bcb3a1ab8eb269, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x401, .tex = {{.kind = 1}, {}, {.kind = 1}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x5e719d48, 0x574b6468, 0xe8ae3b8c, 0x49ce1b8a, 0x38c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x1010, 0x8000, 0x208, 0x800, 0x10000, 0x88000222, 0x3c},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x9ae6aac62fcf97c1ull, .ps_source = 0x0fc0247125952abfull,
        .vertex = {.program = 0x4f9eb762132ae1f1, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x2fcf97c1, 0x9ae6aac6, 0x25952abf, 0xfc02471, 0x38c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x800, 0x10001, 1},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xc0ccf3b6eb1ae8d4ull, .ps_source = 0xe1f1cd2f1a51cea2ull,
        .vertex = {.program = 0x9a7598448c4d1233, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xeb1ae8d4, 0xc0ccf3b6, 0x1a51cea2, 0xe1f1cd2f, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x180012, 0, 0, 0, 0, 0x200, 4},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0x9ff2682ac4c41431ull, .ps_source = 0x1bc359fadc88c8ebull,
        .vertex = {.program = 0xb7c239bc7fa988fb, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xc4c41431, 0x9ff2682a, 0xdc88c8eb, 0x1bc359fa, 0xf8c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x21a11a4, 0x200, 0x12, 0x880, 0, 0x46, 0, 0xa},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x049b3121cc1fd149ull, .ps_source = 0x1d173e9659f54b1eull,
        .vertex = {.program = 0xb3b57c5b9a7450af, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xcc1fd149, 0x49b3121, 0x59f54b1e, 0x1d173e96, 0xd, 2, 1, 1, 2, 0,
            0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 4},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0xac56f5ca68812b03ull, .ps_source = 0x1bc359fadc88c8ebull,
        .vertex = {.program = 0xad3f9bf87748856d, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x68812b03, 0xac56f5ca, 0xdc88c8eb, 0x1bc359fa, 0x38c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0x11000, 0, 0, 0, 0x400},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb0921fcf5c4bf28dull, .ps_source = 0xe1f1cd2f1a51cea2ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0, 0x5c4bf28d, 0xb0921fcf, 0x1a51cea2, 0xe1f1cd2f, 0xb, 2, 1, 0, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x100},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0x85058ddf5b6375e6ull, .ps_source = 0x1bc359fadc88c8ebull,
        .vertex = {.program = 0x4390a90fe19250ff, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x5b6375e6, 0x85058ddf, 0xdc88c8eb, 0x1bc359fa, 0xf8c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0x80},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x049b3121cc1fd149ull, .ps_source = 0xa9dd569ea9c5c309ull,
        .vertex = {.program = 0xb3b57c5b9a7450af, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}, .alphakill = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xcc1fd149, 0x49b3121, 0xa9c5c309, 0xa9dd569e, 0xd, 2, 1, 1, 2, 0,
            0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x431a91ff, 0x1f202, 0x7bf, 0x5dff, 0xdc397, 0xdfffffff, 0xfe, 0xfff},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb0921fcf5c4bf28dull, .ps_source = 0x8eddd92961468648ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x5c4bf28d, 0xb0921fcf, 0x61468648, 0x8eddd929, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 4},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xc3b79726226eb8c1ull, .ps_source = 0x0fc0247125952abfull,
        .vertex = {.program = 0xccd3b9a74a22c2cb, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x226eb8c1, 0xc3b79726, 0x25952abf, 0xfc02471, 0xd, 2, 1, 1, 2, 0,
            0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x8000},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0x511179701f64e1bfull, .ps_source = 0xfe0dd072298ac2b2ull,
        .vertex = {.program = 0x1b707112de27a197, .regs = 0xb},
        .pixel = {.ff = {{4, 0, 2, 1, 4, 0, 2, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .proj = 3},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x1f64e1bf, 0x51117970, 0x298ac2b2, 0xfe0dd072, 0xb, 2, 1, 1, 6, 0xc0057,
            0, 0x100010, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 1, 0x8006, 0xf, 1, 0, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0x44},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0x304b3c8e2c5549d1ull, .ps_source = 0x1bc359fadc88c8ebull,
        .vertex = {.program = 0xfa08fa55ec14ec6b, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x2c5549d1, 0x304b3c8e, 0xdc88c8eb, 0x1bc359fa, 0x78c, 2, 1, 1, 0, 0,
            0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x431a90a2, 0x10000, 0x41a, 0x8c4, 0x100, 3, 0, 0xa},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0x9ff2682ac4c41431ull, .ps_source = 0x1bc359fadc88c8ebull,
        .vertex = {.program = 0xb7c239bc7fa988fb, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xc4c41431, 0x9ff2682a, 0xdc88c8eb, 0x1bc359fa, 0xf8c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x10, 0, 0x200},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x9ae6aac62fcf97c1ull, .ps_source = 0x0fc0247125952abfull,
        .vertex = {.program = 0x4f9eb762132ae1f1, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x2fcf97c1, 0x9ae6aac6, 0x25952abf, 0xfc02471, 0x38c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x431a91ff, 0x1f202, 0x7bf, 0x5dff, 0xdc397, 0xdfffffff, 0xff, 0xfff},
        .pixel_program = 0x14c4769017b92a89ull,
        .vs_source = 0xbb3a6278c2e7ac01ull, .ps_source = 0x2db08b5d0a67f2f5ull,
        .vertex = {.program = 0x8d4f80ddb4f99031, .regs = 0x200},
        .pixel = {.programmable = 1},
        .key_size = 192,
        .key = {
            0, 0x28, 0xc2e7ac01, 0xbb3a6278, 0xa67f2f5, 0x2db08b5d, 0x200, 2, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0x40e00000, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x100, 0, 0x40000008},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0x725af490e91a8de6ull, .ps_source = 0x0676027f80e8ff24ull,
        .vertex = {.program = 0xa0cbcf00aa8ae0c3, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xe91a8de6, 0x725af490, 0x80e8ff24, 0x676027f, 0x38c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x421a90be, 0x1f202, 0x29a, 0x48d4, 0x94100, 0x8fe6fe33, 0xfe, 0xfee},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x9ae6aac62fcf97c1ull, .ps_source = 0x0fc0247125952abfull,
        .vertex = {.program = 0x4f9eb762132ae1f1, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x2fcf97c1, 0x9ae6aac6, 0x25952abf, 0xfc02471, 0x38c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x431a91ff, 0x1f202, 0x7bf, 0x5dff, 0xdc397, 0xdfffffff, 0xff, 0xfff},
        .pixel_program = 0x14c4769017b92a89ull,
        .vs_source = 0x60d8ee3bb19de556ull, .ps_source = 0x2db08b5d0a67f2f5ull,
        .vertex = {.program = 0xa7f404fc37664173, .regs = 0xf00},
        .pixel = {.programmable = 1},
        .key_size = 192,
        .key = {
            0, 0x28, 0xb19de556, 0x60d8ee3b, 0xa67f2f5, 0x2db08b5d, 0xf00, 2, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 2, 0x100002, 0x200002, 0x300002, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0x40e00000, 0x57, 0
        },
    },
    {
        .rooms = {0x10a0, 0, 0x1b, 0xc6, 0x300, 0x4004, 0, 0xf},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0xc5bb1a1e0c934f69ull, .ps_source = 0xac5a867766049fbdull,
        .vertex = {.program = 0x1b707112de27a197, .decode = 0x4000, .regs = 0xb},
        .pixel = {.ff = {{5, 2, 0, 1, 5, 2, 0, 1, 1}, {2, 1, 1, 1, 1, 2, 1, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1, .proj = 7, .alphatest = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xc934f69, 0xc5bb1a1e, 0x66049fbd, 0xac5a8677, 0xb, 2, 1, 1, 6, 0xc0057,
            0, 0x10002a, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 1, 0x800b, 0xf, 1, 0, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0x1501, 0x40203, 0x40090008, 0, 0x11},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0x725af490e91a8de6ull, .ps_source = 0x0676027f80e8ff24ull,
        .vertex = {.program = 0xa0cbcf00aa8ae0c3, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xe91a8de6, 0x725af490, 0x80e8ff24, 0x676027f, 0x38c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x40100000},
        .pixel_program = 0x08dcef845cc8aa5dull,
        .vs_source = 0x2b32c1b9db7207a5ull, .ps_source = 0xf1e67a050fb749d2ull,
        .vertex = {.program = 0x6b2a205984c073cd, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 0x1041, .tex = {{.kind = 1}, {.kind = 1, .rect = 1, .shadow = 1, .flags = 8}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xdb7207a5, 0x2b32c1b9, 0xfb749d2, 0xf1e67a05, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 0, 0x203, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0, 0x80},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0x46e7ff63949c616full, .ps_source = 0x0676027f80e8ff24ull,
        .vertex = {.program = 0x1cb8524e559ff831, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x949c616f, 0x46e7ff63, 0x80e8ff24, 0x676027f, 0x78c, 2, 1, 1, 0, 0,
            0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x80100},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0xb0c65e8bbd09767eull, .ps_source = 0x1bc359fadc88c8ebull,
        .vertex = {.program = 0xebe0a2b6413609b9, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xbd09767e, 0xb0c65e8b, 0xdc88c8eb, 0x1bc359fa, 0x78c, 2, 1, 1, 0, 0,
            0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0x10000},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x114d7c7f584531d6ull, .ps_source = 0x0fc0247125952abfull,
        .vertex = {.program = 0xcc0a3244080130e1, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x584531d6, 0x114d7c7f, 0x25952abf, 0xfc02471, 0x38c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0x10000},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xbd014a9fe5670e38ull, .ps_source = 0x0fc0247125952abfull,
        .vertex = {.program = 0x6b228efce02817df, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xe5670e38, 0xbd014a9f, 0x25952abf, 0xfc02471, 0x78c, 2, 1, 1, 0, 0,
            0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x100},
        .pixel_program = 0x707019fe21480fafull,
        .vs_source = 0x16c6a87ea456f950ull, .ps_source = 0xce0306cd80f85ee9ull,
        .vertex = {.program = 0x2dce7401047787ed, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x41, .tex = {{.kind = 1}, {.kind = 1, .rect = 1, .shadow = 1, .flags = 8}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xa456f950, 0x16c6a87e, 0x80f85ee9, 0xce0306cd, 0xd, 2, 1, 1, 2, 0,
            0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x400, 0, 0, 0, 8},
        .pixel_program = 0xc1ad79eb34696ff9ull,
        .vs_source = 0xd876198aa691bf32ull, .ps_source = 0x36456f9db9076dfbull,
        .vertex = {.program = 0x47aaf22d90dc2f39, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 0x11044, .tex = {{.kind = 1}, {.kind = 1, .rect = 1, .shadow = 1, .flags = 8}, {}, {.kind = 1, .rect = 1, .shadow = 1, .flags = 8}}, .alphatest = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xa691bf32, 0xd876198a, 0xb9076dfb, 0x36456f9d, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0, 0x301, 0x8006, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x2000000, 0x200},
        .pixel_program = 0xc1ad79eb34696ff9ull,
        .vs_source = 0xd876198aa691bf32ull, .ps_source = 0x4bc26f3c09abfd07ull,
        .vertex = {.program = 0x47aaf22d90dc2f39, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 0x1044, .tex = {{.kind = 1}, {.kind = 1, .rect = 1, .shadow = 1, .flags = 8}, {}, {}}, .alphatest = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xa691bf32, 0xd876198a, 0x9abfd07, 0x4bc26f3c, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0, 0x301, 0x8006, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0x34, 0x100},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0x16c6a87ea456f950ull, .ps_source = 0x1bc359fadc88c8ebull,
        .vertex = {.program = 0x2dce7401047787ed, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xa456f950, 0x16c6a87e, 0xdc88c8eb, 0x1bc359fa, 0xd, 2, 1, 1, 2, 0,
            0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x431a90a2, 0x10000, 0x41a, 0x8c4, 0x100, 3, 0, 0xa},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0xb24e9ff4d88723d5ull, .ps_source = 0x1bc359fadc88c8ebull,
        .vertex = {.program = 0x777f3497aa0ce6e7, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xd88723d5, 0xb24e9ff4, 0xdc88c8eb, 0x1bc359fa, 0x78c, 2, 1, 1, 0, 0,
            0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0, 0, 4},
        .pixel_program = 0x08dcef845cc8aa5dull,
        .vs_source = 0x2b32c1b9db7207a5ull, .ps_source = 0xf8186c925e8630e9ull,
        .vertex = {.program = 0x6b2a205984c073cd, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 0x1040, .tex = {{}, {.kind = 1, .rect = 1, .shadow = 1, .flags = 8}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xdb7207a5, 0x2b32c1b9, 0x5e8630e9, 0xf8186c92, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0, 0, 4},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0x511179701f64e1bfull, .ps_source = 0x0c97a63062a9b088ull,
        .vertex = {.program = 0x1b707112de27a197, .regs = 0xb},
        .pixel = {.programmable = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x1f64e1bf, 0x51117970, 0x62a9b088, 0xc97a630, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0x10000},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x3e578683210c9781ull, .ps_source = 0x0fc0247125952abfull,
        .vertex = {.program = 0x5f10fffdb91e8bfd, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x210c9781, 0x3e578683, 0x25952abf, 0xfc02471, 0xf8c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0x400, 0, 0, 0, 1},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0x27ba05ab33f601e2ull, .ps_source = 0x713912116cadb0caull,
        .vertex = {.program = 0xb066871b0b7757c9, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x28, 0x33f601e2, 0x27ba05ab, 0x6cadb0ca, 0x71391211, 0xd, 2, 1, 1, 2, 0,
            0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0x40e00000, 0x57, 0
        },
    },
    {
        .rooms = {0, 0x10000, 0, 0x400, 0, 0x480, 0, 0x77c},
        .pixel_program = 0x08dcef845cc8aa5dull,
        .vs_source = 0x2b32c1b9db7207a5ull, .ps_source = 0xf1e67a050fb749d2ull,
        .vertex = {.program = 0x6b2a205984c073cd, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 0x1041, .tex = {{.kind = 1}, {.kind = 1, .rect = 1, .shadow = 1, .flags = 8}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xdb7207a5, 0x2b32c1b9, 0xfb749d2, 0xf1e67a05, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0x1501, 0x40203, 0x40090008, 0, 0x11},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0x39fadf1f37d05be3ull, .ps_source = 0x0676027f80e8ff24ull,
        .vertex = {.program = 0xbc5ae0492f0fa99f, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x37d05be3, 0x39fadf1f, 0x80e8ff24, 0x676027f, 0xf8c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x431a91ff, 0x1d202, 0x7bf, 0x5dff, 0xdc397, 0xdfffffff, 0xff, 0x3ff},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0x942d4e84e0b1a03bull, .ps_source = 0xa1aef30bfff71893ull,
        .vertex = {.program = 0x1b707112de27a197, .decode = 0x4000, .regs = 0xb, .quad = 1},
        .pixel = {.ff = {{5, 0, 2, 1, 5, 2, 0, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .proj = 3, .quad = 1, .alphatest = 1},
        .key_size = 192,
        .key = {
            1, 0x2d, 0xe0b1a03b, 0x942d4e84, 0xfff71893, 0xa1aef30b, 0xb, 2, 1, 1, 6, 0xc0057,
            0, 0x10002a, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 0, 0, 0, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x40008100, 0, 0x29, 0x13f, 0x10, 0xe000009, 0x18, 1},
        .pixel_program = 0x371532d23dfd51e5ull,
        .vs_source = 0xa5a62315d8939534ull, .ps_source = 0x373d777a5987e9e6ull,
        .vertex = {.program = 0x3c917fd1f5c3b869, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xd8939534, 0xa5a62315, 0x5987e9e6, 0x373d777a, 0xd, 2, 1, 1, 2, 0,
            0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0, 4, 0x140},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb0921fcf5c4bf28dull, .ps_source = 0x8eddd92961468648ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x5c4bf28d, 0xb0921fcf, 0x61468648, 0x8eddd929, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 1, 0x8006, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x100, 0, 0x40000008},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0x39fadf1f37d05be3ull, .ps_source = 0x0676027f80e8ff24ull,
        .vertex = {.program = 0xbc5ae0492f0fa99f, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x37d05be3, 0x39fadf1f, 0x80e8ff24, 0x676027f, 0xf8c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x431a91ff, 0x1f202, 0x7bf, 0x5dff, 0xdc397, 0xdfffffff, 0xff, 0xfff},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0xd94f276c4f3b1832ull, .ps_source = 0x64f3e9511b6371edull,
        .vertex = {.regs = 9, .pretransformed = 1, .quad = 1, .fixed = {.light = 0x905, .tex = {0x2000, 0x2001, 0x2002, 0x2003}}},
        .pixel = {.ff = {{2, 0, 2, 1, 2, 0, 0, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}, {}}, .quad = 1, .alphatest = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x4f3b1832, 0xd94f276c, 0x1b6371ed, 0x64f3e951, 9, 2, 1, 1, 2, 0,
            0, 0x100002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 7, 1, 0, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {1},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0xc25b3db045fd509cull, .ps_source = 0x6698a8724c8624dbull,
        .vertex = {.regs = 0x209, .pretransformed = 1, .quad = 1, .fixed = {.light = 0x905, .tex = {0x4000, 0x2001, 0x2002, 0x2003}}},
        .pixel = {.ff = {{4, 0, 2, 1, 4, 2, 0, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .quad = 1},
        .key_size = 192,
        .key = {
            1, 0x2d, 0x45fd509c, 0xc25b3db0, 0x4c8624db, 0x6698a872, 0x209, 2, 1, 1, 2, 0,
            0, 0x100002, 0, 0, 0, 0, 0, 0x200002, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x180112, 0, 0x200, 0, 0, 0x200, 4},
        .pixel_program = 0x14c4769017b92a89ull,
        .vs_source = 0xe2c7792f2aee3952ull, .ps_source = 0x2db08b5d0a67f2f5ull,
        .vertex = {.program = 0x1a5489ffb2b5409, .regs = 0x700},
        .pixel = {.programmable = 1},
        .key_size = 192,
        .key = {
            0, 0x28, 0x2aee3952, 0xe2c7792f, 0xa67f2f5, 0x2db08b5d, 0x700, 2, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0x80010, 0x100006, 0x1c0006, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0x40e00000, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0x80},
        .pixel_program = 0x5d2b1cfd399ca389ull,
        .vs_source = 0x19e2342776a16befull, .ps_source = 0x49ce1b8ae8ae3b8cull,
        .vertex = {.program = 0x65557b3077fe6af9, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x401, .tex = {{.kind = 1}, {}, {.kind = 1}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x76a16bef, 0x19e23427, 0xe8ae3b8c, 0x49ce1b8a, 0x38c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x21a91c5, 0x14202, 0x204, 0x20, 0, 0x84, 0x10, 0x104},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0x942d4e84e0b1a03bull, .ps_source = 0xa1aef30bfff71893ull,
        .vertex = {.program = 0x1b707112de27a197, .decode = 0x4000, .regs = 0xb, .quad = 1},
        .pixel = {.ff = {{5, 0, 2, 1, 5, 2, 0, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .proj = 3, .quad = 1, .alphatest = 1},
        .key_size = 192,
        .key = {
            1, 0x2d, 0xe0b1a03b, 0x942d4e84, 0xfff71893, 0xa1aef30b, 0xb, 2, 1, 1, 6, 0xc0057,
            0, 0x10002a, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0, 0, 0, 0, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x431a91f2, 0x1f200, 0x61a, 0xddc, 0x10100, 0xc800027f, 0x1c, 0xb},
        .pixel_program = 0xe5604726dbfa4855ull,
        .vs_source = 0xd707fb0349191332ull, .ps_source = 0xb9b9f4e16d77d14dull,
        .vertex = {.program = 0x501d667e282e8fb3, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 0x45, .tex = {{.kind = 1}, {.kind = 1, .rect = 1, .shadow = 1, .flags = 8}, {}, {}}, .alphatest = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x49191332, 0xd707fb03, 0x6d77d14d, 0xb9b9f4e1, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0, 0x301, 0x8006, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {1},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0xd94f276c4f3b1832ull, .ps_source = 0xc64289e5d568f24aull,
        .vertex = {.regs = 9, .pretransformed = 1, .quad = 1, .fixed = {.light = 0x905, .tex = {0x2000, 0x2001, 0x2002, 0x2003}}},
        .pixel = {.ff = {{2, 0, 2, 1, 2, 0, 0, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}, {}}, .quad = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x4f3b1832, 0xd94f276c, 0xd568f24a, 0xc64289e5, 9, 2, 1, 1, 2, 0,
            0, 0x100002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0, 0x80},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0x016237d57772694aull, .ps_source = 0x0676027f80e8ff24ull,
        .vertex = {.program = 0x3c3b3b2cb13ccbf9, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x7772694a, 0x16237d5, 0x80e8ff24, 0x676027f, 0xf8c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x180002},
        .pixel_program = 0x14c4769017b92a89ull,
        .vs_source = 0xe2c7792f2aee3952ull, .ps_source = 0x2db08b5d0a67f2f5ull,
        .vertex = {.program = 0x1a5489ffb2b5409, .regs = 0x700},
        .pixel = {.programmable = 1},
        .key_size = 192,
        .key = {
            0, 0x28, 0x2aee3952, 0xe2c7792f, 0xa67f2f5, 0x2db08b5d, 0x700, 2, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0x80010, 0x100006, 0x1c0006, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8007, 0, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0x40e00000, 0x57, 0
        },
    },
    {
        .rooms = {0x431a91ff, 0x1f202, 0x7bf, 0x5dff, 0xdc397, 0xdfffffff, 0xff, 0xfff},
        .pixel_program = 0x644eb632e9cfa30full,
        .vs_source = 0x33612bc25690508bull, .ps_source = 0xf4b45f37c2bcbcbaull,
        .vertex = {.regs = 0x201, .pretransformed = 1, .quad = 1, .fixed = {.light = 0x907, .tex = {0x2000, 0x2001, 0x2002, 0x2003}}},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1, .rect = 1, .flags = 8}, {}, {}, {}}, .quad = 1},
        .key_size = 192,
        .key = {
            0, 0, 0x5690508b, 0x33612bc2, 0xc2bcbcba, 0xf4b45f37, 0x201, 2, 1, 0, 2, 0,
            0, 0, 0, 0, 0, 0, 0, 0x100010, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x431a91ff, 0x1f202, 0x7bf, 0x5dff, 0xdc397, 0xdfffffff, 0xff, 0xfff},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0x511179701f64e1bfull, .ps_source = 0xe1f1cd2f1a51cea2ull,
        .vertex = {.program = 0x1b707112de27a197, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x1f64e1bf, 0x51117970, 0x1a51cea2, 0xe1f1cd2f, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x431a91ff, 0x1f202, 0x7bf, 0x5dff, 0xdc397, 0xdfffffff, 0xff, 0xfff},
        .pixel_program = 0x14c4769017b92a89ull,
        .vs_source = 0xe2c7792f2aee3952ull, .ps_source = 0x2db08b5d0a67f2f5ull,
        .vertex = {.program = 0x1a5489ffb2b5409, .regs = 0x700},
        .pixel = {.programmable = 1},
        .key_size = 192,
        .key = {
            0, 0x28, 0x2aee3952, 0xe2c7792f, 0xa67f2f5, 0x2db08b5d, 0x700, 2, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0x80010, 0x100006, 0x1c0006, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0x40e00000, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x800},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0x942d4e84e0b1a03bull, .ps_source = 0xa1aef30bfff71893ull,
        .vertex = {.program = 0x1b707112de27a197, .decode = 0x4000, .regs = 0xb, .quad = 1},
        .pixel = {.ff = {{5, 0, 2, 1, 5, 2, 0, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .proj = 3, .quad = 1, .alphatest = 1},
        .key_size = 192,
        .key = {
            1, 0x2d, 0xe0b1a03b, 0x942d4e84, 0xfff71893, 0xa1aef30b, 0xb, 2, 1, 1, 6, 0xc0057,
            0, 0x10002a, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 0, 0, 0, 1, 0x205,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e00, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0x40000008},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0x27ba05ab33f601e2ull, .ps_source = 0x0676027f80e8ff24ull,
        .vertex = {.program = 0xb066871b0b7757c9, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x33f601e2, 0x27ba05ab, 0x80e8ff24, 0x676027f, 0xd, 2, 1, 1, 2, 0,
            0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x21000a4, 0x200, 0x12, 0x880, 0, 0x40, 0, 0xa},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x049b3121cc1fd149ull, .ps_source = 0x1d173e9659f54b1eull,
        .vertex = {.program = 0xb3b57c5b9a7450af, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x28, 0xcc1fd149, 0x49b3121, 0x59f54b1e, 0x1d173e96, 0xd, 2, 1, 1, 2, 0,
            0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0x40e00000, 0x57, 0
        },
    },
    {
        .rooms = {0x31a91d5, 0x1d202, 0x7bf, 0x5971, 0xdc297, 0xdfffffff, 0xff, 0xfff},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0x942d4e84e0b1a03bull, .ps_source = 0xa1aef30bfff71893ull,
        .vertex = {.program = 0x1b707112de27a197, .decode = 0x4000, .regs = 0xb, .quad = 1},
        .pixel = {.ff = {{5, 0, 2, 1, 5, 2, 0, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .proj = 3, .quad = 1, .alphatest = 1},
        .key_size = 192,
        .key = {
            1, 0, 0xe0b1a03b, 0x942d4e84, 0xfff71893, 0xa1aef30b, 0xb, 2, 1, 0, 6, 0xc0057,
            0, 0x10002a, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 1},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xe1ee4965a28a8ab3ull, .ps_source = 0x8eddd92961468648ull,
        .vertex = {.program = 0x7ac2d148e0075b3b, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xa28a8ab3, 0xe1ee4965, 0x61468648, 0x8eddd929, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 1, 0x8006, 0xf, 1, 0, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x8000},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x468f2d86514ec50eull, .ps_source = 0x0fc0247125952abfull,
        .vertex = {.program = 0x2a09a1bcafcf5ef3, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x514ec50e, 0x468f2d86, 0x25952abf, 0xfc02471, 0x78c, 2, 1, 1, 0, 0,
            0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0x150b, 0x40287, 0x40090008, 0, 0x11},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0x27ba05ab33f601e2ull, .ps_source = 0x9e4e52d58a776022ull,
        .vertex = {.program = 0xb066871b0b7757c9, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x28, 0x33f601e2, 0x27ba05ab, 0x8a776022, 0x9e4e52d5, 0xd, 2, 1, 1, 2, 0,
            0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0x40e00000, 0x57, 0
        },
    },
    {
        .rooms = {0x10, 0, 0x200},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xd805f68f6727788full, .ps_source = 0x0fc0247125952abfull,
        .vertex = {.program = 0xbcd19491676d91cb, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x6727788f, 0xd805f68f, 0x25952abf, 0xfc02471, 0x78c, 2, 1, 1, 0, 0,
            0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x100, 0, 0x40000008},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0xb5cbb2dfa8de3c07ull, .ps_source = 0x0676027f80e8ff24ull,
        .vertex = {.program = 0x512c33a8e938e3fb, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xa8de3c07, 0xb5cbb2df, 0x80e8ff24, 0x676027f, 0x78c, 2, 1, 1, 0, 0,
            0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x800, 0x10001, 1},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xe1ee4965a28a8ab3ull, .ps_source = 0x8eddd92961468648ull,
        .vertex = {.program = 0x7ac2d148e0075b3b, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xa28a8ab3, 0xe1ee4965, 0x61468648, 0x8eddd929, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x1008111, 0x200, 0x524, 0, 0, 0x10100164},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x7f8aa52a5d21f5bdull, .ps_source = 0x0fc0247125952abfull,
        .vertex = {.program = 0x22ca82cb24abc905, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x5d21f5bd, 0x7f8aa52a, 0x25952abf, 0xfc02471, 0xf8c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x421a91ee, 0x1b002, 0x1f, 0x5c85, 0x9c397, 0x454401, 0x7f},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb0921fcf5c4bf28dull, .ps_source = 0x8eddd92961468648ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x5c4bf28d, 0xb0921fcf, 0x61468648, 0x8eddd929, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 0, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 4},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x049b3121cc1fd149ull, .ps_source = 0x1d173e9659f54b1eull,
        .vertex = {.program = 0xb3b57c5b9a7450af, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xcc1fd149, 0x49b3121, 0x59f54b1e, 0x1d173e96, 0xd, 2, 1, 1, 2, 0,
            0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x180112, 0, 0x200, 0, 0, 0x200, 4},
        .pixel_program = 0x14c4769017b92a89ull,
        .vs_source = 0x60d8ee3bb19de556ull, .ps_source = 0x2db08b5d0a67f2f5ull,
        .vertex = {.program = 0xa7f404fc37664173, .regs = 0xf00},
        .pixel = {.programmable = 1},
        .key_size = 192,
        .key = {
            0, 0x28, 0xb19de556, 0x60d8ee3b, 0xa67f2f5, 0x2db08b5d, 0xf00, 2, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 2, 0x100002, 0x200002, 0x300002, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0x40e00000, 0x57, 0
        },
    },
    {
        .rooms = {0x10a0104, 0x10000, 0x10, 0x10, 0x100},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0xb0c65e8bbd09767eull, .ps_source = 0x1bc359fadc88c8ebull,
        .vertex = {.program = 0xebe0a2b6413609b9, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xbd09767e, 0xb0c65e8b, 0xdc88c8eb, 0x1bc359fa, 0x78c, 2, 1, 1, 0, 0,
            0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0x500, 0x80, 0, 0, 1},
        .pixel_program = 0x5d2b1cfd399ca389ull,
        .vs_source = 0x0f424b77e7543b42ull, .ps_source = 0x49ce1b8ae8ae3b8cull,
        .vertex = {.program = 0x5bc4ef08d3a43ac7, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x401, .tex = {{.kind = 1}, {}, {.kind = 1}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xe7543b42, 0xf424b77, 0xe8ae3b8c, 0x49ce1b8a, 0xf8c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 8},
        .pixel_program = 0x5d2b1cfd399ca389ull,
        .vs_source = 0x8005e8111bdd8f72ull, .ps_source = 0x49ce1b8ae8ae3b8cull,
        .vertex = {.program = 0x89bf9deced319ae1, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x401, .tex = {{.kind = 1}, {}, {.kind = 1}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x1bdd8f72, 0x8005e811, 0xe8ae3b8c, 0x49ce1b8a, 0xf8c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x421a90be, 0x1f202, 0x29a, 0x48d4, 0x94100, 0x8fe6fe33, 0xfe, 0xfee},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xd805f68f6727788full, .ps_source = 0x0fc0247125952abfull,
        .vertex = {.program = 0xbcd19491676d91cb, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x6727788f, 0xd805f68f, 0x25952abf, 0xfc02471, 0x78c, 2, 1, 1, 0, 0,
            0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0x150b, 0x40287, 0x40090008, 0, 0x11},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0x27ba05ab33f601e2ull, .ps_source = 0x0676027f80e8ff24ull,
        .vertex = {.program = 0xb066871b0b7757c9, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x33f601e2, 0x27ba05ab, 0x80e8ff24, 0x676027f, 0xd, 2, 1, 1, 2, 0,
            0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x1010, 0x8000, 0x208, 0x800, 0x10000, 0x88000222, 0x3c},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xd805f68f6727788full, .ps_source = 0x0fc0247125952abfull,
        .vertex = {.program = 0xbcd19491676d91cb, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x6727788f, 0xd805f68f, 0x25952abf, 0xfc02471, 0x78c, 2, 1, 1, 0, 0,
            0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x40000000, 0, 0, 0x4000, 0, 0x200000},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0x511179701f64e1bfull, .ps_source = 0xe1f1cd2f1a51cea2ull,
        .vertex = {.program = 0x1b707112de27a197, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x1f64e1bf, 0x51117970, 0x1a51cea2, 0xe1f1cd2f, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 0, 0x203, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0, 0x80},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0x90a22e9c3795a41full, .ps_source = 0x0676027f80e8ff24ull,
        .vertex = {.program = 0xa5fded2d83e080ab, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x3795a41f, 0x90a22e9c, 0x80e8ff24, 0x676027f, 0x38c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x431a91ff, 0x1f202, 0x7bf, 0x5dff, 0xdc397, 0xdfffffff, 0xff, 0x3ff},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0x942d4e84e0b1a03bull, .ps_source = 0xa1aef30bfff71893ull,
        .vertex = {.program = 0x1b707112de27a197, .decode = 0x4000, .regs = 0xb, .quad = 1},
        .pixel = {.ff = {{5, 0, 2, 1, 5, 2, 0, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .proj = 3, .quad = 1, .alphatest = 1},
        .key_size = 192,
        .key = {
            1, 0, 0xe0b1a03b, 0x942d4e84, 0xfff71893, 0xa1aef30b, 0xb, 2, 1, 0, 6, 0xc0057,
            0, 0x10002a, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 7, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 4},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0x3d4f6844ef1cc87dull, .ps_source = 0x1bc359fadc88c8ebull,
        .vertex = {.program = 0x8ab3fe497032abb9, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xef1cc87d, 0x3d4f6844, 0xdc88c8eb, 0x1bc359fa, 0xf8c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x800, 0x10001, 1},
        .pixel_program = 0x08dcef845cc8aa5dull,
        .vs_source = 0x3f72fbe4b6661822ull, .ps_source = 0xf1e67a050fb749d2ull,
        .vertex = {.program = 0xc627357a3baea81b, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 0x1041, .tex = {{.kind = 1}, {.kind = 1, .rect = 1, .shadow = 1, .flags = 8}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xb6661822, 0x3f72fbe4, 0xfb749d2, 0xf1e67a05, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x180002},
        .pixel_program = 0x14c4769017b92a89ull,
        .vs_source = 0xbb3a6278c2e7ac01ull, .ps_source = 0x2db08b5d0a67f2f5ull,
        .vertex = {.program = 0x8d4f80ddb4f99031, .regs = 0x200},
        .pixel = {.programmable = 1},
        .key_size = 192,
        .key = {
            0, 0x28, 0xc2e7ac01, 0xbb3a6278, 0xa67f2f5, 0x2db08b5d, 0x200, 2, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8007, 0, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0x40e00000, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x800, 0, 1, 1},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0xc5bb1a1e0c934f69ull, .ps_source = 0xac5a867766049fbdull,
        .vertex = {.program = 0x1b707112de27a197, .decode = 0x4000, .regs = 0xb},
        .pixel = {.ff = {{5, 2, 0, 1, 5, 2, 0, 1, 1}, {2, 1, 1, 1, 1, 2, 1, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1, .proj = 7, .alphatest = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xc934f69, 0xc5bb1a1e, 0x66049fbd, 0xac5a8677, 0xb, 2, 1, 1, 6, 0xc0057,
            0, 0x10002a, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 1, 0x8006, 0xf, 1, 0, 0x203, 1, 0x205,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e00, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x100000, 0, 0, 0, 0, 0, 0x40},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb0921fcf5c4bf28dull, .ps_source = 0x8eddd92961468648ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x5c4bf28d, 0xb0921fcf, 0x61468648, 0x8eddd929, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 1, 0x800b, 0xf, 1, 0, 0x203, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x800, 0x10001, 1},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xe1ee4965a28a8ab3ull, .ps_source = 0x8eddd92961468648ull,
        .vertex = {.program = 0x7ac2d148e0075b3b, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xa28a8ab3, 0xe1ee4965, 0x61468648, 0x8eddd929, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 0, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0, 8},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xd805f68f6727788full, .ps_source = 0x1d173e9659f54b1eull,
        .vertex = {.program = 0xbcd19491676d91cb, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x6727788f, 0xd805f68f, 0x59f54b1e, 0x1d173e96, 0x78c, 2, 1, 1, 0, 0,
            0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x40100051, 0xc202, 0x84, 0x44bf, 0x40012, 0x8311210, 0x74, 1},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb0921fcf5c4bf28dull, .ps_source = 0x8eddd92961468648ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x5c4bf28d, 0xb0921fcf, 0x61468648, 0x8eddd929, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 1, 0x8006, 0xf, 1, 0, 0x203, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0, 8},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x62c593a3ab37d57full, .ps_source = 0x1d173e9659f54b1eull,
        .vertex = {.program = 0x697df3438a77f65f, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xab37d57f, 0x62c593a3, 0x59f54b1e, 0x1d173e96, 0xf8c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x21a91c5, 0x14202, 0x204, 0x20, 0, 0x84, 0x10, 0x104},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0x942d4e84e0b1a03bull, .ps_source = 0xa1aef30bfff71893ull,
        .vertex = {.program = 0x1b707112de27a197, .decode = 0x4000, .regs = 0xb, .quad = 1},
        .pixel = {.ff = {{5, 0, 2, 1, 5, 2, 0, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .proj = 3, .quad = 1, .alphatest = 1},
        .key_size = 192,
        .key = {
            1, 0x2d, 0xe0b1a03b, 0x942d4e84, 0xfff71893, 0xa1aef30b, 0xb, 2, 1, 1, 6, 0xc0057,
            0, 0x10002a, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 7, 0, 0, 0, 1, 0x205,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e00, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x431a91ff, 0x1f202, 0x7bf, 0x5dff, 0xdc397, 0xdfffffff, 0xff, 0xfff},
        .pixel_program = 0xd00d9b6109932b3dull,
        .vs_source = 0x8897b25a2056193cull, .ps_source = 0xc116b77759ca1680ull,
        .vertex = {.regs = 0x1e01, .pretransformed = 1, .quad = 1, .fixed = {.light = 0x907, .tex = {0x2000, 0x2001, 0x2002, 0x2003}}},
        .pixel = {.programmable = 1, .tex_modes = 0x8421, .tex = {{.kind = 1, .rect = 1, .flags = 8}, {.kind = 1, .rect = 1, .flags = 8}, {.kind = 1, .rect = 1, .flags = 8}, {.kind = 1, .rect = 1, .flags = 8}}, .quad = 1},
        .key_size = 192,
        .key = {
            0, 0, 0x2056193c, 0x8897b25a, 0x59ca1680, 0xc116b777, 0x1e01, 2, 1, 0, 2, 0,
            0, 0, 0, 0, 0, 0, 0, 0x100010, 0x180010, 0x200010, 0x280010, 0,
            0, 0, 0, 0, 0, 0, 0xf, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x8000, 0, 0x200, 0, 0, 0x20, 0x3c},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x049b3121cc1fd149ull, .ps_source = 0x0fc0247125952abfull,
        .vertex = {.program = 0xb3b57c5b9a7450af, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xcc1fd149, 0x49b3121, 0x25952abf, 0xfc02471, 0xd, 2, 1, 1, 2, 0,
            0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0x400, 0, 0, 0, 1},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0x27ba05ab33f601e2ull, .ps_source = 0xeb0cb85dabb700b4ull,
        .vertex = {.program = 0xb066871b0b7757c9, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x401, .tex = {{.kind = 1}, {}, {.kind = 1}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x33f601e2, 0x27ba05ab, 0xabb700b4, 0xeb0cb85d, 0xd, 2, 1, 1, 2, 0,
            0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0x10000, 0, 0x400, 0, 0, 0, 8},
        .pixel_program = 0xe5604726dbfa4855ull,
        .vs_source = 0xd707fb0349191332ull, .ps_source = 0xb9b9f4e16d77d14dull,
        .vertex = {.program = 0x501d667e282e8fb3, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 0x45, .tex = {{.kind = 1}, {.kind = 1, .rect = 1, .shadow = 1, .flags = 8}, {}, {}}, .alphatest = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x49191332, 0xd707fb03, 0x6d77d14d, 0xb9b9f4e1, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0, 0x301, 0x8006, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x431a91ef, 0x17202, 0x7bf, 0x4dff, 0xd8397, 0x4bffff7f, 0x9f, 0xc2f},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0xc5bb1a1e0c934f69ull, .ps_source = 0xac5a867766049fbdull,
        .vertex = {.program = 0x1b707112de27a197, .decode = 0x4000, .regs = 0xb},
        .pixel = {.ff = {{5, 2, 0, 1, 5, 2, 0, 1, 1}, {2, 1, 1, 1, 1, 2, 1, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1, .proj = 7, .alphatest = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xc934f69, 0xc5bb1a1e, 0x66049fbd, 0xac5a8677, 0xb, 2, 1, 1, 6, 0xc0057,
            0, 0x10002a, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 1, 0x8006, 0xf, 1, 0, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x431a91ff, 0x1f202, 0x7bf, 0x5dff, 0xdc397, 0xdfffffff, 0xff, 0xfff},
        .pixel_program = 0xd00d9b6109932b3dull,
        .vs_source = 0x8897b25a2056193cull, .ps_source = 0xc116b77759ca1680ull,
        .vertex = {.regs = 0x1e01, .pretransformed = 1, .quad = 1, .fixed = {.light = 0x907, .tex = {0x2000, 0x2001, 0x2002, 0x2003}}},
        .pixel = {.programmable = 1, .tex_modes = 0x8421, .tex = {{.kind = 1, .rect = 1, .flags = 8}, {.kind = 1, .rect = 1, .flags = 8}, {.kind = 1, .rect = 1, .flags = 8}, {.kind = 1, .rect = 1, .flags = 8}}, .quad = 1},
        .key_size = 192,
        .key = {
            0, 0, 0x2056193c, 0x8897b25a, 0x59ca1680, 0xc116b777, 0x1e01, 2, 1, 0, 2, 0,
            0, 0, 0, 0, 0, 0, 0, 0x100010, 0x180010, 0x200010, 0x280010, 0,
            0, 0, 1, 1, 1, 0x8006, 0xf, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x180002},
        .pixel_program = 0x14c4769017b92a89ull,
        .vs_source = 0x60d8ee3bb19de556ull, .ps_source = 0x2db08b5d0a67f2f5ull,
        .vertex = {.program = 0xa7f404fc37664173, .regs = 0xf00},
        .pixel = {.programmable = 1},
        .key_size = 192,
        .key = {
            0, 0x28, 0xb19de556, 0x60d8ee3b, 0xa67f2f5, 0x2db08b5d, 0xf00, 2, 1, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 2, 0x100002, 0x200002, 0x300002, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8007, 0, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0x40e00000, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0x80},
        .pixel_program = 0x5d2b1cfd399ca389ull,
        .vs_source = 0x51cc682eb63ded13ull, .ps_source = 0x49ce1b8ae8ae3b8cull,
        .vertex = {.program = 0x5d04d37883312711, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x401, .tex = {{.kind = 1}, {}, {.kind = 1}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xb63ded13, 0x51cc682e, 0xe8ae3b8c, 0x49ce1b8a, 0x78c, 2, 1, 1, 0, 0,
            0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x180012, 0, 0, 0, 0, 0x200, 4},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0x9d818be217c867c7ull, .ps_source = 0x1bc359fadc88c8ebull,
        .vertex = {.program = 0xef9a4b7428e4ed33, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x17c867c7, 0x9d818be2, 0xdc88c8eb, 0x1bc359fa, 0x38c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0x80},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x62c593a3ab37d57full, .ps_source = 0xa9dd569ea9c5c309ull,
        .vertex = {.program = 0x697df3438a77f65f, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}, .alphakill = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xab37d57f, 0x62c593a3, 0xa9c5c309, 0xa9dd569e, 0xf8c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0x44},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0x85058ddf5b6375e6ull, .ps_source = 0x1bc359fadc88c8ebull,
        .vertex = {.program = 0x4390a90fe19250ff, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x5b6375e6, 0x85058ddf, 0xdc88c8eb, 0x1bc359fa, 0xf8c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0, 8},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0x511179701f64e1bfull, .ps_source = 0xe1f1cd2f1a51cea2ull,
        .vertex = {.program = 0x1b707112de27a197, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x1f64e1bf, 0x51117970, 0x1a51cea2, 0xe1f1cd2f, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 0, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x800, 1, 1},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xe1ee4965a28a8ab3ull, .ps_source = 0xe1f1cd2f1a51cea2ull,
        .vertex = {.program = 0x7ac2d148e0075b3b, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0, 0xa28a8ab3, 0xe1ee4965, 0x1a51cea2, 0xe1f1cd2f, 0xb, 2, 1, 0, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x1010, 0x8000, 0x208, 0x800, 0x10000, 0x88000222, 0x3c},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x62c593a3ab37d57full, .ps_source = 0x0fc0247125952abfull,
        .vertex = {.program = 0x697df3438a77f65f, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xab37d57f, 0x62c593a3, 0x25952abf, 0xfc02471, 0xf8c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x1008111, 0x200, 0x524, 0, 0, 0x10100164},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x468f2d86514ec50eull, .ps_source = 0x0fc0247125952abfull,
        .vertex = {.program = 0x2a09a1bcafcf5ef3, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x514ec50e, 0x468f2d86, 0x25952abf, 0xfc02471, 0x78c, 2, 1, 1, 0, 0,
            0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x108040, 0, 0, 0x1000, 0x291, 1, 0xaf},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb0921fcf5c4bf28dull, .ps_source = 0x8eddd92961468648ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x5c4bf28d, 0xb0921fcf, 0x61468648, 0x8eddd929, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 1, 0x8006, 0xf, 1, 0, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x8000},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xf412ef04aa3cbed8ull, .ps_source = 0x0fc0247125952abfull,
        .vertex = {.program = 0x172f719c73ce6401, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xaa3cbed8, 0xf412ef04, 0x25952abf, 0xfc02471, 0x38c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0x200, 0x2c, 0x100b, 0x200, 0x8000000, 0, 0x21},
        .pixel_program = 0x371532d23dfd51e5ull,
        .vs_source = 0x7366d16dc7d60f1eull, .ps_source = 0x373d777a5987e9e6ull,
        .vertex = {.program = 0x98230210e4906433, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xc7d60f1e, 0x7366d16d, 0x5987e9e6, 0x373d777a, 0xd, 2, 1, 1, 2, 0,
            0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x10a0104, 0x10200, 0x10, 0x10, 0x100},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0xac56f5ca68812b03ull, .ps_source = 0x1bc359fadc88c8ebull,
        .vertex = {.program = 0xad3f9bf87748856d, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x68812b03, 0xac56f5ca, 0xdc88c8eb, 0x1bc359fa, 0x38c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x100},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0x304b3c8e2c5549d1ull, .ps_source = 0x1bc359fadc88c8ebull,
        .vertex = {.program = 0xfa08fa55ec14ec6b, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x2c5549d1, 0x304b3c8e, 0xdc88c8eb, 0x1bc359fa, 0x78c, 2, 1, 1, 0, 0,
            0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x421a90be, 0x1f202, 0x29a, 0x48d4, 0x94100, 0x8fe6fe33, 0xfe, 0xfee},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x62c593a3ab37d57full, .ps_source = 0x0fc0247125952abfull,
        .vertex = {.program = 0x697df3438a77f65f, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xab37d57f, 0x62c593a3, 0x25952abf, 0xfc02471, 0xf8c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {1},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0x33612bc25690508bull, .ps_source = 0xe05f75562df5f066ull,
        .vertex = {.regs = 0x201, .pretransformed = 1, .quad = 1, .fixed = {.light = 0x905, .tex = {0x2000, 0x2001, 0x2002, 0x2003}}},
        .pixel = {.ff = {{2, 2, 2, 1, 1, 2, 0, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}, {}}, .tex = {{.kind = 1, .rect = 1}, {}, {}, {}}, .quad = 1},
        .key_size = 192,
        .key = {
            0, 0, 0x5690508b, 0x33612bc2, 0x2df5f066, 0xe05f7556, 0x201, 2, 1, 0, 2, 0,
            0, 0, 0, 0, 0, 0, 0, 0x100010, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x8000, 0, 0, 0, 0, 0, 0, 4},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0x16c6a87ea456f950ull, .ps_source = 0xa540a28364eb9173ull,
        .vertex = {.program = 0x2dce7401047787ed, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xa456f950, 0x16c6a87e, 0x64eb9173, 0xa540a283, 0xd, 2, 1, 1, 2, 0,
            0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x1008111, 0x200, 0x524, 0, 0, 0x10100164},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xf412ef04aa3cbed8ull, .ps_source = 0x0fc0247125952abfull,
        .vertex = {.program = 0x172f719c73ce6401, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xaa3cbed8, 0xf412ef04, 0x25952abf, 0xfc02471, 0x38c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x10a8040, 0x1200, 0x405, 0x157b, 0, 0x890010d, 0x98, 0xa45},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb0921fcf5c4bf28dull, .ps_source = 0x8eddd92961468648ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x5c4bf28d, 0xb0921fcf, 0x61468648, 0x8eddd929, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 1, 0x8006, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0, 0, 4},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb0921fcf5c4bf28dull, .ps_source = 0x8eddd92961468648ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x5c4bf28d, 0xb0921fcf, 0x61468648, 0x8eddd929, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 1, 0x800b, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x31a91fd, 0x7202, 0x687, 0x4dff, 0xd8316, 0x5bc0a5f, 0x97, 0x10f},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0xc5bb1a1e0c934f69ull, .ps_source = 0xac5a867766049fbdull,
        .vertex = {.program = 0x1b707112de27a197, .decode = 0x4000, .regs = 0xb},
        .pixel = {.ff = {{5, 2, 0, 1, 5, 2, 0, 1, 1}, {2, 1, 1, 1, 1, 2, 1, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1, .proj = 7, .alphatest = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xc934f69, 0xc5bb1a1e, 0x66049fbd, 0xac5a8677, 0xb, 2, 1, 1, 6, 0xc0057,
            0, 0x10002a, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 0, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x800, 0, 1},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xe1ee4965a28a8ab3ull, .ps_source = 0x8eddd92961468648ull,
        .vertex = {.program = 0x7ac2d148e0075b3b, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xa28a8ab3, 0xe1ee4965, 0x61468648, 0x8eddd929, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 1, 0x8006, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x8008, 0x10000, 0x100, 0x579, 0x10, 0x214480, 0x27, 0x57e},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0x511179701f64e1bfull, .ps_source = 0xe1f1cd2f1a51cea2ull,
        .vertex = {.program = 0x1b707112de27a197, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x1f64e1bf, 0x51117970, 0x1a51cea2, 0xe1f1cd2f, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 1},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xe1ee4965a28a8ab3ull, .ps_source = 0x8eddd92961468648ull,
        .vertex = {.program = 0x7ac2d148e0075b3b, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xa28a8ab3, 0xe1ee4965, 0x61468648, 0x8eddd929, 0xb, 2, 1, 1, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 0, 0x203, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0xa8130, 0x10200, 0x49f, 0x30, 0x8110, 0x108000, 3},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xbd014a9fe5670e38ull, .ps_source = 0x0fc0247125952abfull,
        .vertex = {.program = 0x6b228efce02817df, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xe5670e38, 0xbd014a9f, 0x25952abf, 0xfc02471, 0x78c, 2, 1, 1, 0, 0,
            0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x180012, 0, 0, 0, 0, 0x200, 4},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0xb24e9ff4d88723d5ull, .ps_source = 0x1bc359fadc88c8ebull,
        .vertex = {.program = 0x777f3497aa0ce6e7, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xd88723d5, 0xb24e9ff4, 0xdc88c8eb, 0x1bc359fa, 0x78c, 2, 1, 1, 0, 0,
            0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0, 0,
            0, 0, 1, 0x302, 0x303, 0x8006, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x103, 0, 0x90000},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0x7bdb42824468c9c5ull, .ps_source = 0x0676027f80e8ff24ull,
        .vertex = {.program = 0x17193de2700d9e31, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x4468c9c5, 0x7bdb4282, 0x80e8ff24, 0x676027f, 0xd, 2, 1, 1, 2, 0,
            0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0xa8130, 0x10200, 0x49f, 0x30, 0x8110, 0x108000, 3},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x114d7c7f584531d6ull, .ps_source = 0x0fc0247125952abfull,
        .vertex = {.program = 0xcc0a3244080130e1, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x584531d6, 0x114d7c7f, 0x25952abf, 0xfc02471, 0x38c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {1, 2, 1, 0x20, 0, 0x3f6fd00, 0x20, 0x800},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xc3b79726226eb8c1ull, .ps_source = 0x0fc0247125952abfull,
        .vertex = {.program = 0xccd3b9a74a22c2cb, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x226eb8c1, 0xc3b79726, 0x25952abf, 0xfc02471, 0xd, 2, 1, 1, 2, 0,
            0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x10a0104, 0x10000, 0x10, 0x10, 0x100},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0x3d4f6844ef1cc87dull, .ps_source = 0x1bc359fadc88c8ebull,
        .vertex = {.program = 0x8ab3fe497032abb9, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0xef1cc87d, 0x3d4f6844, 0xdc88c8eb, 0x1bc359fa, 0xf8c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x400001a2, 0x1d202, 0x280, 0x404, 0, 0x89f04c00, 0x93, 0x40a},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb0921fcf5c4bf28dull, .ps_source = 0xe1f1cd2f1a51cea2ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 192,
        .key = {
            0, 0, 0x5c4bf28d, 0xb0921fcf, 0x1a51cea2, 0xe1f1cd2f, 0xb, 2, 1, 0, 2, 0x100002,
            0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0x302, 1, 0x8006, 0xf, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 1, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0xb, 0x85, 8, 0, 1},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0x3c28f8644384f8d8ull, .ps_source = 0x0676027f80e8ff24ull,
        .vertex = {.program = 0x158593bbf655b6cd, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 192,
        .key = {
            0, 0x2d, 0x4384f8d8, 0x3c28f864, 0x80e8ff24, 0x676027f, 0x38c, 2, 1, 1, 0, 0,
            2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0xf, 1, 1, 0x203, 1, 0x207,
            0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0, 0, 0x1b02, 0, 0, 0x57, 0
        },
    },
};
#endif

constexpr uint64_t kVulkanCatalogLayout = 0x00005c0000000108ull;
constexpr CatalogEntry kVulkanCatalog[] = {
    {
        .rooms = {0, 0, 0, 0x103, 0, 0x90000},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0x4624880b13eab50eull, .ps_source = 0xb3b881b2aa8634a2ull,
        .vertex = {.program = 0x17193de2700d9e31, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x13eab50e, 0x4624880b, 0xaa8634a2, 0xb3b881b2, 0xd, 2, 0, 1, 1, 2,
            0, 0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x10},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x4838d2769945e5cfull, .ps_source = 0x7722ce998710d5e7ull,
        .vertex = {.program = 0x697df3438a77f65f, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x9945e5cf, 0x4838d276, 0x8710d5e7, 0x7722ce99, 0xf8c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0,
            0, 0, 0, 1, 0x70, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x800, 1, 0x40000000, 2},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xc23c9367f0401226ull, .ps_source = 0xe8f1a70f9cdfc4a2ull,
        .vertex = {.program = 0x7ac2d148e0075b3b, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xf0401226, 0xc23c9367, 0x9cdfc4a2, 0xe8f1a70f, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x40000000, 0, 0, 0x4000, 0, 0x200000},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xa54a8f95e74215fdull, .ps_source = 0x1c1e78d787b5cb15ull,
        .vertex = {.program = 0x1b707112de27a197, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xe74215fd, 0xa54a8f95, 0x87b5cb15, 0x1c1e78d7, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 0, 0x203, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0x34, 0x100},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0xffb005e174bf40f3ull, .ps_source = 0x3a5ef96fb6ebb554ull,
        .vertex = {.program = 0x2dce7401047787ed, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x74bf40f3, 0xffb005e1, 0xb6ebb554, 0x3a5ef96f, 0xd, 2, 0, 1, 1, 2,
            0, 0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0x400, 0, 0, 0, 1},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0xeca46f0f0f3210e2ull, .ps_source = 0x907d828aae45088dull,
        .vertex = {.program = 0xb066871b0b7757c9, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x401, .tex = {{.kind = 1}, {}, {.kind = 1}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xf3210e2, 0xeca46f0f, 0xae45088d, 0x907d828a, 0xd, 2, 0, 1, 1, 2,
            0, 0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x30a9155, 0x10202, 0x204, 0x830, 0x10000, 0x81249249, 0x18, 0x800},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0x4f499e8f1f8af5d8ull, .ps_source = 0x3501a69ad04724c0ull,
        .vertex = {.program = 0x1b707112de27a197, .decode = 0x4000, .regs = 0xb, .quad = 1},
        .pixel = {.ff = {{5, 0, 2, 1, 5, 2, 0, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .proj = 3, .quad = 1, .alphatest = 1},
        .key_size = 264,
        .key = {
            1, 0, 0x1f8af5d8, 0x4f499e8f, 0xd04724c0, 0x3501a69a, 0xb, 2, 1, 1, 1, 6,
            0xc0057, 0, 0x10002a, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x14, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0, 0, 0, 0, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x180002},
        .pixel_program = 0x14c4769017b92a89ull,
        .vs_source = 0x1fd2d5acd50db8dfull, .ps_source = 0x5932d33fe3b52cb7ull,
        .vertex = {.program = 0x1a5489ffb2b5409, .regs = 0x700},
        .pixel = {.programmable = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xd50db8df, 0x1fd2d5ac, 0xe3b52cb7, 0x5932d33f, 0x700, 2, 1, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0x80010, 0x100006, 0x1c0006, 0, 0,
            0, 0, 0, 1, 0x2c, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8007,
            0, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0x40e00000, 0x2c, 0
        },
    },
    {
        .rooms = {0x30a11a4, 0x200, 0x1a, 0x880, 0, 0x46, 0, 0xa},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xf8700d69cbaf7e5bull, .ps_source = 0xb718a4e855630046ull,
        .vertex = {.program = 0xb3b57c5b9a7450af, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xcbaf7e5b, 0xf8700d69, 0x55630046, 0xb718a4e8, 0xd, 2, 0, 1, 1, 2,
            0, 0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0x400, 0, 0, 0, 1},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0xeca46f0f0f3210e2ull, .ps_source = 0x4b17760546476530ull,
        .vertex = {.program = 0xb066871b0b7757c9, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xf3210e2, 0xeca46f0f, 0x46476530, 0x4b177605, 0xd, 2, 0, 1, 1, 2,
            0, 0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0x40e00000, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0x10000, 0, 0x400, 0, 0, 0, 8},
        .pixel_program = 0xe5604726dbfa4855ull,
        .vs_source = 0xb8a30eac312ca5eaull, .ps_source = 0x3a0ce0d69658dfc2ull,
        .vertex = {.program = 0x501d667e282e8fb3, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 0x45, .tex = {{.kind = 1}, {.kind = 1, .rect = 1, .shadow = 1}, {}, {}}, .alphatest = 1},
        .key_size = 264,
        .key = {
            0, 0, 0x312ca5ea, 0xb8a30eac, 0x9658dfc2, 0x3a0ce0d6, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0x301, 0x8006,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x1000000, 0, 0, 0, 0, 0x44},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0xde4eea3e159f88bdull, .ps_source = 0x3a5ef96fb6ebb554ull,
        .vertex = {.program = 0x4390a90fe19250ff, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x159f88bd, 0xde4eea3e, 0xb6ebb554, 0x3a5ef96f, 0xf8c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0,
            0, 0, 0, 1, 0x70, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x8000, 0, 0x200, 0, 0, 0x20, 0x1c},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xf8700d69cbaf7e5bull, .ps_source = 0x7722ce998710d5e7ull,
        .vertex = {.program = 0xb3b57c5b9a7450af, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xcbaf7e5b, 0xf8700d69, 0x8710d5e7, 0x7722ce99, 0xd, 2, 0, 1, 1, 2,
            0, 0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x8000},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x8953da43c65301a1ull, .ps_source = 0x7722ce998710d5e7ull,
        .vertex = {.program = 0x2a09a1bcafcf5ef3, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xc65301a1, 0x8953da43, 0x8710d5e7, 0x7722ce99, 0x78c, 2, 1, 1, 1, 0,
            0, 0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0,
            0, 0, 0, 1, 0x2c, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x180012, 0, 0, 0, 0, 0x200, 4},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0xfaa4e63b89e7d2beull, .ps_source = 0x3a5ef96fb6ebb554ull,
        .vertex = {.program = 0xef9a4b7428e4ed33, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x89e7d2be, 0xfaa4e63b, 0xb6ebb554, 0x3a5ef96f, 0x38c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0,
            0, 0, 0, 1, 0x50, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0, 0, 4},
        .pixel_program = 0x08dcef845cc8aa5dull,
        .vs_source = 0xc0b33c4ae67c6810ull, .ps_source = 0xe0a623fa7004c161ull,
        .vertex = {.program = 0x6b2a205984c073cd, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 0x1040, .tex = {{}, {.kind = 1, .rect = 1, .shadow = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xe67c6810, 0xc0b33c4a, 0x7004c161, 0xe0a623fa, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0x1509, 0x40283, 0x40090008, 0, 0x10},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0xe52d4077e2733cb7ull, .ps_source = 0xb3b881b2aa8634a2ull,
        .vertex = {.program = 0xa0cbcf00aa8ae0c3, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xe2733cb7, 0xe52d4077, 0xaa8634a2, 0xb3b881b2, 0x38c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0,
            0, 0, 0, 1, 0x50, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x100, 0x40000, 0x40000008},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0x518af26744051928ull, .ps_source = 0xb3b881b2aa8634a2ull,
        .vertex = {.program = 0xbc5ae0492f0fa99f, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x44051928, 0x518af267, 0xaa8634a2, 0xb3b881b2, 0xf8c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0,
            0, 0, 0, 1, 0x70, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x10a0104, 0x10200, 0x10, 0x10, 0x100},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0xc8e5cf44f1d2e2f5ull, .ps_source = 0x3a5ef96fb6ebb554ull,
        .vertex = {.program = 0xad3f9bf87748856d, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xf1d2e2f5, 0xc8e5cf44, 0xb6ebb554, 0x3a5ef96f, 0x38c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0,
            0, 0, 0, 1, 0x50, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x31a91bf, 0x1f202, 0x7bf, 0x48f4, 0x9c110, 0x9ff6fff7, 0xff, 0xfee},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xf8700d69cbaf7e5bull, .ps_source = 0x7722ce998710d5e7ull,
        .vertex = {.program = 0xb3b57c5b9a7450af, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xcbaf7e5b, 0xf8700d69, 0x8710d5e7, 0x7722ce99, 0xd, 2, 0, 1, 1, 2,
            0, 0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0x40e00000, 0x2c, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0xa, 0x40080},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0x7171caa5b9e9f897ull, .ps_source = 0xb3b881b2aa8634a2ull,
        .vertex = {.program = 0xa5fded2d83e080ab, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xb9e9f897, 0x7171caa5, 0xaa8634a2, 0xb3b881b2, 0x38c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0,
            0, 0, 0, 1, 0x50, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x1010, 0x8000, 0x208, 0x800, 0x10000, 0x88000222, 0x1c},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x4838d2769945e5cfull, .ps_source = 0x7722ce998710d5e7ull,
        .vertex = {.program = 0x697df3438a77f65f, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x9945e5cf, 0x4838d276, 0x8710d5e7, 0x7722ce99, 0xf8c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0,
            0, 0, 0, 1, 0x70, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x30a9155, 0x10202, 0x204, 0x830, 0x10000, 0x81249249, 0x18, 0x800},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0x4f499e8f1f8af5d8ull, .ps_source = 0x3501a69ad04724c0ull,
        .vertex = {.program = 0x1b707112de27a197, .decode = 0x4000, .regs = 0xb, .quad = 1},
        .pixel = {.ff = {{5, 0, 2, 1, 5, 2, 0, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .proj = 3, .quad = 1, .alphatest = 1},
        .key_size = 264,
        .key = {
            1, 0, 0x1f8af5d8, 0x4f499e8f, 0xd04724c0, 0x3501a69a, 0xb, 2, 1, 1, 1, 6,
            0xc0057, 0, 0x10002a, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x14, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            7, 0, 0, 0, 1, 0x205, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e00, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0, 8},
        .pixel_program = 0x371532d23dfd51e5ull,
        .vs_source = 0xe7ea3ca1cfa873eeull, .ps_source = 0x97bec5a5d67ecd76ull,
        .vertex = {.program = 0x3c917fd1f5c3b869, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}, .alphakill = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xcfa873ee, 0xe7ea3ca1, 0xd67ecd76, 0x97bec5a5, 0xd, 2, 0, 1, 1, 2,
            0, 0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 0, 0x203, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x180012, 0, 0, 0, 0, 0x200, 4},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0xe8ddae10bfeb5d16ull, .ps_source = 0x3a5ef96fb6ebb554ull,
        .vertex = {.program = 0x777f3497aa0ce6e7, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xbfeb5d16, 0xe8ddae10, 0xb6ebb554, 0x3a5ef96f, 0x78c, 2, 1, 1, 1, 0,
            0, 0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0,
            0, 0, 0, 1, 0x2c, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0xb, 0x40085, 8, 0, 1},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0x8b454da4bf448991ull, .ps_source = 0xb3b881b2aa8634a2ull,
        .vertex = {.program = 0x158593bbf655b6cd, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xbf448991, 0x8b454da4, 0xaa8634a2, 0xb3b881b2, 0x38c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0,
            0, 0, 0, 1, 0x50, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x1010, 0x8000, 0x208, 0x800, 0x10000, 0x88000222, 0x1c},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xce4cb173459caeb1ull, .ps_source = 0x7722ce998710d5e7ull,
        .vertex = {.program = 0xbcd19491676d91cb, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x459caeb1, 0xce4cb173, 0x8710d5e7, 0x7722ce99, 0x78c, 2, 1, 1, 1, 0,
            0, 0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0,
            0, 0, 0, 1, 0x2c, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x180002},
        .pixel_program = 0x14c4769017b92a89ull,
        .vs_source = 0xc78f0202244d3b33ull, .ps_source = 0x5932d33fe3b52cb7ull,
        .vertex = {.program = 0xa7f404fc37664173, .regs = 0xf00},
        .pixel = {.programmable = 1},
        .key_size = 264,
        .key = {
            0, 0, 0x244d3b33, 0xc78f0202, 0xe3b52cb7, 0x5932d33f, 0xf00, 2, 0, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 2, 0x100002, 0x200002, 0x300002, 0,
            0, 0, 0, 1, 0x40, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8007,
            0, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0x40e00000, 0x2c, 0
        },
    },
    {
        .rooms = {0x90a0, 0, 0x1b, 0xc6, 0x300, 0x4004, 8, 0xf},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0x3e41aba8126adbafull, .ps_source = 0x59d62613374ecaf5ull,
        .vertex = {.program = 0x1b707112de27a197, .decode = 0x4000, .regs = 0xb},
        .pixel = {.ff = {{5, 2, 0, 1, 5, 2, 0, 1, 1}, {2, 1, 1, 1, 1, 2, 1, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1, .proj = 7, .alphatest = 1},
        .key_size = 264,
        .key = {
            0, 0, 0x126adbaf, 0x3e41aba8, 0x374ecaf5, 0x59d62613, 0xb, 2, 0, 1, 1, 6,
            0xc0057, 0, 0x10002a, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x14, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 1, 0x800b,
            0xf, 1, 0, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0xb, 0x40085, 8, 0, 1},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0x495189642314567cull, .ps_source = 0xb3b881b2aa8634a2ull,
        .vertex = {.program = 0xfd446e65656abb89, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x2314567c, 0x49518964, 0xaa8634a2, 0xb3b881b2, 0x78c, 2, 1, 1, 1, 0,
            0, 0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0,
            0, 0, 0, 1, 0x2c, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0, 2},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xc23c9367f0401226ull, .ps_source = 0x1c1e78d787b5cb15ull,
        .vertex = {.program = 0x7ac2d148e0075b3b, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xf0401226, 0xc23c9367, 0x87b5cb15, 0x1c1e78d7, 0xb, 2, 0, 1, 0, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 1, 0x8006,
            0xf, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 4},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0x7dacbc78ccc44855ull, .ps_source = 0x3a5ef96fb6ebb554ull,
        .vertex = {.program = 0xebe0a2b6413609b9, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xccc44855, 0x7dacbc78, 0xb6ebb554, 0x3a5ef96f, 0x78c, 2, 1, 1, 1, 0,
            0, 0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0,
            0, 0, 0, 1, 0x2c, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x31a917c, 0x3202, 0x687, 0x4dff, 0xd8396, 0x49bc0a7f, 0x9f, 0x10f},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0x3e41aba8126adbafull, .ps_source = 0x59d62613374ecaf5ull,
        .vertex = {.program = 0x1b707112de27a197, .decode = 0x4000, .regs = 0xb},
        .pixel = {.ff = {{5, 2, 0, 1, 5, 2, 0, 1, 1}, {2, 1, 1, 1, 1, 2, 1, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1, .proj = 7, .alphatest = 1},
        .key_size = 264,
        .key = {
            0, 0, 0x126adbaf, 0x3e41aba8, 0x374ecaf5, 0x59d62613, 0xb, 2, 0, 1, 1, 6,
            0xc0057, 0, 0x10002a, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x14, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 0, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x30a90a2, 0x10200, 0x41a, 0x8c4, 0x100, 3, 0, 0xa},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0xfaa4e63b89e7d2beull, .ps_source = 0x3a5ef96fb6ebb554ull,
        .vertex = {.program = 0xef9a4b7428e4ed33, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x89e7d2be, 0xfaa4e63b, 0xb6ebb554, 0x3a5ef96f, 0x38c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0,
            0, 0, 0, 1, 0x50, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x800, 1, 0, 2},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xc23c9367f0401226ull, .ps_source = 0xe8f1a70f9cdfc4a2ull,
        .vertex = {.program = 0x7ac2d148e0075b3b, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xf0401226, 0xc23c9367, 0x9cdfc4a2, 0xe8f1a70f, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 0, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x8000, 0, 0, 0, 0, 0, 0, 4},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0xffb005e174bf40f3ull, .ps_source = 0x9e60706d402c0640ull,
        .vertex = {.program = 0x2dce7401047787ed, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 264,
        .key = {
            0, 0, 0x74bf40f3, 0xffb005e1, 0x402c0640, 0x9e60706d, 0xd, 2, 0, 1, 1, 2,
            0, 0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x800, 0, 1, 1},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0x3e41aba8126adbafull, .ps_source = 0x59d62613374ecaf5ull,
        .vertex = {.program = 0x1b707112de27a197, .decode = 0x4000, .regs = 0xb},
        .pixel = {.ff = {{5, 2, 0, 1, 5, 2, 0, 1, 1}, {2, 1, 1, 1, 1, 2, 1, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1, .proj = 7, .alphatest = 1},
        .key_size = 264,
        .key = {
            0, 0, 0x126adbaf, 0x3e41aba8, 0x374ecaf5, 0x59d62613, 0xb, 2, 0, 1, 1, 6,
            0xc0057, 0, 0x10002a, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x14, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 1, 0x8006,
            0xf, 1, 0, 0x203, 1, 0x205, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e00, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0x1509, 0x40283, 0x40090008, 0, 0x10},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0x948bf844aafadd08ull, .ps_source = 0xb3b881b2aa8634a2ull,
        .vertex = {.program = 0x512c33a8e938e3fb, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xaafadd08, 0x948bf844, 0xaa8634a2, 0xb3b881b2, 0x78c, 2, 1, 1, 1, 0,
            0, 0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0,
            0, 0, 0, 1, 0x2c, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x8040, 0, 0, 0x1000, 0x40291, 1, 0xad},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb3fe8e9ae9415352ull, .ps_source = 0xe8f1a70f9cdfc4a2ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xe9415352, 0xb3fe8e9a, 0x9cdfc4a2, 0xe8f1a70f, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 1, 0x8006,
            0xf, 1, 0, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x431a91ff, 0x1f202, 0x7bf, 0x5dff, 0xdc397, 0xdfffffff, 0xff, 0xfff},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0x56ec857271eec139ull, .ps_source = 0x5373d99df1779004ull,
        .vertex = {.regs = 9, .pretransformed = 1, .quad = 1, .fixed = {.light = 0x905, .tex = {0x2000, 0x2001, 0x2002, 0x2003}}},
        .pixel = {.ff = {{2, 0, 2, 1, 2, 0, 0, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}, {}}, .quad = 1, .alphatest = 1},
        .key_size = 264,
        .key = {
            0, 0, 0x71eec139, 0x56ec8572, 0xf1779004, 0x5373d99d, 9, 2, 0, 1, 1, 2,
            0, 0, 0x100002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x20, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            7, 1, 0, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x100, 0x40000, 0x40000008},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0xe52d4077e2733cb7ull, .ps_source = 0xb3b881b2aa8634a2ull,
        .vertex = {.program = 0xa0cbcf00aa8ae0c3, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xe2733cb7, 0xe52d4077, 0xaa8634a2, 0xb3b881b2, 0x38c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0,
            0, 0, 0, 1, 0x50, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {1, 2, 1, 0x20, 0, 0x3f6fd00, 0x20, 0x800},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xc007acfa55e90d2cull, .ps_source = 0x7722ce998710d5e7ull,
        .vertex = {.program = 0xccd3b9a74a22c2cb, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x55e90d2c, 0xc007acfa, 0x8710d5e7, 0x7722ce99, 0xd, 2, 0, 1, 1, 2,
            0, 0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x31a91ff, 0x1f202, 0x7bf, 0x5dff, 0xdc397, 0xdfffffff, 0xff, 0xfff},
        .pixel_program = 0x14c4769017b92a89ull,
        .vs_source = 0x1fd2d5acd50db8dfull, .ps_source = 0x5932d33fe3b52cb7ull,
        .vertex = {.program = 0x1a5489ffb2b5409, .regs = 0x700},
        .pixel = {.programmable = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xd50db8df, 0x1fd2d5ac, 0xe3b52cb7, 0x5932d33f, 0x700, 2, 1, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0x80010, 0x100006, 0x1c0006, 0, 0,
            0, 0, 0, 1, 0x2c, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0x40e00000, 0x2c, 0
        },
    },
    {
        .rooms = {0x430a917f, 0x1f202, 0x20c, 0xdbe, 0x10100, 0x83249249, 0x3d, 0x800},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0x4f499e8f1f8af5d8ull, .ps_source = 0x3501a69ad04724c0ull,
        .vertex = {.program = 0x1b707112de27a197, .decode = 0x4000, .regs = 0xb, .quad = 1},
        .pixel = {.ff = {{5, 0, 2, 1, 5, 2, 0, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .proj = 3, .quad = 1, .alphatest = 1},
        .key_size = 264,
        .key = {
            1, 0, 0x1f8af5d8, 0x4f499e8f, 0xd04724c0, 0x3501a69a, 0xb, 2, 1, 1, 0, 6,
            0xc0057, 0, 0x10002a, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x14, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0x40000008},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0xeca46f0f0f3210e2ull, .ps_source = 0xb3b881b2aa8634a2ull,
        .vertex = {.program = 0xb066871b0b7757c9, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xf3210e2, 0xeca46f0f, 0xaa8634a2, 0xb3b881b2, 0xd, 2, 0, 1, 1, 2,
            0, 0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x30000a4, 0x200, 0x1a, 0x880, 0, 0x40, 0, 0xa},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xf8700d69cbaf7e5bull, .ps_source = 0xb718a4e855630046ull,
        .vertex = {.program = 0xb3b57c5b9a7450af, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xcbaf7e5b, 0xf8700d69, 0x55630046, 0xb718a4e8, 0xd, 2, 0, 1, 1, 2,
            0, 0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0x40e00000, 0x2c, 0
        },
    },
    {
        .rooms = {0x431a117f, 0x7200, 0x1af, 0x49ff, 0x4006, 0x2fd1e99, 0xeb},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb3fe8e9ae9415352ull, .ps_source = 0xe8f1a70f9cdfc4a2ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xe9415352, 0xb3fe8e9a, 0x9cdfc4a2, 0xe8f1a70f, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 0, 0x203, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x31a91d5, 0x1d202, 0x7bf, 0x5971, 0xdc297, 0xdfffffff, 0xff, 0xfff},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0x4f499e8f1f8af5d8ull, .ps_source = 0x3501a69ad04724c0ull,
        .vertex = {.program = 0x1b707112de27a197, .decode = 0x4000, .regs = 0xb, .quad = 1},
        .pixel = {.ff = {{5, 0, 2, 1, 5, 2, 0, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .proj = 3, .quad = 1, .alphatest = 1},
        .key_size = 264,
        .key = {
            1, 0, 0x1f8af5d8, 0x4f499e8f, 0xd04724c0, 0x3501a69a, 0xb, 2, 1, 1, 0, 6,
            0xc0057, 0, 0x10002a, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x14, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0, 8},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xa54a8f95e74215fdull, .ps_source = 0x1c1e78d787b5cb15ull,
        .vertex = {.program = 0x1b707112de27a197, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xe74215fd, 0xa54a8f95, 0x87b5cb15, 0x1c1e78d7, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 0, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x1010, 0x8000, 0x208, 0x800, 0x10000, 0x88000222, 0x1c},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x4873aea6164c7573ull, .ps_source = 0x7722ce998710d5e7ull,
        .vertex = {.program = 0x4f9eb762132ae1f1, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x164c7573, 0x4873aea6, 0x8710d5e7, 0x7722ce99, 0x38c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0,
            0, 0, 0, 1, 0x50, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x31a91ff, 0x1f202, 0x7bf, 0x5dff, 0xdc397, 0xdfffffff, 0xff, 0xfff},
        .pixel_program = 0x14c4769017b92a89ull,
        .vs_source = 0xc78f0202244d3b33ull, .ps_source = 0x5932d33fe3b52cb7ull,
        .vertex = {.program = 0xa7f404fc37664173, .regs = 0xf00},
        .pixel = {.programmable = 1},
        .key_size = 264,
        .key = {
            0, 0, 0x244d3b33, 0xc78f0202, 0xe3b52cb7, 0x5932d33f, 0xf00, 2, 0, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 2, 0x100002, 0x200002, 0x300002, 0,
            0, 0, 0, 1, 0x40, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0x40e00000, 0x2c, 0
        },
    },
    {
        .rooms = {0x431a91ff, 0x1f202, 0x7bf, 0x5dff, 0xdc397, 0xdfffffff, 0xff, 0xfff},
        .pixel_program = 0xd00d9b6109932b3dull,
        .vs_source = 0x84e1561bd15d9882ull, .ps_source = 0xbbe056cc97b89d6cull,
        .vertex = {.regs = 0x1e01, .pretransformed = 1, .quad = 1, .fixed = {.light = 0x907, .tex = {0x2000, 0x2001, 0x2002, 0x2003}}},
        .pixel = {.programmable = 1, .tex_modes = 0x8421, .tex = {{.kind = 1, .rect = 1}, {.kind = 1, .rect = 1}, {.kind = 1, .rect = 1}, {.kind = 1, .rect = 1}}, .quad = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xd15d9882, 0x84e1561b, 0x97b89d6c, 0xbbe056cc, 0x1e01, 2, 1, 1, 0, 2,
            0, 0, 0, 0, 0, 0, 0, 0, 0x100010, 0x180010, 0x200010, 0x280010,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x10},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x4873aea6164c7573ull, .ps_source = 0x7722ce998710d5e7ull,
        .vertex = {.program = 0x4f9eb762132ae1f1, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x164c7573, 0x4873aea6, 0x8710d5e7, 0x7722ce99, 0x38c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0,
            0, 0, 0, 1, 0x50, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0x11000, 0, 0, 0, 0x400},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb3fe8e9ae9415352ull, .ps_source = 0x1c1e78d787b5cb15ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xe9415352, 0xb3fe8e9a, 0x87b5cb15, 0x1c1e78d7, 0xb, 2, 0, 1, 0, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x1008008, 0x10000, 0x100, 0x579, 0x10, 0x214480, 0x27, 0x57e},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xa54a8f95e74215fdull, .ps_source = 0x1c1e78d787b5cb15ull,
        .vertex = {.program = 0x1b707112de27a197, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xe74215fd, 0xa54a8f95, 0x87b5cb15, 0x1c1e78d7, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0, 8},
        .pixel_program = 0x371532d23dfd51e5ull,
        .vs_source = 0xe7ea3ca1cfa873eeull, .ps_source = 0x97bec5a5d67ecd76ull,
        .vertex = {.program = 0x3c917fd1f5c3b869, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}, .alphakill = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xcfa873ee, 0xe7ea3ca1, 0xd67ecd76, 0x97bec5a5, 0xd, 2, 0, 1, 1, 2,
            0, 0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0x1509, 0x40283, 0x40090008, 0, 0x10},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0x518af26744051928ull, .ps_source = 0xb3b881b2aa8634a2ull,
        .vertex = {.program = 0xbc5ae0492f0fa99f, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x44051928, 0x518af267, 0xaa8634a2, 0xb3b881b2, 0xf8c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0,
            0, 0, 0, 1, 0x70, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {1},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0x3e0fa33a9c00375bull, .ps_source = 0xbef5d053c0d66a9eull,
        .vertex = {.regs = 0x209, .pretransformed = 1, .quad = 1, .fixed = {.light = 0x905, .tex = {0x4000, 0x2001, 0x2002, 0x2003}}},
        .pixel = {.ff = {{4, 0, 2, 1, 4, 2, 0, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .quad = 1},
        .key_size = 264,
        .key = {
            1, 0, 0x9c00375b, 0x3e0fa33a, 0xc0d66a9e, 0xbef5d053, 0x209, 2, 0, 1, 1, 2,
            0, 0, 0x100002, 0, 0, 0, 0, 0, 0x200002, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 4},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0xe123887755195ef7ull, .ps_source = 0x3a5ef96fb6ebb554ull,
        .vertex = {.program = 0x8ab3fe497032abb9, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x55195ef7, 0xe1238877, 0xb6ebb554, 0x3a5ef96f, 0xf8c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0,
            0, 0, 0, 1, 0x70, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x10a0104, 0x10000, 0x10, 0x10, 0x100},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0x7dacbc78ccc44855ull, .ps_source = 0x3a5ef96fb6ebb554ull,
        .vertex = {.program = 0xebe0a2b6413609b9, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xccc44855, 0x7dacbc78, 0xb6ebb554, 0x3a5ef96f, 0x78c, 2, 1, 1, 1, 0,
            0, 0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0,
            0, 0, 0, 1, 0x2c, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x431a91ff, 0x1f202, 0x7bf, 0x5dff, 0xdc397, 0xdfffffff, 0xfe, 0xfff},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb3fe8e9ae9415352ull, .ps_source = 0xe8f1a70f9cdfc4a2ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xe9415352, 0xb3fe8e9a, 0x9cdfc4a2, 0xe8f1a70f, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x101010, 0, 0, 0, 0, 0x100, 0x60},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb3fe8e9ae9415352ull, .ps_source = 0xe8f1a70f9cdfc4a2ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xe9415352, 0xb3fe8e9a, 0x9cdfc4a2, 0xe8f1a70f, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 1, 0x800b,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x431a91ff, 0x1f202, 0x7bf, 0x5dff, 0xdc397, 0xdfffffff, 0xff, 0xfff},
        .pixel_program = 0xd00d9b6109932b3dull,
        .vs_source = 0x84e1561bd15d9882ull, .ps_source = 0xbbe056cc97b89d6cull,
        .vertex = {.regs = 0x1e01, .pretransformed = 1, .quad = 1, .fixed = {.light = 0x907, .tex = {0x2000, 0x2001, 0x2002, 0x2003}}},
        .pixel = {.programmable = 1, .tex_modes = 0x8421, .tex = {{.kind = 1, .rect = 1}, {.kind = 1, .rect = 1}, {.kind = 1, .rect = 1}, {.kind = 1, .rect = 1}}, .quad = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xd15d9882, 0x84e1561b, 0x97b89d6c, 0xbbe056cc, 0x1e01, 2, 1, 1, 0, 2,
            0, 0, 0, 0, 0, 0, 0, 0, 0x100010, 0x180010, 0x200010, 0x280010,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0x8006,
            0xf, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x30a90a2, 0x10000, 0x41a, 0x8c4, 0x100, 3, 0, 0xa},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0xe8ddae10bfeb5d16ull, .ps_source = 0x3a5ef96fb6ebb554ull,
        .vertex = {.program = 0x777f3497aa0ce6e7, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xbfeb5d16, 0xe8ddae10, 0xb6ebb554, 0x3a5ef96f, 0x78c, 2, 1, 1, 1, 0,
            0, 0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0,
            0, 0, 0, 1, 0x2c, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x100},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0xd05561d73f8003ebull, .ps_source = 0x3a5ef96fb6ebb554ull,
        .vertex = {.program = 0x3719ba15bc9c76c7, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x3f8003eb, 0xd05561d7, 0xb6ebb554, 0x3a5ef96f, 0x38c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0,
            0, 0, 0, 1, 0x50, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x8000},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xd79a92d08b9df366ull, .ps_source = 0x7722ce998710d5e7ull,
        .vertex = {.program = 0x22ca82cb24abc905, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x8b9df366, 0xd79a92d0, 0x8710d5e7, 0x7722ce99, 0xf8c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0,
            0, 0, 0, 1, 0x70, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x31a91f2, 0x1b200, 0x61a, 0xddc, 0x50100, 0xc800027f, 0x1c, 0xa},
        .pixel_program = 0xe5604726dbfa4855ull,
        .vs_source = 0xb8a30eac312ca5eaull, .ps_source = 0x3a0ce0d69658dfc2ull,
        .vertex = {.program = 0x501d667e282e8fb3, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 0x45, .tex = {{.kind = 1}, {.kind = 1, .rect = 1, .shadow = 1}, {}, {}}, .alphatest = 1},
        .key_size = 264,
        .key = {
            0, 0, 0x312ca5ea, 0xb8a30eac, 0x9658dfc2, 0x3a0ce0d6, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0x301, 0x8006,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0, 8},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x4838d2769945e5cfull, .ps_source = 0xb718a4e855630046ull,
        .vertex = {.program = 0x697df3438a77f65f, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x9945e5cf, 0x4838d276, 0x55630046, 0xb718a4e8, 0xf8c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0,
            0, 0, 0, 1, 0x70, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 4},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0xc8e5cf44f1d2e2f5ull, .ps_source = 0x3a5ef96fb6ebb554ull,
        .vertex = {.program = 0xad3f9bf87748856d, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xf1d2e2f5, 0xc8e5cf44, 0xb6ebb554, 0x3a5ef96f, 0x38c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0,
            0, 0, 0, 1, 0x50, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0, 8},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xce4cb173459caeb1ull, .ps_source = 0xb718a4e855630046ull,
        .vertex = {.program = 0xbcd19491676d91cb, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x459caeb1, 0xce4cb173, 0x55630046, 0xb718a4e8, 0x78c, 2, 1, 1, 1, 0,
            0, 0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0,
            0, 0, 0, 1, 0x2c, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x800},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0x4f499e8f1f8af5d8ull, .ps_source = 0x3501a69ad04724c0ull,
        .vertex = {.program = 0x1b707112de27a197, .decode = 0x4000, .regs = 0xb, .quad = 1},
        .pixel = {.ff = {{5, 0, 2, 1, 5, 2, 0, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .proj = 3, .quad = 1, .alphatest = 1},
        .key_size = 264,
        .key = {
            1, 0, 0x1f8af5d8, 0x4f499e8f, 0xd04724c0, 0x3501a69a, 0xb, 2, 1, 1, 1, 6,
            0xc0057, 0, 0x10002a, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x14, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 0, 0, 0, 1, 0x205, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e00, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0x80},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xf8700d69cbaf7e5bull, .ps_source = 0xc1a97d13dccb2ccfull,
        .vertex = {.program = 0xb3b57c5b9a7450af, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}, .alphakill = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xcbaf7e5b, 0xf8700d69, 0xdccb2ccf, 0xc1a97d13, 0xd, 2, 0, 1, 1, 2,
            0, 0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x431a91ef, 0x17202, 0x7bf, 0x4dff, 0xd8397, 0x4bffff7f, 0xbf, 0xc2f},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0x3e41aba8126adbafull, .ps_source = 0x59d62613374ecaf5ull,
        .vertex = {.program = 0x1b707112de27a197, .decode = 0x4000, .regs = 0xb},
        .pixel = {.ff = {{5, 2, 0, 1, 5, 2, 0, 1, 1}, {2, 1, 1, 1, 1, 2, 1, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1, .proj = 7, .alphatest = 1},
        .key_size = 264,
        .key = {
            0, 0, 0x126adbaf, 0x3e41aba8, 0x374ecaf5, 0x59d62613, 0xb, 2, 0, 1, 1, 6,
            0xc0057, 0, 0x10002a, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x14, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 1, 0x8006,
            0xf, 1, 0, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x431a91ff, 0x1f202, 0x7bf, 0x5dff, 0xdc397, 0xdfffffff, 0xff, 0x1ff},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0x4f499e8f1f8af5d8ull, .ps_source = 0x3501a69ad04724c0ull,
        .vertex = {.program = 0x1b707112de27a197, .decode = 0x4000, .regs = 0xb, .quad = 1},
        .pixel = {.ff = {{5, 0, 2, 1, 5, 2, 0, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .proj = 3, .quad = 1, .alphatest = 1},
        .key_size = 264,
        .key = {
            1, 0, 0x1f8af5d8, 0x4f499e8f, 0xd04724c0, 0x3501a69a, 0xb, 2, 1, 1, 1, 6,
            0xc0057, 0, 0x10002a, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x14, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 0, 0, 0, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 4},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xf8700d69cbaf7e5bull, .ps_source = 0xb718a4e855630046ull,
        .vertex = {.program = 0xb3b57c5b9a7450af, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xcbaf7e5b, 0xf8700d69, 0x55630046, 0xb718a4e8, 0xd, 2, 0, 1, 1, 2,
            0, 0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0, 8},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x4873aea6164c7573ull, .ps_source = 0xb718a4e855630046ull,
        .vertex = {.program = 0x4f9eb762132ae1f1, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x164c7573, 0x4873aea6, 0x55630046, 0xb718a4e8, 0x38c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0,
            0, 0, 0, 1, 0x50, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x20},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0x4f499e8f1f8af5d8ull, .ps_source = 0x3501a69ad04724c0ull,
        .vertex = {.program = 0x1b707112de27a197, .decode = 0x4000, .regs = 0xb, .quad = 1},
        .pixel = {.ff = {{5, 0, 2, 1, 5, 2, 0, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .proj = 3, .quad = 1, .alphatest = 1},
        .key_size = 264,
        .key = {
            1, 0, 0x1f8af5d8, 0x4f499e8f, 0xd04724c0, 0x3501a69a, 0xb, 2, 0, 1, 1, 6,
            0xc0057, 0, 0x10002a, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x14, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 0, 0, 0, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x10},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xce4cb173459caeb1ull, .ps_source = 0x7722ce998710d5e7ull,
        .vertex = {.program = 0xbcd19491676d91cb, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x459caeb1, 0xce4cb173, 0x8710d5e7, 0x7722ce99, 0x78c, 2, 1, 1, 1, 0,
            0, 0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0,
            0, 0, 0, 1, 0x2c, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x400001a2, 0x1d202, 0x280, 0x404, 0, 0x89f04c00, 0x93, 0x40a},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb3fe8e9ae9415352ull, .ps_source = 0x1c1e78d787b5cb15ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xe9415352, 0xb3fe8e9a, 0x87b5cb15, 0x1c1e78d7, 0xb, 2, 0, 1, 0, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 1, 0x8006,
            0xf, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x31a91ff, 0x1f202, 0x7bf, 0x5dff, 0xdc397, 0xdfffffff, 0xff, 0xfff},
        .pixel_program = 0x14c4769017b92a89ull,
        .vs_source = 0x0c7189a423c190f5ull, .ps_source = 0x5932d33fe3b52cb7ull,
        .vertex = {.program = 0x8d4f80ddb4f99031, .regs = 0x200},
        .pixel = {.programmable = 1},
        .key_size = 264,
        .key = {
            0, 0, 0x23c190f5, 0xc7189a4, 0xe3b52cb7, 0x5932d33f, 0x200, 2, 0, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0,
            0, 0, 0, 1, 0x10, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0x40e00000, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0, 4, 0x140},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb3fe8e9ae9415352ull, .ps_source = 0xe8f1a70f9cdfc4a2ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xe9415352, 0xb3fe8e9a, 0x9cdfc4a2, 0xe8f1a70f, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 1, 0x8006,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x400, 0, 0, 0, 8},
        .pixel_program = 0xc1ad79eb34696ff9ull,
        .vs_source = 0x999a2624ff5b9551ull, .ps_source = 0xe2acfb46144329e8ull,
        .vertex = {.program = 0x47aaf22d90dc2f39, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 0x11044, .tex = {{.kind = 1}, {.kind = 1, .rect = 1, .shadow = 1}, {}, {.kind = 1, .rect = 1, .shadow = 1}}, .alphatest = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xff5b9551, 0x999a2624, 0x144329e8, 0xe2acfb46, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0x301, 0x8006,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x431a91ff, 0x1f202, 0x7bf, 0x5dff, 0xdc397, 0xdfffffff, 0xff, 0x9ff},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0x4f499e8f1f8af5d8ull, .ps_source = 0x3501a69ad04724c0ull,
        .vertex = {.program = 0x1b707112de27a197, .decode = 0x4000, .regs = 0xb, .quad = 1},
        .pixel = {.ff = {{5, 0, 2, 1, 5, 2, 0, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .proj = 3, .quad = 1, .alphatest = 1},
        .key_size = 264,
        .key = {
            1, 0, 0x1f8af5d8, 0x4f499e8f, 0xd04724c0, 0x3501a69a, 0xb, 2, 0, 1, 0, 6,
            0xc0057, 0, 0x10002a, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x14, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x31a91ff, 0x1f202, 0x7bf, 0x5dff, 0xdc397, 0xdfffffff, 0xff, 0xfff},
        .pixel_program = 0x08dcef845cc8aa5dull,
        .vs_source = 0xc0b33c4ae67c6810ull, .ps_source = 0x4c3d2d14ac491e02ull,
        .vertex = {.program = 0x6b2a205984c073cd, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 0x1041, .tex = {{.kind = 1}, {.kind = 1, .rect = 1, .shadow = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xe67c6810, 0xc0b33c4a, 0xac491e02, 0x4c3d2d14, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0x200, 0x2c, 0x100b, 0x10200, 0x8000000, 0, 0x20},
        .pixel_program = 0x371532d23dfd51e5ull,
        .vs_source = 0xe56dd9f517c7e0daull, .ps_source = 0x976f25b7e652dc1eull,
        .vertex = {.program = 0x98230210e4906433, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x17c7e0da, 0xe56dd9f5, 0xe652dc1e, 0x976f25b7, 0xd, 2, 0, 1, 1, 2,
            0, 0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x80100},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0xe123887755195ef7ull, .ps_source = 0x3a5ef96fb6ebb554ull,
        .vertex = {.program = 0x8ab3fe497032abb9, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x55195ef7, 0xe1238877, 0xb6ebb554, 0x3a5ef96f, 0xf8c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0,
            0, 0, 0, 1, 0x70, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0xb, 0x40085, 8, 0, 1},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0xfe9f3974779ccdc5ull, .ps_source = 0xb3b881b2aa8634a2ull,
        .vertex = {.program = 0x94e5de1ac8188969, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x779ccdc5, 0xfe9f3974, 0xaa8634a2, 0xb3b881b2, 0xf8c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0,
            0, 0, 0, 1, 0x70, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0x150b, 0x40287, 0x40090008, 0, 0x11},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0xeca46f0f0f3210e2ull, .ps_source = 0xb3b881b2aa8634a2ull,
        .vertex = {.program = 0xb066871b0b7757c9, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xf3210e2, 0xeca46f0f, 0xaa8634a2, 0xb3b881b2, 0xd, 2, 0, 1, 1, 2,
            0, 0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x800, 1, 0x40000000, 2},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xd1fcb1daf551e1a2ull, .ps_source = 0x1c1e78d787b5cb15ull,
        .vertex = {.program = 0x9a7598448c4d1233, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xf551e1a2, 0xd1fcb1da, 0x87b5cb15, 0x1c1e78d7, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x1008111, 0x200, 0x524, 0, 0, 0x10100164},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xa0572b8233b20652ull, .ps_source = 0x7722ce998710d5e7ull,
        .vertex = {.program = 0x172f719c73ce6401, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x33b20652, 0xa0572b82, 0x8710d5e7, 0x7722ce99, 0x38c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0,
            0, 0, 0, 1, 0x50, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0x508, 0, 0, 0, 1},
        .pixel_program = 0x5d2b1cfd399ca389ull,
        .vs_source = 0xcecd18f4bfe04a4eull, .ps_source = 0x547f8d0ec7ca7e84ull,
        .vertex = {.program = 0x2df7f99d6e7ba7, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x401, .tex = {{.kind = 1}, {}, {.kind = 1}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xbfe04a4e, 0xcecd18f4, 0xc7ca7e84, 0x547f8d0e, 0x78c, 2, 1, 1, 1, 0,
            0, 0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0,
            0, 0, 0, 1, 0x2c, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x10a8040, 0x1200, 0x405, 0x1d7b, 0x14000, 0x890010d, 0x98, 0xa44},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb3fe8e9ae9415352ull, .ps_source = 0xe8f1a70f9cdfc4a2ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xe9415352, 0xb3fe8e9a, 0x9cdfc4a2, 0xe8f1a70f, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 1, 0x8006,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x8000, 0x10000, 8},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x6f4f2657cd668fa9ull, .ps_source = 0x7722ce998710d5e7ull,
        .vertex = {.program = 0x5f10fffdb91e8bfd, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xcd668fa9, 0x6f4f2657, 0x8710d5e7, 0x7722ce99, 0xf8c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0,
            0, 0, 0, 1, 0x70, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0, 2},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xc23c9367f0401226ull, .ps_source = 0xe8f1a70f9cdfc4a2ull,
        .vertex = {.program = 0x7ac2d148e0075b3b, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xf0401226, 0xc23c9367, 0x9cdfc4a2, 0xe8f1a70f, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 1, 0x8006,
            0xf, 1, 0, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x800, 1, 0x40000000, 2},
        .pixel_program = 0x08dcef845cc8aa5dull,
        .vs_source = 0x28542b391580407dull, .ps_source = 0x4c3d2d14ac491e02ull,
        .vertex = {.program = 0xc627357a3baea81b, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 0x1041, .tex = {{.kind = 1}, {.kind = 1, .rect = 1, .shadow = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x1580407d, 0x28542b39, 0xac491e02, 0x4c3d2d14, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x80100},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0xc8e5cf44f1d2e2f5ull, .ps_source = 0x3a5ef96fb6ebb554ull,
        .vertex = {.program = 0xad3f9bf87748856d, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xf1d2e2f5, 0xc8e5cf44, 0xb6ebb554, 0x3a5ef96f, 0x38c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0,
            0, 0, 0, 1, 0x50, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 8},
        .pixel_program = 0x5d2b1cfd399ca389ull,
        .vs_source = 0x68348c4c6c3c556full, .ps_source = 0x547f8d0ec7ca7e84ull,
        .vertex = {.program = 0x89bf9deced319ae1, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x401, .tex = {{.kind = 1}, {}, {.kind = 1}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x6c3c556f, 0x68348c4c, 0xc7ca7e84, 0x547f8d0e, 0xf8c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0,
            0, 0, 0, 1, 0x70, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x30a91f2, 0x18200, 0x61a, 0xdc4, 0x100, 0xc0000267, 0x19, 0xa},
        .pixel_program = 0xc1ad79eb34696ff9ull,
        .vs_source = 0x999a2624ff5b9551ull, .ps_source = 0xe2acfb46144329e8ull,
        .vertex = {.program = 0x47aaf22d90dc2f39, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 0x11044, .tex = {{.kind = 1}, {.kind = 1, .rect = 1, .shadow = 1}, {}, {.kind = 1, .rect = 1, .shadow = 1}}, .alphatest = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xff5b9551, 0x999a2624, 0x144329e8, 0xe2acfb46, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0x301, 0x8006,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0x500, 0x80, 0, 0, 1},
        .pixel_program = 0x5d2b1cfd399ca389ull,
        .vs_source = 0x6b5adef7ad019009ull, .ps_source = 0x547f8d0ec7ca7e84ull,
        .vertex = {.program = 0x5bc4ef08d3a43ac7, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x401, .tex = {{.kind = 1}, {}, {.kind = 1}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xad019009, 0x6b5adef7, 0xc7ca7e84, 0x547f8d0e, 0xf8c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0,
            0, 0, 0, 1, 0x70, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x431a91ff, 0x1f202, 0x7bf, 0x5dff, 0xdc397, 0xdfffffff, 0xff, 0xfff},
        .pixel_program = 0x644eb632e9cfa30full,
        .vs_source = 0x604697e0cc7ab244ull, .ps_source = 0x66cfb16d99ed6bcaull,
        .vertex = {.regs = 0x201, .pretransformed = 1, .quad = 1, .fixed = {.light = 0x907, .tex = {0x2000, 0x2001, 0x2002, 0x2003}}},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1, .rect = 1}, {}, {}, {}}, .quad = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xcc7ab244, 0x604697e0, 0x99ed6bca, 0x66cfb16d, 0x201, 2, 1, 1, 0, 2,
            0, 0, 0, 0, 0, 0, 0, 0, 0x100010, 0, 0, 0,
            0, 0, 0, 1, 0x18, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x8000, 0x10000, 8},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x5f701b2d02ef75ceull, .ps_source = 0x7722ce998710d5e7ull,
        .vertex = {.program = 0x6b228efce02817df, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x2ef75ce, 0x5f701b2d, 0x8710d5e7, 0x7722ce99, 0x78c, 2, 1, 1, 1, 0,
            0, 0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0,
            0, 0, 0, 1, 0x2c, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x8000},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xa0572b8233b20652ull, .ps_source = 0x7722ce998710d5e7ull,
        .vertex = {.program = 0x172f719c73ce6401, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x33b20652, 0xa0572b82, 0x8710d5e7, 0x7722ce99, 0x38c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0,
            0, 0, 0, 1, 0x50, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x180112, 0, 0, 0, 0, 0x200, 4},
        .pixel_program = 0x14c4769017b92a89ull,
        .vs_source = 0x1fd2d5acd50db8dfull, .ps_source = 0x5932d33fe3b52cb7ull,
        .vertex = {.program = 0x1a5489ffb2b5409, .regs = 0x700},
        .pixel = {.programmable = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xd50db8df, 0x1fd2d5ac, 0xe3b52cb7, 0x5932d33f, 0x700, 2, 1, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0x80010, 0x100006, 0x1c0006, 0, 0,
            0, 0, 0, 1, 0x2c, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0x40e00000, 0x2c, 0
        },
    },
    {
        .rooms = {0x180112, 0, 0, 0, 0, 0x200, 4},
        .pixel_program = 0x14c4769017b92a89ull,
        .vs_source = 0xc78f0202244d3b33ull, .ps_source = 0x5932d33fe3b52cb7ull,
        .vertex = {.program = 0xa7f404fc37664173, .regs = 0xf00},
        .pixel = {.programmable = 1},
        .key_size = 264,
        .key = {
            0, 0, 0x244d3b33, 0xc78f0202, 0xe3b52cb7, 0x5932d33f, 0xf00, 2, 0, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 2, 0x100002, 0x200002, 0x300002, 0,
            0, 0, 0, 1, 0x40, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0x40e00000, 0x2c, 0
        },
    },
    {
        .rooms = {0x30a90a2, 0x10000, 0x41a, 0x8c4, 0x100, 3, 0, 0xa},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0xa7960402b331f12full, .ps_source = 0x3a5ef96fb6ebb554ull,
        .vertex = {.program = 0xb7c239bc7fa988fb, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xb331f12f, 0xa7960402, 0xb6ebb554, 0x3a5ef96f, 0xf8c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0,
            0, 0, 0, 1, 0x70, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x180012, 0, 0, 0, 0, 0x200, 4},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0xa7960402b331f12full, .ps_source = 0x3a5ef96fb6ebb554ull,
        .vertex = {.program = 0xb7c239bc7fa988fb, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xb331f12f, 0xa7960402, 0xb6ebb554, 0x3a5ef96f, 0xf8c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0,
            0, 0, 0, 1, 0x70, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0xa8130, 0x10200, 0x49f, 0x30, 0x8110, 0x108000, 3},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x6f4f2657cd668fa9ull, .ps_source = 0x7722ce998710d5e7ull,
        .vertex = {.program = 0x5f10fffdb91e8bfd, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xcd668fa9, 0x6f4f2657, 0x8710d5e7, 0x7722ce99, 0xf8c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0,
            0, 0, 0, 1, 0x70, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0x10000, 0, 0x400, 0, 0x480, 0, 0x77c},
        .pixel_program = 0x08dcef845cc8aa5dull,
        .vs_source = 0xc0b33c4ae67c6810ull, .ps_source = 0x4c3d2d14ac491e02ull,
        .vertex = {.program = 0x6b2a205984c073cd, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 0x1041, .tex = {{.kind = 1}, {.kind = 1, .rect = 1, .shadow = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xe67c6810, 0xc0b33c4a, 0xac491e02, 0x4c3d2d14, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x180112, 0, 0, 0, 0, 0x200, 4},
        .pixel_program = 0x14c4769017b92a89ull,
        .vs_source = 0x0c7189a423c190f5ull, .ps_source = 0x5932d33fe3b52cb7ull,
        .vertex = {.program = 0x8d4f80ddb4f99031, .regs = 0x200},
        .pixel = {.programmable = 1},
        .key_size = 264,
        .key = {
            0, 0, 0x23c190f5, 0xc7189a4, 0xe3b52cb7, 0x5932d33f, 0x200, 2, 0, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0,
            0, 0, 0, 1, 0x10, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0x40e00000, 0x2c, 0
        },
    },
    {
        .rooms = {0xa8130, 0x10200, 0x49f, 0x30, 0x8110, 0x108000, 3},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xc6d37fbb13c0a1f2ull, .ps_source = 0x7722ce998710d5e7ull,
        .vertex = {.program = 0xcc0a3244080130e1, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x13c0a1f2, 0xc6d37fbb, 0x8710d5e7, 0x7722ce99, 0x38c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0,
            0, 0, 0, 1, 0x50, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x302908c, 0x10002, 0, 0xd80, 0x10010, 0x80080, 0xd, 0x75c},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb3fe8e9ae9415352ull, .ps_source = 0xe8f1a70f9cdfc4a2ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xe9415352, 0xb3fe8e9a, 0x9cdfc4a2, 0xe8f1a70f, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0x80},
        .pixel_program = 0x5d2b1cfd399ca389ull,
        .vs_source = 0x00bbcadd2abb0cafull, .ps_source = 0x547f8d0ec7ca7e84ull,
        .vertex = {.program = 0x5d04d37883312711, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x401, .tex = {{.kind = 1}, {}, {.kind = 1}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x2abb0caf, 0xbbcadd, 0xc7ca7e84, 0x547f8d0e, 0x78c, 2, 1, 1, 1, 0,
            0, 0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0,
            0, 0, 0, 1, 0x2c, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x31a91bf, 0x1f202, 0x7bf, 0x48f4, 0x9c110, 0x9ff6ff77, 0xff, 0xfee},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xf8700d69cbaf7e5bull, .ps_source = 0x7722ce998710d5e7ull,
        .vertex = {.program = 0xb3b57c5b9a7450af, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xcbaf7e5b, 0xf8700d69, 0x8710d5e7, 0x7722ce99, 0xd, 2, 0, 1, 1, 2,
            0, 0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x100},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0xde4eea3e159f88bdull, .ps_source = 0x3a5ef96fb6ebb554ull,
        .vertex = {.program = 0x4390a90fe19250ff, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x159f88bd, 0xde4eea3e, 0xb6ebb554, 0x3a5ef96f, 0xf8c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0,
            0, 0, 0, 1, 0x70, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0xa, 0x40080},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0xa2e1c8a48067c0a6ull, .ps_source = 0xb3b881b2aa8634a2ull,
        .vertex = {.program = 0x3c3b3b2cb13ccbf9, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x8067c0a6, 0xa2e1c8a4, 0xaa8634a2, 0xb3b881b2, 0xf8c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0,
            0, 0, 0, 1, 0x70, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0x80},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x4873aea6164c7573ull, .ps_source = 0xc1a97d13dccb2ccfull,
        .vertex = {.program = 0x4f9eb762132ae1f1, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}, .alphakill = 1},
        .key_size = 264,
        .key = {
            0, 0, 0x164c7573, 0x4873aea6, 0xdccb2ccf, 0xc1a97d13, 0x38c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0,
            0, 0, 0, 1, 0x50, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0x150b, 0x40287, 0x40090008, 0, 0x11},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0xeca46f0f0f3210e2ull, .ps_source = 0x22dd4cb5741d5671ull,
        .vertex = {.program = 0xb066871b0b7757c9, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xf3210e2, 0xeca46f0f, 0x741d5671, 0x22dd4cb5, 0xd, 2, 0, 1, 1, 2,
            0, 0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0x40e00000, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x100, 0x40000, 0x40000008},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0x948bf844aafadd08ull, .ps_source = 0xb3b881b2aa8634a2ull,
        .vertex = {.program = 0x512c33a8e938e3fb, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xaafadd08, 0x948bf844, 0xaa8634a2, 0xb3b881b2, 0x78c, 2, 1, 1, 1, 0,
            0, 0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0,
            0, 0, 0, 1, 0x2c, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 8},
        .pixel_program = 0x5d2b1cfd399ca389ull,
        .vs_source = 0x9d17f0824542ff30ull, .ps_source = 0x547f8d0ec7ca7e84ull,
        .vertex = {.program = 0xa9bcb3a1ab8eb269, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x401, .tex = {{.kind = 1}, {}, {.kind = 1}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x4542ff30, 0x9d17f082, 0xc7ca7e84, 0x547f8d0e, 0x38c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0,
            0, 0, 0, 1, 0x50, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x100},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0x783893e121d68abcull, .ps_source = 0x3a5ef96fb6ebb554ull,
        .vertex = {.program = 0xfa08fa55ec14ec6b, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x21d68abc, 0x783893e1, 0xb6ebb554, 0x3a5ef96f, 0x78c, 2, 1, 1, 1, 0,
            0, 0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0,
            0, 0, 0, 1, 0x2c, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0x200},
        .pixel_program = 0xc1ad79eb34696ff9ull,
        .vs_source = 0x999a2624ff5b9551ull, .ps_source = 0x9d99749c8a8feb2eull,
        .vertex = {.program = 0x47aaf22d90dc2f39, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 0x1044, .tex = {{.kind = 1}, {.kind = 1, .rect = 1, .shadow = 1}, {}, {}}, .alphatest = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xff5b9551, 0x999a2624, 0x8a8feb2e, 0x9d99749c, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0x301, 0x8006,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x30a9155, 0x10202, 0x204, 0x830, 0x50200, 0x81249249, 0x18, 0x800},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0x4f499e8f1f8af5d8ull, .ps_source = 0x3501a69ad04724c0ull,
        .vertex = {.program = 0x1b707112de27a197, .decode = 0x4000, .regs = 0xb, .quad = 1},
        .pixel = {.ff = {{5, 0, 2, 1, 5, 2, 0, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .proj = 3, .quad = 1, .alphatest = 1},
        .key_size = 264,
        .key = {
            1, 0, 0x1f8af5d8, 0x4f499e8f, 0xd04724c0, 0x3501a69a, 0xb, 2, 0, 1, 0, 6,
            0xc0057, 0, 0x10002a, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x14, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 4},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xc007acfa55e90d2cull, .ps_source = 0x7722ce998710d5e7ull,
        .vertex = {.program = 0xccd3b9a74a22c2cb, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x55e90d2c, 0xc007acfa, 0x8710d5e7, 0x7722ce99, 0xd, 2, 0, 1, 1, 2,
            0, 0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x8100, 0, 0x29, 0x13f, 0x10010, 0xe000009, 0x18, 1},
        .pixel_program = 0x371532d23dfd51e5ull,
        .vs_source = 0xe7ea3ca1cfa873eeull, .ps_source = 0x976f25b7e652dc1eull,
        .vertex = {.program = 0x3c917fd1f5c3b869, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xcfa873ee, 0xe7ea3ca1, 0xe652dc1e, 0x976f25b7, 0xd, 2, 0, 1, 1, 2,
            0, 0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x1008111, 0x200, 0x524, 0, 0, 0x10100164},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xd79a92d08b9df366ull, .ps_source = 0x7722ce998710d5e7ull,
        .vertex = {.program = 0x22ca82cb24abc905, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x8b9df366, 0xd79a92d0, 0x8710d5e7, 0x7722ce99, 0xf8c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0,
            0, 0, 0, 1, 0x70, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x420a01ae, 0x1f202, 0x680, 0x5da5, 0x40297, 0xc9fedc5b, 0xff, 0x5db},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb3fe8e9ae9415352ull, .ps_source = 0x1c1e78d787b5cb15ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xe9415352, 0xb3fe8e9a, 0x87b5cb15, 0x1c1e78d7, 0xb, 2, 0, 1, 0, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0xa, 0x40080},
        .pixel_program = 0x6b7d629b1f945515ull,
        .vs_source = 0x2bdb50c4d4df2872ull, .ps_source = 0xb3b881b2aa8634a2ull,
        .vertex = {.program = 0x1cb8524e559ff831, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x421, .tex = {{.kind = 1}, {.kind = 1}, {.kind = 1}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xd4df2872, 0x2bdb50c4, 0xaa8634a2, 0xb3b881b2, 0x78c, 2, 1, 1, 1, 0,
            0, 0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0,
            0, 0, 0, 1, 0x2c, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0, 0x40},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb3fe8e9ae9415352ull, .ps_source = 0xe8f1a70f9cdfc4a2ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xe9415352, 0xb3fe8e9a, 0x9cdfc4a2, 0xe8f1a70f, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 1, 0x800b,
            0xf, 1, 0, 0x203, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x10a0104, 0x10000, 0x10, 0x10, 0x100},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0xe123887755195ef7ull, .ps_source = 0x3a5ef96fb6ebb554ull,
        .vertex = {.program = 0x8ab3fe497032abb9, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x55195ef7, 0xe1238877, 0xb6ebb554, 0x3a5ef96f, 0xf8c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0,
            0, 0, 0, 1, 0x70, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x1000000, 0x200, 0, 0, 0, 0x44},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0xd05561d73f8003ebull, .ps_source = 0x3a5ef96fb6ebb554ull,
        .vertex = {.program = 0x3719ba15bc9c76c7, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x3f8003eb, 0xd05561d7, 0xb6ebb554, 0x3a5ef96f, 0x38c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0,
            0, 0, 0, 1, 0x50, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x21a90be, 0x1f202, 0x29a, 0x48d4, 0x94100, 0x8fe6fe33, 0xfe, 0xfee},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xce4cb173459caeb1ull, .ps_source = 0x7722ce998710d5e7ull,
        .vertex = {.program = 0xbcd19491676d91cb, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x459caeb1, 0xce4cb173, 0x8710d5e7, 0x7722ce99, 0x78c, 2, 1, 1, 1, 0,
            0, 0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0,
            0, 0, 0, 1, 0x2c, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0, 2},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xd1fcb1daf551e1a2ull, .ps_source = 0x1c1e78d787b5cb15ull,
        .vertex = {.program = 0x9a7598448c4d1233, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xf551e1a2, 0xd1fcb1da, 0x87b5cb15, 0x1c1e78d7, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x8000, 0x10000, 8},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xc6d37fbb13c0a1f2ull, .ps_source = 0x7722ce998710d5e7ull,
        .vertex = {.program = 0xcc0a3244080130e1, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x13c0a1f2, 0xc6d37fbb, 0x8710d5e7, 0x7722ce99, 0x38c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0,
            0, 0, 0, 1, 0x50, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x800},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xc23c9367f0401226ull, .ps_source = 0xe8f1a70f9cdfc4a2ull,
        .vertex = {.program = 0x7ac2d148e0075b3b, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xf0401226, 0xc23c9367, 0x9cdfc4a2, 0xe8f1a70f, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0x80},
        .pixel_program = 0x5d2b1cfd399ca389ull,
        .vs_source = 0xfe77600aacaabd12ull, .ps_source = 0x547f8d0ec7ca7e84ull,
        .vertex = {.program = 0x65557b3077fe6af9, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x401, .tex = {{.kind = 1}, {}, {.kind = 1}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xacaabd12, 0xfe77600a, 0xc7ca7e84, 0x547f8d0e, 0x38c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0,
            0, 0, 0, 1, 0x50, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x80100},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0x7dacbc78ccc44855ull, .ps_source = 0x3a5ef96fb6ebb554ull,
        .vertex = {.program = 0xebe0a2b6413609b9, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xccc44855, 0x7dacbc78, 0xb6ebb554, 0x3a5ef96f, 0x78c, 2, 1, 1, 1, 0,
            0, 0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0,
            0, 0, 0, 1, 0x2c, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x1000000, 0, 0, 0, 0, 0x44},
        .pixel_program = 0xd9dc5d5e42409389ull,
        .vs_source = 0x783893e121d68abcull, .ps_source = 0x3a5ef96fb6ebb554ull,
        .vertex = {.program = 0xfa08fa55ec14ec6b, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x21d68abc, 0x783893e1, 0xb6ebb554, 0x3a5ef96f, 0x78c, 2, 1, 1, 1, 0,
            0, 0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0,
            0, 0, 0, 1, 0x2c, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0, 0, 4},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xa54a8f95e74215fdull, .ps_source = 0x7e539794c0f180cbull,
        .vertex = {.program = 0x1b707112de27a197, .regs = 0xb},
        .pixel = {.programmable = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xe74215fd, 0xa54a8f95, 0xc0f180cb, 0x7e539794, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x21a91ee, 0x1b000, 0x1f, 0x5c85, 0xdc397, 0x454401, 0x7f},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb3fe8e9ae9415352ull, .ps_source = 0xe8f1a70f9cdfc4a2ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xe9415352, 0xb3fe8e9a, 0x9cdfc4a2, 0xe8f1a70f, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 0, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x1008111, 0x200, 0x524, 0, 0, 0x10100164},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x8953da43c65301a1ull, .ps_source = 0x7722ce998710d5e7ull,
        .vertex = {.program = 0x2a09a1bcafcf5ef3, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xc65301a1, 0x8953da43, 0x8710d5e7, 0x7722ce99, 0x78c, 2, 1, 1, 1, 0,
            0, 0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0,
            0, 0, 0, 1, 0x2c, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x800, 1, 0x40000000, 2},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xc23c9367f0401226ull, .ps_source = 0x1c1e78d787b5cb15ull,
        .vertex = {.program = 0x7ac2d148e0075b3b, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xf0401226, 0xc23c9367, 0x87b5cb15, 0x1c1e78d7, 0xb, 2, 0, 1, 0, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0xa8130, 0x10200, 0x49f, 0x30, 0x8110, 0x108000, 3},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x5f701b2d02ef75ceull, .ps_source = 0x7722ce998710d5e7ull,
        .vertex = {.program = 0x6b228efce02817df, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x2ef75ce, 0x5f701b2d, 0x8710d5e7, 0x7722ce99, 0x78c, 2, 1, 1, 1, 0,
            0, 0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0,
            0, 0, 0, 1, 0x2c, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x8000, 0, 8, 0, 2, 0, 8},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0xa54a8f95e74215fdull, .ps_source = 0xd9bdfcb4c222f59cull,
        .vertex = {.program = 0x1b707112de27a197, .regs = 0xb},
        .pixel = {.ff = {{4, 0, 2, 1, 4, 0, 2, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}, {}}, .tex = {{.kind = 1}, {}, {}, {}}, .proj = 3},
        .key_size = 264,
        .key = {
            0, 0, 0xe74215fd, 0xa54a8f95, 0xc222f59c, 0xd9bdfcb4, 0xb, 2, 1, 1, 1, 6,
            0xc0057, 0, 0x100010, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x18, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 1, 0x8006,
            0xf, 1, 0, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0x80},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0xce4cb173459caeb1ull, .ps_source = 0xc1a97d13dccb2ccfull,
        .vertex = {.program = 0xbcd19491676d91cb, .decode = 0x20004400, .regs = 0x78c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}, .alphakill = 1},
        .key_size = 264,
        .key = {
            0, 0, 0x459caeb1, 0xce4cb173, 0xdccb2ccf, 0xc1a97d13, 0x78c, 2, 1, 1, 1, 0,
            0, 0x4002a, 0x28002a, 0, 0, 0, 0x26, 0x80010, 0x100006, 0x1c0006, 0, 0,
            0, 0, 0, 1, 0x2c, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x431a91ff, 0x1f202, 0x7bf, 0x5dff, 0xdc397, 0xdfffffff, 0xff, 0xdff},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xa54a8f95e74215fdull, .ps_source = 0x1c1e78d787b5cb15ull,
        .vertex = {.program = 0x1b707112de27a197, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xe74215fd, 0xa54a8f95, 0x87b5cb15, 0x1c1e78d7, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x21a90be, 0x1f202, 0x29a, 0x48d4, 0x94100, 0x8fe6fe33, 0xfe, 0xfee},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x4873aea6164c7573ull, .ps_source = 0x7722ce998710d5e7ull,
        .vertex = {.program = 0x4f9eb762132ae1f1, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x164c7573, 0x4873aea6, 0x8710d5e7, 0x7722ce99, 0x38c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0,
            0, 0, 0, 1, 0x50, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {1},
        .pixel_program = 0x0000000000000000ull,
        .vs_source = 0x56ec857271eec139ull, .ps_source = 0x8710aaa7c526e86cull,
        .vertex = {.regs = 9, .pretransformed = 1, .quad = 1, .fixed = {.light = 0x905, .tex = {0x2000, 0x2001, 0x2002, 0x2003}}},
        .pixel = {.ff = {{2, 0, 2, 1, 2, 0, 0, 1, 1}, {1, 2, 1, 1, 1, 2, 1, 1, 1}, {}, {}}, .quad = 1},
        .key_size = 264,
        .key = {
            0, 0, 0x71eec139, 0x56ec8572, 0xc526e86c, 0x8710aaa7, 9, 2, 0, 1, 1, 2,
            0, 0, 0x100002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x20, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8006,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x40100051, 0xc202, 0x84, 0x44bf, 0x40012, 0x8311210, 0x74},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb3fe8e9ae9415352ull, .ps_source = 0xe8f1a70f9cdfc4a2ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xe9415352, 0xb3fe8e9a, 0x9cdfc4a2, 0xe8f1a70f, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 1, 0x8006,
            0xf, 1, 0, 0x203, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0x80},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x4838d2769945e5cfull, .ps_source = 0xc1a97d13dccb2ccfull,
        .vertex = {.program = 0x697df3438a77f65f, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}, .alphakill = 1},
        .key_size = 264,
        .key = {
            0, 0, 0x9945e5cf, 0x4838d276, 0xdccb2ccf, 0xc1a97d13, 0xf8c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0,
            0, 0, 0, 1, 0x70, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x40, 0, 0, 0x500, 0, 0, 0, 1},
        .pixel_program = 0x5d2b1cfd399ca389ull,
        .vs_source = 0x631348fdeafa5ba3ull, .ps_source = 0x547f8d0ec7ca7e84ull,
        .vertex = {.program = 0x6b238c678ed7f83, .regs = 0x38c},
        .pixel = {.programmable = 1, .tex_modes = 0x401, .tex = {{.kind = 1}, {}, {.kind = 1}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0xeafa5ba3, 0x631348fd, 0xc7ca7e84, 0x547f8d0e, 0x38c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0, 0, 0,
            0, 0, 0, 1, 0x50, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x21a90be, 0x1f202, 0x29a, 0x48d4, 0x94100, 0x8fe6fe33, 0xfe, 0xfee},
        .pixel_program = 0xc9097fa5c77d6bf1ull,
        .vs_source = 0x4838d2769945e5cfull, .ps_source = 0x7722ce998710d5e7ull,
        .vertex = {.program = 0x697df3438a77f65f, .regs = 0xf8c},
        .pixel = {.programmable = 1, .tex_modes = 0x21, .tex = {{.kind = 1}, {.kind = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x9945e5cf, 0x4838d276, 0x8710d5e7, 0x7722ce99, 0xf8c, 2, 0, 1, 1, 0,
            0, 2, 0x100002, 0, 0, 0, 0x200002, 0x300002, 0x400002, 0x500002, 0x600002, 0,
            0, 0, 0, 1, 0x70, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 1, 0x207, 0xff, 0xff, 0x1e01, 0x1e01, 0x1e01, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0, 0, 0, 0, 4},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xb3fe8e9ae9415352ull, .ps_source = 0xe8f1a70f9cdfc4a2ull,
        .vertex = {.program = 0xef8bc06bbb05784f, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xe9415352, 0xb3fe8e9a, 0x9cdfc4a2, 0xe8f1a70f, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 1, 0x800b,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x100},
        .pixel_program = 0x707019fe21480fafull,
        .vs_source = 0xffb005e174bf40f3ull, .ps_source = 0x760dc4400125ba7full,
        .vertex = {.program = 0x2dce7401047787ed, .regs = 0xd},
        .pixel = {.programmable = 1, .tex_modes = 0x41, .tex = {{.kind = 1}, {.kind = 1, .rect = 1, .shadow = 1}, {}, {}}},
        .key_size = 264,
        .key = {
            0, 0, 0x74bf40f3, 0xffb005e1, 0x125ba7f, 0x760dc440, 0xd, 2, 0, 1, 1, 2,
            0, 0x100002, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0, 0, 0, 0x800},
        .pixel_program = 0xb570854e5a13ab49ull,
        .vs_source = 0xc23c9367f0401226ull, .ps_source = 0xe8f1a70f9cdfc4a2ull,
        .vertex = {.program = 0x7ac2d148e0075b3b, .regs = 0xb},
        .pixel = {.programmable = 1, .tex_modes = 1, .tex = {{.kind = 1}, {}, {}, {}}, .alphakill = 1},
        .key_size = 264,
        .key = {
            0, 0, 0xf0401226, 0xc23c9367, 0x9cdfc4a2, 0xe8f1a70f, 0xb, 2, 0, 1, 1, 2,
            0x100002, 0, 0x200002, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 0x30, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 1, 0x8006,
            0xf, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 0x1b02, 0, 0, 0x2c, 0
        },
    },
    {
        .rooms = {0x180002},
        .pixel_program = 0x14c4769017b92a89ull,
        .vs_source = 0x0c7189a423c190f5ull, .ps_source = 0x5932d33fe3b52cb7ull,
        .vertex = {.program = 0x8d4f80ddb4f99031, .regs = 0x200},
        .pixel = {.programmable = 1},
        .key_size = 264,
        .key = {
            0, 0, 0x23c190f5, 0xc7189a4, 0xe3b52cb7, 0x5932d33f, 0x200, 2, 0, 1, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0,
            0, 0, 0, 1, 0x10, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 0x302, 0x303, 0x8007,
            0, 1, 1, 0x203, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0x1b02, 0, 0x40e00000, 0x2c, 0
        },
    },
};

