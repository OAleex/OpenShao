# Contributing to OpenShao

Bug fixes, testing, documentation, translations and faithful restorations are welcome.
Read the [AI usage policy](AI_POLICY.md) before submitting a contribution.

## Project direction

OpenShao aims to give Shaolin Monks the care its original release needed.
The project combines a native PC port with fixes, improvements and restored cut content.
It preserves the game's identity, atmosphere and gameplay.

Restorations need evidence from the original game or reliable development material.
An unused file alone does not establish how it was meant to work. Explain what is
known and what would require interpretation. When the original trigger is lost, an
interpretation is acceptable if it uses only original material, follows the file's
own context (area, scene, script) and stays optional. Discuss changes to combat, balance,
story or presentation with the maintainer before starting substantial work.
Features that take the game away from its original character do not belong in the
base project.

### Creative additions and mods

Creative additions and speculative reconstructions, including AI-generated content
absent from the original game, are welcome as optional [mods](docs/MODS.md).
Label them as new creations or interpretations. Disclose any AI use.

Keep these mods separate from restorations in the base game. Use the supported mod interfaces.
The current mod system has limited extensibility.
The same rules on game files, private data and licensing apply to mod submissions.

## Fan translation mods

Made a fan translation or fan dub of the game? See [Offer a fan translation mod](docs/TRANSLATIONS.md).
Use the Fan translation mod issue form. Do not open pull requests with translated game data.

## Issues and pull requests

- For a bug, describe what happened, what you expected and how to reproduce it.
  Include the OpenShao version and relevant system details. See the
  [reporting instructions](README.md#how-do-i-report-a-problem).
- Keep each PR focused. Explain the problem, the change and how you checked it.
- Understand every change you submit. Be ready to explain the code and address review feedback.
- Test the affected behavior. State what you tested and what still needs checking.
  See [Building](docs/BUILDING.md) for the build and test commands.
- Write code, comments and documentation in English. Preserve existing localization.
- Keep descriptions short and specific. Put version-by-version changes in release notes.
- Be respectful of contributors and the maintainer's time. A submission does not guarantee acceptance.

## Game files and private data

Players must supply their own game. Never submit these files:

- Disc images, XBE files or game archives.
- Extracted textures, models, audio or movies.
- Generated translations of game code.
- Proprietary SDK files.

Exclude saves, raw memory dumps, analysis databases, credentials and other private data from public submissions.

**Uploading game files or private data to the project's public channels results in
a permanent ban, even if the upload was accidental.** This covers PR commits and
their history, issues, comments, attachments and links to unauthorized downloads.
Review every file before pushing. Deleting it in a later commit does not remove it
from the earlier history.

Use a screenshot or a short gameplay clip to illustrate a visual bug. For diagnostics,
use the launcher's filtered report. Remove crash dumps and any private data from the
archive before sharing it publicly. Raw rendering captures contain game resources
and must stay local.

Original work offered to the project needs clear authorship and licensing.
Discuss asset contributions with the maintainer first. Keep mod authoring files local.
Follow the [mod guide](docs/MODS.md) for reviewed packages. The project's
[code license](LICENSE) does not grant rights to the original game's assets.
