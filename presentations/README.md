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
| Length | 15 slides, about 9 minutes of scripted notes, in English |

## Files

- `Tenneco_Robotics_Setter_Diego_de_Leon.pptx` is the deck. Every slide has
  English speaker notes.
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
| 8 | Industrialization | How time is split, 60% project and 40% cell support |
| 9 to 11 | Improvement | The manual booth, the move to 4M, and before versus after |
| 12 | Training process update | A proposal: reviewed, found, rebuilt |
| 13 | Next | Levels L, U and O, and what is needed |
| 14 | Conclusions | In place and next |
| 15 | Thank you | |

The 4M improvement counts as the first skill in progress under *Implementing
improvement ideas* (Level O), so the totals are 4 on my own, 13 in progress
and 6 not started. The schedule in `nivel-I/` still lists that skill as not
started.

## Style

The style follows Diego's previous Tenneco deck:

- Dark road backgrounds.
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

The deck has no photos. Diego adds his own by hand:

| Slide | Frame | Photo |
|---|---|---|
| 9 | PHOTO, Manual welding booth | The booth before the change |

To use a frame, delete its label, drop the photo on top, and crop the photo to
the frame. Photos also fit well on slides 8 and 10 if there is room.

## Still to fill in

- **Slide 8, *My part*:** two bracketed lines, the industrialization project
  and your role in it. The notes have a matching bracket.
- **Slide 11, results:** every `XX` value. That is minutes per part before
  and after, % reduction, welds moved to the robot, and minutes of
  uncomfortable welding removed per shift. The notes on slide 11 use the same
  `XX` placeholders.
- **Slide 10, the four steps:** confirm that they match what was actually
  done.
