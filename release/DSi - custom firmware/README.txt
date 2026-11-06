DSi or DSi XL with custom firmware
==================================

This is for a DSi that's been modded with Unlaunch (for example by
following dsi.cfw.guide), so TWiLight Menu++ runs from the SD card in the
side of the console. hiyaCFW works too.

You'll need: the DSi's SD card and a computer with an SD card reader.


1. Copy the files
-----------------

Turn the DSi off, take out its SD card and put it in your computer.

Open "Copy to DSi SD card", select everything inside it, and drag it onto
the root of the SD card. "Root" means the very top of the card, not
inside any folder. If your computer asks about merging folders or
replacing files, choose yes to all.

When you're done, the top of the SD card should have:

  BOOT.NDS          TWiLight Menu++ (Unlaunch starts this)
  _nds              TWiLight Menu++ and SD Transporter files
  roms              where your games go
  title             only used by hiyaCFW, harmless otherwise
  ntrboot.nds       part of TWiLight Menu++, leave it there

If you already had TWiLight Menu++ on this card, that's fine. Copying
over it keeps your settings, and just updates it to the SD Transporter
version.


2. Start TWiLight Menu++
------------------------

Put the SD card back and turn the DSi on. If Unlaunch is set to start
TWiLight Menu++ (most guides set it up that way), it opens straight away.
If not, hold A and B while turning the DSi on to open the Unlaunch menu
and pick TWiLight Menu++ from there.


3. Set up the Pokewalker
------------------------

Put your HeartGold or SoulSilver ROM in roms/nds on the SD card. Then
follow "HOW TO USE THE POKEWALKER.txt" in the main folder.


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

You can use the virtual Pokewalker with a real HGSS cartridge too.

  1. Back up your cartridge's save first, if you can.
  2. In TWiLight Menu++, open Virtual-Pokewalker-Cartridge from your
     game list (it's in roms/nds).
  3. Put your HGSS cartridge in when it asks, then press A to check it.
  4. It shows your trainer and Pokemon. Press A to start the game, or B
     to cancel.
  5. Use the Pokewalker in the game as normal, pressing SELECT when it
     searches. Leave the SD card in the whole time.

The cartridge's walker is saved on the SD card in roms/nds/saves, so you
can walk with it in the Pokewalker-NDS companion like any other walker.
