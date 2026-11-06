3DS or 2DS with a flashcart
===========================

This covers any 3DS, 2DS, New 3DS or New 2DS using a DS flashcart in the
cartridge slot.

You'll need: your flashcart, its microSD card, and a computer with a
microSD card reader.


1. Copy the files to the flashcart
----------------------------------

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

Put the microSD card back in the flashcart, the flashcart in the 3DS, and
start the flashcart from the HOME Menu. In your flashcart's normal menu,
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


4. Optional: the 3DS Pokewalker app
-----------------------------------

Skip this if your 3DS can't run homebrew. If you've never set up the
Homebrew Launcher, it can't.

If it can, copy the contents of "Copy to 3DS SD card" onto the root of
the 3DS's own SD card (the one in the side or back of the console, NOT
the flashcart's microSD). It adds:

  3ds/pokewalker3ds/pokewalker3ds.3dsx
  3ds/pokewalker3ds/pokewalker3ds.smdh

Open it from the Homebrew Launcher. It can count your real steps with the
3DS's pedometer.

One catch: the 3DS app only sees walker files on the 3DS's own SD card.
Your flashcart game keeps its walker on the flashcart's microSD, so to
use the 3DS app with it you'll need to copy the .walker file across. For
most people with a flashcart, the Pokewalker-NDS companion on the
flashcart is simpler.


Good to know
------------

  - The virtual Slot-1 and Slot-2 options (Pal Park and Poke Transfer)
    need TWiLight Menu++ running from the 3DS's own SD card, which means
    custom firmware. They don't show up on this setup.
  - Using the virtual Pokewalker with a real HGSS cartridge also needs
    custom firmware. If you install it later, use the "3DS - custom
    firmware" folder instead.
