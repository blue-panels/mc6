# PDF Viewer <!-- help:notitle -->

PDF viewer (lua-pdf)

F3 on a PDF shows the page: the text layer as text, the pictures as
sixel where the terminal draws it.  pdftohtml reads the pages, one run
of eight at a time, and writes the pictures out next to them.

A page is written out whole, so the usual keys scroll it: PgUp, PgDn,
the arrows, Home and End.  The menu changes the page.

```
  >          - the next page
  <          - the page before
  p          - go to a page by its number
  +  -       - the zoom, from 25% to 400%
  /          - search the text of the document
  n  N       - the next match, the one before
  i          - the menu: the page and the zoom, and the way the page
               is laid out
  F8         - switch between the plugin's view and the file itself
  F1         - this help
```

The search reads the pages as it goes and stops after 200 of them.  The
text of the last four runs of pages it read is kept, along with the
files written out for them and the pictures of the page on the screen;
the rest is read again when the reader comes back to it.  The
page with the match is drawn with the match in reverse video, and the
viewer opens the page at its row.  F7 and F6, the search and the filter
of the viewer, work on the rows of a file and are switched off here:
the screen is drawn from a stream, not read from a file.

In the menu, Alt-N is the next page and Alt-P the previous one; the
Page field takes a number.  Three layouts:

```
  text       - a line of the document is a row of the screen, and a
               character of the document is about a cell wide; the
               pictures sit in the flow of the text
  page       - the paper as printed, scaled to the width of the viewer
  fit        - the paper as printed, the whole page in the window
```

A page with no text layer is a picture of a page, and is laid out
"fit" whatever the menu says.

The status line of the viewer says which page is shown, the keys that
move it, the search if one is running, and the layout and the zoom
when they are not the plain ones.

The text needs poppler-utils (pdftohtml, pdfinfo).  The pictures need
libsixel (img2sixel) or chafa where the terminal draws sixel, and chafa
where it does not: there a picture is drawn with characters, in the
place and the size it has on the page.  A picture larger than the window
is drawn smaller: the viewer draws a picture whole or not at all.  With
neither of them the status line says "no pictures" and \[image\] stands
where a picture belongs.  Where pdftohtml is not installed there are no
pages to show: the viewer shows the file as it is, and the status line
names what is missing.
