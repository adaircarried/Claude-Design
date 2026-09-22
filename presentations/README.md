# Tenneco intern project presentation

Executive deck for the internship project review — robotic welding (GMAW),
ILUO skill path.

| | |
|---|---|
| Presenter | Diego Adair de León Márquez |
| Program | Technical Setter Interns |
| Plant tutor | Fernando Robledo |
| Audience | Plant manager · HR · Engineering manager |
| Date | 25 September 2026 |
| Length | 13 slides · ~15 min · English |

## Files

- `Tenneco_Intern_Project_Diego_de_Leon.pptx` — the deck. Speaker notes in
  English on every slide.
- `Tenneco_Intern_Project_Diego_de_Leon.pdf` — read-only preview.
- `deck.py` — generator. Rebuilds the deck from the corporate template so the
  masters, layouts, logo and photographic backgrounds stay untouched.

## Rebuilding

The corporate template is marked TENNECO CONFIDENTIAL and is deliberately not
committed. Put it next to this script as `template.pptx`, or pass its path:

```bash
pip install python-pptx
python3 deck.py /path/to/Presentacion_de_becarios.pptx
```

## Narrative

Every slide carries a **message title**: it states its conclusion rather than
naming its topic, with a small eyebrow above it for the section and, where it
helps, a support line below it for the evidence. Slide 3 is the executive
summary, placed up front so the argument is complete within the first three
minutes.

Slides are deliberately sparse. Roughly 1,150 words of narration live in the
speaker notes, not on the slides, which is about nine minutes spoken and leaves
room for questions.

## Transitions

Every slide carries a **Morph** transition, written straight into the slide XML
behind a markup-compatibility choice: PowerPoint 2016 and newer play Morph,
anything older falls back to a fade. The four ILUO badges on slides 5 and 6
share the shape names `!!iluo_I` through `!!iluo_O`, which forces PowerPoint to
match them, so the level cards animate down into the scorecard rows instead of
cross-fading. Keep those names if you edit those two slides.

## Still to fill in

Slide 8 (*The integration project*) carries four bracketed placeholders — what
the integration is, the role, what it taught, and its status. Everything else
is final.
