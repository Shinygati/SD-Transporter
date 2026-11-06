DSi or DSi XL with a flashcart
==============================

You'll need: your flashcart, its microSD card, and a computer with a
microSD card reader.


1. Copy the files
-----------------

Take the microSD card out of your flashcart and put it in your computer.

Open "Copy to flashcart microSD", select everything inside it, and drag
it onto the root of the microSD card. "Root" means the very top of the
card, not inside any folder. If your computer asks about merging folders
or replacing files, choose yes to all.

When you're done, the top of your microSD card should have:

  BOOT.NDS          TWiLight Menu++ (open this to start it)
  BOOT_ALT.NDS      a backup way to start it, for some flashcarts
  _nds              TWiLight Menu++ and SD Transporter files
  roms              where your games go

and your flashcart's own files, if it had any.

If you already had TWiLight Menu++ on this card, that's fine. Copying
over it keeps your settings, and just updates it to the SD Transporter
version.


2. Start TWiLight Menu++
------------------------

Put the microSD card back in the flashcart, the flashcart in the DSi, and
open the flashcart from the DSi menu. In your flashcart's normal menu,
open BOOT.NDS.

If BOOT.NDS won't open, or you want TWiLight Menu++ to start
automatically every time, open "Flashcart boot files (only if your
flashcart needs them)":

  - Autoboot: find the folder with your flashcart's name and copy what's
    inside it to the root of the microSD card. After that, TWiLight
    Menu++ starts by itself when you open the flashcart.
  - Flashcart Loader: read the README.txt in there. It lists which
    loader to use for flashcarts that can't open BOOT.NDS directly.

Most flashcarts don't need either of these.


3. Set up the Pokewalker
------------------------

Put your HeartGold or SoulSilver ROM in roms/nds on the microSD card.
Then follow "HOW TO USE THE POKEWALKER.txt" in the main folder.


4. Optional: using a real HeartGold/SoulSilver cartridge
--------------------------------------------------------

Skip this unless BOTH of these are true:

  - you want to use the virtual Pokewalker with a real HGSS cartridge,
    not a ROM, AND
  - your flashcart can start programs in DSi mode with access to the
    DSi's own SD card (the SD card in the side of the console). DSpico
    can do this. Most R4-style flashcarts can't. If you're not sure,
    yours probably can't.

If that's you, copy the contents of "Copy to DSi SD card (needs DSi-mode
SD access, e.g. DSpico)" onto the root of the DSi's own SD card (not the
flashcart's microSD). It contains:

  roms/nds/Virtual-Pokewalker-Cartridge.nds   the real-cartridge launcher
  roms/nds/Pokewalker-NDS.nds                 the companion app

To play:

  1. Back up your cartridge's save first, if you can.
  2. Start Virtual-Pokewalker-Cartridge.nds from the DSi's SD card.
  3. When it asks, take the flashcart out and put your HGSS cartridge in.
     Press A to check the cartridge.
  4. It shows your trainer and Pokemon. Press A to start the game, or B
     to cancel.
  5. Use the Pokewalker in the game as normal, pressing SELECT when it
     searches. Leave the DSi's SD card in the whole time.

The walker for a real cartridge is saved on the DSi's SD card, in
roms/nds/saves. To walk, catch and find items with it, open the
Pokewalker-NDS companion from the DSi's SD card too. The copy on your
flashcart can only see walkers on the flashcart.


Good to know
------------

The virtual Slot-1 and Slot-2 options (Pal Park and Poke Transfer) need
TWiLight Menu++ running from the DSi's own SD card, which means custom
firmware. They don't show up on this setup. If you install Unlaunch
later, use the "DSi - custom firmware" folder instead.
