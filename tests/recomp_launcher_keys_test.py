"""Every launcher text key the code asks for must exist in the English catalog, inside its "strings" object.
A key placed outside "strings", or never added, shows up in the launcher as the key itself."""
import glob, json, os, re, sys

root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..')
strings = json.load(open(os.path.join(root, 'resources', 'launcher', 'en.json'), encoding='utf-8'))['strings']
prefixes = sorted({key.split('.')[0] for key in strings})
literal = re.compile(r'"((?:%s)\.[a-z0-9_.]+)"' % '|'.join(prefixes))
missing = {}
for path in glob.glob(os.path.join(root, 'src', 'launcher*.cpp')) + glob.glob(os.path.join(root, 'src', 'mods', '*.cpp')):
    for match in literal.finditer(open(path, encoding='utf-8').read()):
        if match.group(1) not in strings:
            missing.setdefault(match.group(1), os.path.basename(path))
if missing:
    for key, path in sorted(missing.items()):
        print('missing from resources/launcher/en.json strings:', key, '(used in %s)' % path)
    sys.exit(1)
print('PASS: every launcher text key used in the code is in the English catalog')
