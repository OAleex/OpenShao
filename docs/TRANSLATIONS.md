# Help translate OpenShao

Offer to translate OpenShao into a language that is not yet available.
The maintainer can contact you to agree on the work and provide the translation materials and instructions.

## Offer your help

Open an [Offer translation help issue](https://github.com/OAleex/OpenShao/issues/new?template=fan_translation.yml).
Select the languages you would like to translate OpenShao into.
The list excludes languages already available in the game or through published language pack mods.
If you select **Other**, specify the languages or regional variants in the next field.
The maintainer can review offers for languages outside the list.

Provide a contact method that the maintainer can use, such as an email address, Discord username or profile that accepts messages.
The issue is public.

## Coordinate the translation

Discuss the language, scope and next steps with the maintainer.
The maintainer can provide the files, explain what needs translation and arrange how to exchange the work.
The maintainer coordinates review, testing, author credits and publication with the contributors.

Follow the project's [rules for game files and private data](../CONTRIBUTING.md#game-files-and-private-data) when sharing materials.

## Launcher text

The launcher text is in `resources/launcher/`, one JSON file for each language.
The launcher offers the same languages as the game. Add a launcher file when the game has the language.

1. Copy `en.json` and name the copy with the language code, such as `pl.json`.
2. Set `language` to the code and `name` to the language's own name.
3. Translate each value in `strings`. Keep each key.
4. Keep each placeholder in braces, such as `{version}`, without translation.
5. Use only Latin characters up to U+00FF. The launcher font has no other characters.
6. Rebuild the executable. The build embeds all files.

The `launcher_text` test checks that each file has all keys and the same placeholders.
