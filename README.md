Crypt Sweeper
=============

Death has discovered a loophole in their contract: no humans means no job - and early retirement!
Battle monsters strategically placed across a haunted graveyard, uncover enemy patterns, and and
foil Death’s twisted plan to end humanity.

## Credits

Crypt Sweeper made by:

Sean Connelly and Casey Dean from [Pocket Pulp](https://pulp.biz).

### Special Thanks

[Daniel Benmergui](https://danielben.itch.io/) - Creator of
[DragonSweeper](https://danielben.itch.io/dragonsweeper) which inspired this game

L. Beethoven - Moonlight Sonata

[Adigun A. Pollack](https://bsky.app/profile/adigunpolack.bsky.social) -
[AAP-64 Palette](https://lospec.com/palette-list/aap-64)

Build Instructions
==================

This project used [gba-bootstrap](https://github.com/AntonioND/gba-bootstrap) as a starting point.

You will need to install the ARM GCC via:

```bash
# Mac OSX
brew install gcc-arm-embedded

# Linux
sudo apt install gcc-arm-none-eabi

# Windows
# Download from:
#   https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
```

Then, run `make`:

```bash
make
```

And the ROM will be output to `tgt/cryptsweeper.gba`.
