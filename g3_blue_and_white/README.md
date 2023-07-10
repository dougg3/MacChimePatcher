# Power Macintosh G3 (Blue and White) startup sound patcher

This applies to the Power Mac G3 with model identifier PowerMac1,1. It requires the "G3 Firmware" file from the [Power Macintosh G3 (Blue and White) Firmware Update 1.1](https://support.apple.com/kb/DL1251).

## Building

Type `make` to build this program.

## Preparing a new startup sound

Set up the sound in Audacity. It will need to be a 44.1 KHz mono sound with a maximum length of just under 2.5 seconds. Export it in Audacity by going to File -> Export -> Export Audio and choosing the following settings:

- Format: Other uncompressed files
- Header: RAW (header-less)
- Encoding: Signed 16-bit PCM

For the rest of the example commands, I will assume you named the file `sound.raw`.

This will likely export the sound as little-endian, but we need big-endian. You can make a big-endian copy using the following command:

`dd conv=swab < sound.raw > sound_be.raw`

This final `sound_be.raw` file is the file you will pass to the command in the next step.

## Running

Starting from the original `G3 Firmware` file and your custom `sound_be.raw` file, run the following command:

```
mkdir patched
./inject_chime G3\ Firmware sound_be.raw patched/G3\ Firmware
```

The new version of `G3 Firmware` in the patched directory is the newly patched output firmware file.

Make sure to preserve the resource fork of the file when you copy it back to the Mac. I handle this by storing the firmware file on a netatalk server and modifying the file through a Linux or Windows computer.

## Patching the updater

The firmware updater program won't allow you to install the patched firmware because it's already up to date.

The updater program has an allow list of firmware versions that can be patched. If you open it in a hex editor, you should see the list with entries such as:

`Apple PowerMac1,1 1.0b4 BootROM built on 11/06/98 at 09:04:50`

Simply modify one of these entries (I picked the last one) to be the following:

`Apple PowerMac1,1 1.1f4 BootROM built on 04/09/99 at 13:57:32`

Just like with the update file, make sure to preserve the resource fork when you make this change.

Note that after installing the update, it will warn you that the update was unsuccessful. This is a false alarm. If it persists at every boot, make sure your `System Folder:Startup Items` folder doesn't contain a copy of the firmware updater.

## More information

My blog has [a post with more info about how I patched the startup chime](https://www.downtowndougbrown.com/2012/07/power-macintosh-g3-blue-and-white-custom-startup-sound/).
