# Describing a player's look for ISSD

A player's look is described with three values:

```json
{ "skin": "dark", "hair_color": "blond", "hair_style": 12 }
```

Two of those are not game keys. The loader does not read `skin` or
`hair_color`: they are labels the fitter consumes, and it turns them into the
`skin_tones` and `skin_tone` keys the loader does read. Write the labels per
player, run the fitter, then ship the pack it writes.

This page is for turning a photo (or a description) into those three values,
by hand or by showing the photo to an AI model - the prompt at the end can be
pasted as it is.

## From labels to a pack

1. **Label.** Give each player `skin`, `hair_color` and `hair_style` in the
   pack's JSON, using the vocabulary below.
2. **Fit.** Run

   ```
   python tools/fit_looks.py --pack <pack.json>
   ```

   The fitter looks for the retail cartridge in the working directory; if
   yours is elsewhere, pass `--base <rom>`. It rewrites the pack in place
   unless `--out <pack.json>` names a new file. A team has two palettes, so it
   works out which two skin-and-hair combinations come closest to what the
   squad asked for, writes the team's `skin_tones`, gives each labelled player
   his `skin_tone` and keeps his `hair_style`, and removes the `skin` and
   `hair_color` labels. It warns for each player it cannot represent exactly,
   naming the closest palette used instead.
3. **Play.** Copy the fitted pack into `mods/` and enable it. A pack that
   still carries `skin` or `hair_color` loads - the loader skips keys it does
   not know - but the look does nothing until the fitter has converted it.

The palette-level keys the loader actually reads are documented in
[MODDING.md](MODDING.md), section 6a.

## skin

`"light"` or `"dark"` - the game has no third tone. Judge the face in normal
light: clearly brown skin is `dark`; pale, tanned, olive and anything lighter
is `light`.

## hair_color

`"black"`, `"brown"`, `"blond"`, `"red"` or `"grey"` (white hair counts as
grey). Dark brown that reads as black in normal light is `black`; light brown
that reads as yellow is `blond`; ginger and auburn are `red`.

## hair_style

0-13, from the hair picker in web-snes-issd-studio. The loader stores the low
nibble of the appearance byte, so 0-15 are accepted, but hair graphics exist
only for 0-13: 0 is the standard head and 1-13 are overlay graphics. 14 and
15 are written as given and have no graphic; do not use them. Goalkeepers are
always 0, and the loader forces them to 0.

Each style is a few-pixel overlay seen in one pose, so the categories are
about silhouette and length, not detail. Shapes 0-10 agree with the older ISS
Deluxe editor's names (given in brackets); 11-13 have no name anywhere, so
those three are described from the pixels alone and are the least certain.

| # | Category | What to look for in a photo |
|---|---|---|
| 0 | **Short, neat** (standard head) | An ordinary short haircut: a close cap over the top and ears, nothing long, nothing curly. The fallback when nothing else fits. |
| 1 | **Short, curly or textured, light coverage** (Curly) | Short, tousled or curly hair on top, a small tuft at the back, scalp showing through. |
| 2 | **Longer curly or wavy** (Long curly) | As 1 but longer: curls or waves reaching the nape or collar at the back. |
| 3 | **Full hair with a full beard** (Long with beard) | A bushy mass of hair framed by facial hair running under the face. Use it for any full beard. |
| 4 | **Long straight hair** (Long straight) | Smooth, heavy hair swept back and hanging well past the collar, no curl. |
| 5 | **Dreadlocks, braids or twists** (Dreadlocks) | Hair in separate ropes or twists with gaps between them. |
| 6 | **Afro or big round curly volume** (Afro) | A tall, round mass standing out all around the head, ears and temples showing. |
| 7 | **Ponytail** (Ponytail) | Hair pulled back flat on top and gathered into a single tail with a tie. |
| 8 | **Bald or nearly bald** (Bald) | Shaved head, or a small patch of hair at the back or side with the crown and forehead bare. |
| 9 | **Mid-length, smooth and rounded** (Mid length) | Neat, straight hair of medium length: a smooth arch over the ears and a flat top, no curl. |
| 10 | **Long hair with a band or ribbon** (Long with ribbon) | Long hair down the back with a headband or ribbon across the top. |
| 11 | **Swept-back or slicked-back, thin tail** | Short hair brushed back or to the side, forehead clear, a thin strand at the nape. |
| 12 | **Medium length with sideburns, face open** | Thick hair over the top and down both sides to the cheeks, ending around the collar; the face is open and there is no beard. |
| 13 | **Flat-top, buzz or high-and-tight** | Very short, flat across the top, skin showing over the ears, often a darker streak on top. |

Telling close ones apart: 1 and 2 differ only in length; 3 and 12 differ in
the beard; 6 is round and standing out where 12 hangs close to the head; 9 is
smooth and short of the collar where 12 reaches it; 8 and 13 are both very
short but 13 still has a flat band of hair on top.

## Prompt for an AI model

```
You are labelling a football player's appearance for a retro game that has
only a few looks. Look at the photo and answer with JSON only:

{"skin": "light|dark", "hair_color": "black|brown|blond|red|grey", "hair_style": 0-13}

skin: "dark" only if the skin is clearly brown; otherwise "light".
hair_color: choose the closest of black, brown, blond, red, grey (white = grey).
hair_style: choose the number whose description fits best; if unsure use 0.

0 short neat cap of hair, nothing long or curly
1 short curly or textured, sparse
2 longer curly or wavy, to the nape or collar
3 full hair with a full beard
4 long straight hair swept back, past the collar
5 dreadlocks, braids or twists
6 afro or big round curly volume
7 ponytail with a tie
8 bald or nearly bald
9 mid-length, smooth and rounded, short of the collar
10 long hair with a headband or ribbon
11 short hair swept or slicked back, forehead clear, thin strand at the nape
12 medium length with sideburns, to the collar, face open, no beard
13 flat-top, buzz cut or high-and-tight
```

## What is certain and what is not

Certain: the loader accepts 0-15, but only 0-13 have graphics (0 is the
standard head, 1-13 are overlay graphics); 14 and 15 have no graphic, and
goalkeepers are always 0. Descriptions of 0, 4, 5, 7, 8 and 10 are the
surest; 3, 6 and 9 are likely; 1, 2, 11, 12 and 13 are readings of a handful
of pixels and worth checking against the picker. The picker draws every
style in the same brown, so colour tells you nothing about hair_color.

The other check is the fitter's own output: a player whose look a team's two
palettes cannot show exactly gets the closest match and a warning naming him
and the palette used. Read those before shipping the pack.

---

The light/dark skin split, the five hair-colour classes, the hair-style table
and the AI prompt were contributed by Poligraf in the course of issue #4.
