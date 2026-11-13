3DS or 2DS with custom firmware
===============================

This is for a 3DS, 2DS, New 3DS or New 2DS with custom firmware
(Luma3DS), for example set up by following 3ds.hacks.guide. You should
already have FBI on your HOME Menu, since the guide installs it.

You'll need: the 3DS's SD card and a computer with an SD card reader.


1. Copy the files
-----------------

Turn the 3DS off, take out its SD card and put it in your computer.

Open "Copy to 3DS SD card", select everything inside it, and drag it onto
the root of the SD card. "Root" means the very top of the card, not
inside any folder. If your computer asks about merging folders or
replacing files, choose yes to all.

When you're done, the SD card should have (along with everything that
was already on it):

  BOOT.NDS                     TWiLight Menu++
  _nds                         TWiLight Menu++ and SD Transporter files
  roms                         where your games go
  cia/TWiLight Menu.cia        puts TWiLight Menu++ on your HOME Menu
  3ds/pokewalker3ds            the 3DS Pokewalker app

If you already had TWiLight Menu++ on this card, that's fine. Copying
over it keeps your settings, and just updates it to the SD Transporter
version.


2. Put TWiLight Menu++ on the HOME Menu
---------------------------------------

Skip this if TWiLight Menu++ is already on your HOME Menu.

  1. Put the SD card back and turn the 3DS on.
  2. Open FBI.
  3. Go to SD, then cia, then pick "TWiLight Menu.cia".
  4. Choose "Install and delete CIA" and press A to confirm.
  5. Press HOME. TWiLight Menu++ is now on your HOME Menu.


3. Set up the Pokewalker
------------------------

Put your HeartGold or SoulSilver ROM in roms/nds on the SD card. Then
follow "HOW TO USE THE POKEWALKER.txt" in the main folder.

For walking, you have two companion apps. Use whichever you like; they
share the same walker files.

  - Pokewalker-NDS: open it from TWiLight Menu++.
  - The 3DS Pokewalker app: open it from the Homebrew Launcher. It can
    count your real steps using the 3DS's built-in pedometer.


4. Pal Park and Poke Transfer (optional)
----------------------------------------

With this setup you can also use a virtual cartridge slot:

  - Pal Park: highlight a GBA Pokemon game (.gba file on the SD card),
    press Y, choose "Set as virtual Slot-2 cartridge" and press A. Then
    start Diamond, Pearl, Platinum, HeartGold or SoulSilver as normal,
    and the game sees that GBA game as if it were in the GBA slot.
  - Poke Transfer (moving Pokemon up to Black/White): you need a second
    console running Black, White, Black 2 or White 2. On THIS console,
    highlight your Diamond, Pearl, Platinum, HeartGold or SoulSilver
    game, press Y, choose "Set as virtual Slot-1 cartridge" and press A.
    The game needs a save already, so play it once first. Then start
    Poke Transfer on the other console as normal, and on this console
    receive it through DS Download Play. It reads your game from the SD
    card as if the cartridge were in the slot.

Press A on the same option again to turn it off. Keep backups of your
saves before moving Pokemon.


5. Optional: using a real HeartGold/SoulSilver cartridge
--------------------------------------------------------

You can use the virtual Pokewalker with a real HGSS cartridge in the
3DS's cartridge slot too.

  1. Back up your cartridge's save first, if you can.
  2. In TWiLight Menu++, open Virtual-Pokewalker-Cartridge from your
     game list (it's in roms/nds).
  3. Put your HGSS cartridge in when it asks, then press A to check it.
  4. It shows your trainer and Pokemon. Press A to start the game, or B
     to cancel.
  5. Use the Pokewalker in the game as normal, pressing SELECT when it
     searches. Leave the SD card in the whole time.

The cartridge's walker is saved on the SD card in roms/nds/saves, so you
can walk with it in either companion app like any other walker.
