# Robotics Setter Technician: progress presentation

Progress deck for the plant manager. It is built on the official objectives
of the *Técnico Ajustador Robótica* program and ends with the final project:
the 4M improvement and the training process update.

| | |
|---|---|
| Presenter | Diego Adair de León Márquez |
| Program | Robotics Setter Technician (Técnico Ajustador Robótica) |
| Plant tutor | Fernando Robledo |
| Audience | Plant manager, non-technical |
| Date | 25 September 2026 |
| Length | 14 slides, 15 minutes, in English |

## Files

- `Tenneco_Robotics_Setter_Diego_de_Leon.pptx` is the deck. The speaker notes
  are not a script. Each slide carries only the key points not to forget.
- `Tenneco_Robotics_Setter_Diego_de_Leon.pdf` is a read-only preview.
  LibreOffice renders it without Segoe UI, so lines wrap a little earlier
  than they do in PowerPoint.
- `deck.py` is the generator.

## Rebuilding

The corporate template is marked TENNECO CONFIDENTIAL, so it is deliberately
not committed. Put it next to this script as `template.pptx`, or pass its
path:

```bash
pip install python-pptx pillow
python3 deck.py /path/to/Presentacion_de_becarios.pptx
```

## Storyline

| # | Slide | Role |
|---|---|---|
| 1 | Title | Program information |
| 2 | Win | The Tenneco value |
| 3 | Program scope and objective | The official objective and both halves of the final project |
| 4 | Four levels | I, L, U and O, in plain words |
| 5 | Eight weeks in, fourteen to go | Timeline, with today highlighted |
| 6 | Level I | The five topics and how they were learned |
| 7 | Four skills on my own | Progress on the 23 skills, one dot per skill |
| 8 | Industrialization | The Cummins project, 60% of the time, and 40% at the cells |
| 9 | Improvement idea and implementation | 4M: problem, idea and what is running today |
| 10 | Results | Cycle time 174 s to 93 s, load, weld and unload per part |
| 11 | Training process update | A proposal: reviewed, found, rebuilt |
| 12 | Next | Levels L, U and O, and what is needed |
| 13 | Conclusions | In place and next |
| 14 | Thank you | |

The 4M improvement counts as the first skill in progress under *Implementing
improvement ideas* (Level O), so the totals are 4 on my own, 13 in progress
and 6 not started. The schedule in `nivel-I/` still lists that skill as not
started.

## Style

Backgrounds alternate so the deck does not read as one long block. Slides
1, 2, 4, 6, 8, 10, 12 and 14 are dark. Slides 3, 5, 7, 9, 11 and 13 use the
white `Standard_Light` layout, with navy text, Tenneco blue (0033A0) instead
of lime, and panels in a soft navy tint, since glass has nothing to copy on
white. The 4M results (slide 10) stay dark on purpose, so the lime numbers
stand out most.

The dark slides follow Diego's previous Tenneco deck:

- Road backgrounds.
- 28 pt white message titles with a 13 pt lime support line.
- Cards on a 0.28 in margin grid.
- The lime chevrons are left alone: nothing sits in their zone, bottom left.

**Glass panels.** Each panel copies the exact piece of layout background it
sits on. That copy is lightly blurred, lifted toward a light steel blue (gray
for the *before* card) and saturated, then placed back on the same spot at
50% opacity. Opacity is a native picture setting (Format Picture >
Transparency), so it can still be tuned by hand. The generator writes the
crops to `build/glass/`, which is ignored by git.

## Transitions

Every slide uses **Morph**, with a fade fallback for PowerPoint versions older
than 2016.

Titles are named `!! Title 1`. Panels that repeat from slide to slide share
`!!` names, so PowerPoint slides and resizes them instead of cross-fading:

- `!! Glass A` and `!! Glass B`
- `!! Card 1` to `!! Card 4`
- `!! Strip`

Keep those names if you edit the slides.

## Photos

The deck has no photos. Diego adds his own by hand in the two frames on
slide 9, *Before: manual welding booth* and *After: 4M robotic cell*. To use a
frame, delete its label, drop the photo on top, and crop the photo to the
frame.

## 4M data

The cycle times are in seconds per part and come from Diego. The fixture is
the same in both cells, so load and unload stay equal.

| | Manual booth | 4M robotic cell |
|---|---|---|
| Load | 20 | 20 |
| Welding | 146 | 65 |
| Unload | 8 | 8 |
| **Cycle time** | **174** | **93** |

That is 47% less cycle time, 56% less welding time and 81 s saved per part.
