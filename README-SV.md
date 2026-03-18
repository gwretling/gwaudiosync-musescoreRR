# GW Audio Sync GUI – Windows VST3

Det här är GUI-versionen av pluginen för MuseScore/andra VST3-hostar.

Funktioner:
- synligt pluginfönster
- knapp för att ladda ljudfil
- visar vald fil
- gain
- loop
- följer hostens play/stop/seek via transporten

## GitHub Actions-bygg

Lägg hela mappen i ditt repo och använd workflowen i:

`.github/workflows/build-windows-vst3.yml`

Artifacten du ska ladda ner heter:

`GWAudioSyncGUI-vst3`

## Lokal byggning på Windows

```bat
build_windows.bat
```

## Viktigt

Den här versionen använder JUCE och hämtar automatiskt JUCE från GitHub om du inte redan har en lokal checkout i `third_party/JUCE` eller via `JUCE_DIR`.
