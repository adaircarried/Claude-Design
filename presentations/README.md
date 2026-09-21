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

Every slide carries a **message title** — it states its conclusion rather than
naming its topic — with a small eyebrow above it for the section and a support
line below it for the evidence. Slide 3 is the executive summary, placed up
front so the argument is complete within the first three minutes.

## Still to fill in

Slide 8 (*The integration project*) carries four bracketed placeholders — what
the integration is, the role, what it taught, and its status. Everything else
is final.
