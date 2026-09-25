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

- `Tenneco_Robotics_Setter_Diego_de_Leon_V3.pptx` is the current deck: the
  supervisor's review, applied on top of his own edits. The speaker notes
  carry only the key points not to forget.
- `Tenneco_Robotics_Setter_Diego_de_Leon_V3.pdf` is a read-only preview.
  LibreOffice renders it without Segoe UI, so lines wrap a little earlier
  than they do in PowerPoint.
- `Tenneco_Robotics_Setter_Diego_de_Leon_V2.pptx` is the supervisor's
  hand-edited version, with Diego's photos. V3 is built from it.
- `revise_v3.py` turns V2 into V3. It edits V2 in place, so the
  supervisor's changes and the photos are kept.
- `deck.py` generated V1, before the review. It is kept for reference and no
  longer describes the current deck.

## Supervisor review (V2 to V3)

1. **Gantt instead of the timeline** (slide 5). The template asks for one
   explicitly. It shows weeks 32 to 53, real in green and plan in light blue
   (the plant Excel's colours), today's date, and the Level I exam in
   week 40.
2. **Level I is not complete** (slides 6, 7 and 13). Five topics mastered:
   safety, GMAW equipment, process variables, discontinuities and control
   documents. Four still open: metal transfer, joints, positions and
   symbols.
3. **The ILUO matrix as the plan** (slide 4). The objective, then one row per
   level: what it covers, how it is validated, the hours and the method.
   That adds up to 40 hours of formal training.

Also in V3:

- The supervisor's wording is kept, cleaned of long dashes and typos.
- The numbers now agree across slides:
  - 90% industrialization and 10% cell support.
  - Level L in October, U and O in November and December.
  - The exam in week 40.

## Rebuilding

The corporate template is marked TENNECO CONFIDENTIAL, so it is deliberately
not committed. Put it next to this script as `template.pptx`, or pass its
path:

```bash
pip install python-pptx pillow
python3 revise_v3.py Tenneco_Robotics_Setter_Diego_de_Leon_V2.pptx /path/to/Presentacion_de_becarios.pptx
```

The template is only read for its road background, to cut the glass panels.

## Storyline

| # | Slide | Role |
|---|---|---|
| 1 | Title | Program information |
| 2 | Win | The Tenneco value |
| 3 | Program scope and objective | The official objective and both halves of the final project |
| 4 | The plan | The ILUO matrix, paraphrased, under the objective |
| 5 | Level I exam next, in week 40 | Gantt of skills, done and next |
| 6 | Level I | Five topics mastered, four still open |
| 7 | Eleven skills achieved | Progress on the 23 skills, one dot per skill |
| 8 | Industrialization | MY27 Cummins DOC and DPF program, 90% of the time |
| 9 | Improvement idea and implementation | 4M: problem, idea and what is running today |
| 10 | Results | Cycle time 174 s to 93 s, load, weld and unload per part |
| 11 | Training process update | A proposal: reviewed, found, rebuilt |
| 12 | Next | Levels L, U and O, and what is needed |
| 13 | Conclusions | In place and next |
| 14 | Thank you | |

Totals on slide 7:

| Level | Achieved | In progress | Not started |
|---|---|---|---|
| I | 5 | 4 | 0 |
| L | 5 | 0 | 0 |
| U | 0 | 1 | 2 |
| O | 1 | 1 | 4 |
| **Total** | **11** | **6** | **6** |

Where the counts come from:

- Level I follows Diego.
- Level L follows the supervisor's V2.
- In Level O, the 4M improvement counts as *Implementing improvement ideas*,
  in progress.

The schedule in `nivel-I/` predates this review and has not been updated to
match.

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

Morph pairs objects by their `!!` name, but only when both are **the same
kind of object**. A picture never morphs into a rectangle, even with the same
name. That is why every panel is one rectangle. On dark slides the copied
background is that rectangle's picture fill, and on white slides the
rectangle has a flat tint.

Names that carry from slide to slide:

- `!! Title 1` and `!! Subtitle 1`.
- `!! Glass A` for the left panel and `!! Glass B` for the right one.
- On the card slides (4 and 11), the first card is `!! Glass A` and the last
  card is `!! Glass B`. That way the two panels split into the cards and join
  back together on the next slide.

To keep the animation working when you edit:

1. Keep the names. Rename a shape in Home > Arrange > Selection Pane.
2. Do not replace a panel with a picture. To show a photo inside a panel,
   put the photo on top of it.
3. Do not copy and paste a panel as a picture.

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
