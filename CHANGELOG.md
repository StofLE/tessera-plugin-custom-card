# Changelog

## 0.7.0
- A new part, `image`: a picture of Home Assistant's own where a value would stand, such as a count a template drew
  larger than the screen's largest digits, an event's poster or a person's photo:
  `image|<mark>|<source>|<label>|<colour>|<icon>|<fit>`. The source is an image entity without its `image.`
  (`image|5.358|counter_scanned|Scanned`) or a path of Home Assistant's own (`/local/poster.jpg`); the mark changes
  with the picture, and where there are no pictures (a board without the memory for them) the card shows it as a
  value. The card a tap opens shows the pictures too, larger in its room. Images stand side by side as values do, from the left under their labels, and take the
  room the tile has left until they fill their columns. `contain` (the default) shows the whole picture on the card's
  colour, `cover` fills its place.
- Needs Tessera 0.4.89 or later (plugin API 0.13, pictures on a card).

## 0.6.1 - 2026-10-18
- Rings as large as the tile: on a card of rings they grow into the room the tile has (as tall as a wide tile one row
  high), all of one size, their values in the largest face the rings hold, and the label stands in the open bottom of
  its ring instead of under it.
- Four values on a wide tile one row high stand as two rows of two where that gives them a larger face than one row.
- Values whose parts all have an icon take the row of labels where it makes them larger: the icon moves up to its
  label, so each value has its column's whole width (four values in one row under the tile's name).
- A third option of the tile in the inspector: **Values in a row**, automatic (as before), all in one row, or two to a
  row (four values on a wide tile as one row of four or two rows of two).
- These four by Schumey (pull request #3), thank you.
- One ring wherever it stands: a ring among values (two counts and their share) is the same ring as on a card of rings,
  its value large in its middle and its label in its opening, and it grows into the room the tile has, at most half
  again as tall as the values' line so it stays in step with them on a large tile, with the values in the middle of the
  line beside it. The small ring as tall as the values' numbers is gone.
- A ring among values is never set apart on a row of its own: where a narrow tile has no room for it beside them, its
  share stands as a value under the counts, which keep their large face.
- Rings that fit only without the tile's name give way to their numbers as values where those keep it: a 2x1 of rings
  shows its name.

## 0.6.0 - 2026-10-17
- A table: `row` parts that follow each other stand as rows, each label at the left and its value at the right, so the
  values of a list (rooms, batteries, departures) stand under each other.
- A long line of words takes two lines where the card has the room, instead of ending in dots.
- Rows and lines of words take a larger face where the card has room for it, never larger than its values.
- A ring: `ring` parts stand as rings, several side by side (the air of a room, the batteries of the house), the value
  large in the middle of its ring and the icon and label under it, all rings of a row as large as the largest
  (Schumey's design, pull request #2). A ring always keeps its icon and label under it, even where the values give
  theirs up for a larger face: its colour says how good a value is, not what it is. On a card with values a ring is compact: it stands among them as tall as their numbers, with its number
  inside (a share beside two counts). On a tile too small for them, their numbers stand as values.
- A badge: an icon field that starts with `#` shows its words in a box of the part's colour, as a line's number in a
  list of departures (`row|4|min|Main station|blue|#16`).
- Values of a shorter row stand under the columns of the row above it instead of spreading over the width.
- Values take the large digits also beside a row with a time or words (`row|15:25||Race day`): only the values that
  stand large count, so two counts over a share and a time are as large as the card lets them be.
- The card keeps its parts in the screen's PSRAM, so the memory Wi-Fi needs stays free.
- Two options of the tile in the inspector (Schumey, pull request #2): **Show the name** (on) and **Values in their
  colour** (off). Both keep a tile as it was by default.
- Needs Tessera 0.4.88 or later.

## 0.5.0 - 2026-10-14
- Large numbers on a large tile: where they fit and every value is a number, the values take the large digits of
  Tessera's climate card. A value with a word in it keeps the face it had.
- Green, yellow, orange, red and blue are Tessera's own colours by meaning (good, moderate, bad, the accent), readable
  on the card in light and dark, as Tessera colours a level.
- Where nothing fits a small tile, its values stand two to a row, or one, instead of being cut short in one row.
- A text beside other parts on a flat tile keeps its words whole where the parts can stand under each other
  instead; labels still come first.
- Needs Tessera 0.4.88 or later: an older one keeps offering 0.4.3.

## 0.4.3 - 2026-10-12
- Up to eight parts on a card instead of six: two full rows of four values. Each tile of the card costs the screen a
  little more memory for it.
- Lines of text and bars across the card get smaller with the values where the tile is short, instead of running out
  at the bottom; where even the smallest size is too large, the card shows the lines that fit whole.

## 0.4.2 - 2026-10-12
- A picture on the Plugins page.

## 0.4.1 - 2026-10-11
- A coloured icon is darker in the light look where it would not read on the card: a yellow one turns ochre on white.

## 0.4.0 - 2026-10-10
- A tap on the tile opens its parts large on a card over the page, with their labels.
- Needs a Tessera from 2026-10-10 or later: an older one shows the card empty under its top bar.

## 0.3.3 - 2026-10-09
- Listed under the topic Home on the Plugins page.
- Needs a Tessera with plugin API 0.7: update the app first.

## 0.3.2 - 2026-10-09
- A label without an icon or a colour always stays, so every number says what it is.
- An icon or a dot in front of a number keeps a little more room from it.

## 0.3.1 - 2026-10-09
- The tile's inspector offers only the sensors that describe a card (with an `elements` attribute).

## 0.3.0 - 2026-10-09
- First release: values, bars and lines of words that a template sensor describes, side by side on a wide tile and
  stacked on any other, in the screen's fonts and colours.
