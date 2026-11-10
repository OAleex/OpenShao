"""Keep the SDK dispatcher bound to code linked into the OpenShao executable."""


def adapt_dispatch(text, replace_once):
    begin = text.index('#ifdef _WIN32\n// the C runtime functions the game code DLL calls')
    end = text.index('\n#endif', begin) + len('\n#endif')
    text = text[:begin] + text[end:]
    begin = text.index('#ifdef _WIN32\n// the recompiled code\'s tables')
    end = text.index('\n#endif', begin) + len('\n#endif')
    text = text[:begin] + '''// Generated tables are linked into the executable on both platforms.
const X86FuncMapping* g_mappings = X86FuncMappings;
const uint32_t* g_continuations = X86Continuations;
uint32_t g_entry = X86EntryPoint;''' + text[end:]
    begin = text.index('#ifdef _WIN32\n// the game code DLL and its tables')
    end = text.index('\n#endif', begin) + len('\n#endif')
    text = text[:begin] + text[end:]
    return replace_once(text, '''#ifndef _WIN32
    g_lo = X86CodeBase;
    g_hi = X86CodeEnd;
#endif''', '''    g_lo = X86CodeBase;
    g_hi = X86CodeEnd;''')
