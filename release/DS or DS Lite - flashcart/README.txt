DS or DS Lite with a flashcart
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

Put the microSD card back in the flashcart, the flashcart in the DS, and
turn it on. In your flashcart's normal menu, open BOOT.NDS.

If BOOT.NDS won't open, or you want TWiLight Menu++ to start
automatically every time, open "Flashcart boot files (only if your
flashcart needs them)":

  - Autoboot: find the folder with your flashcart's name and copy what's
    inside it to the root of the microSD card. After that, TWiLight
    Menu++ starts by itself when you turn the DS on.
  - Flashcart Loader: read the README.txt in there. It lists which
    loader to use for flashcarts that can't open BOOT.NDS directly.

Most flashcarts don't need either of these.


3. Set up the Pokewalker
------------------------

Put your HeartGold or SoulSilver ROM in roms/nds on the microSD card.
Then follow "HOW TO USE THE POKEWALKER.txt" in the main folder.


Good to know
------------

  - On an original DS or DS Lite, the virtual Pokewalker is still
    experimental. Keep copies of your .sav and .walker files.
  - The virtual Slot-1 and Slot-2 options (Pal Park and Poke Transfer)
    need a DSi or 3DS running TWiLight Menu++ from the console's own SD
    card, so they don't show up on this setup.
  - Real HeartGold/SoulSilver cartridges can't use the virtual
    Pokewalker on a DS or DS Lite. That needs a DSi or 3DS.
