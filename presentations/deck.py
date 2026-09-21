# -*- coding: utf-8 -*-
"""Tenneco intern project deck - Diego Adair de Leon Marquez.

Built on top of the corporate template so the masters, layouts, logo and
photographic backgrounds stay exactly as the brand team shipped them.
"""
from pptx import Presentation
from pptx.util import Inches, Pt
from pptx.dml.color import RGBColor
from pptx.enum.text import PP_ALIGN, MSO_ANCHOR
from pptx.enum.shapes import MSO_SHAPE, MSO_CONNECTOR

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
# Corporate template supplied by the internship program. It is marked
# TENNECO CONFIDENTIAL, so it is deliberately NOT committed here - drop it
# next to this script as template.pptx, or pass its path as argv[1].
SRC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "template.pptx")
OUT = os.path.join(HERE, "Tenneco_Intern_Project_Diego_de_Leon.pptx")

# ---------------------------------------------------------------- palette
# Straight from the template's own colour-palette slides.
NAVY   = RGBColor(0x05, 0x1C, 0x2C)   # primary
CARD   = RGBColor(0x0B, 0x2A, 0x3E)   # navy one step up, for cards on dark
LIME   = RGBColor(0xD5, 0xFB, 0x00)   # accent - used sparingly, never as data
WHITE  = RGBColor(0xFF, 0xFF, 0xFF)
GRAYL  = RGBColor(0xC6, 0xCD, 0xD1)
BLUE   = RGBColor(0x00, 0x33, 0xA0)   # Tenneco blue
MUTED  = RGBColor(0x6E, 0x7C, 0x87)   # muted ink on light
MUTEDD = RGBColor(0x9F, 0xAF, 0xBA)   # muted ink on dark
TINT   = RGBColor(0xF4, 0xF6, 0xF7)   # card surface on light
RULE   = RGBColor(0x1B, 0x38, 0x4A)   # hairline on dark
FONT   = "Segoe UI"

# Ordered status ramp, single hue, light -> dark. Status is never carried by
# colour alone: every use ships with a written legend and a count.
S_DONE, S_WIP, S_NOT = 2, 1, 0
FILL = {S_DONE: BLUE,
        S_WIP:  RGBColor(0x5C, 0x85, 0xD6),
        S_NOT:  RGBColor(0xE3, 0xE7, 0xEB)}

SW = 13.333

# ---------------------------------------------------------------- helpers
def txbox(slide, x, y, w, h, align=PP_ALIGN.LEFT, anchor=MSO_ANCHOR.TOP):
    tb = slide.shapes.add_textbox(Inches(x), Inches(y), Inches(w), Inches(h))
    tf = tb.text_frame
    tf.word_wrap = True
    tf.margin_left = tf.margin_right = tf.margin_top = tf.margin_bottom = 0
    tf.vertical_anchor = anchor
    tf.paragraphs[0].alignment = align
    return tb, tf

def para(tf, text, size, color, bold=False, first=False, space_before=0,
         align=None, italic=False, spacing=None):
    p = tf.paragraphs[0] if first else tf.add_paragraph()
    if align is not None:
        p.alignment = align
    p.space_before = Pt(space_before)
    p.space_after = Pt(0)
    if spacing:
        p.line_spacing = spacing
    r = p.add_run()
    r.text = text
    r.font.size = Pt(size)
    r.font.bold = bold
    r.font.italic = italic
    r.font.name = FONT
    r.font.color.rgb = color
    return p

def rect(slide, x, y, w, h, fill=None, line=None, lw=1.0,
         shape=MSO_SHAPE.RECTANGLE):
    sp = slide.shapes.add_shape(shape, Inches(x), Inches(y), Inches(w), Inches(h))
    if fill is None:
        sp.fill.background()
    else:
        sp.fill.solid()
        sp.fill.fore_color.rgb = fill
    if line is None:
        sp.line.fill.background()
    else:
        sp.line.color.rgb = line
        sp.line.width = Pt(lw)
    sp.shadow.inherit = False
    # drop the theme style reference - it re-applies a drop shadow in some
    # renderers even when effectLst is empty
    st = sp._element.find('{http://schemas.openxmlformats.org/presentationml/2006/main}style')
    if st is not None:
        sp._element.remove(st)
    tf = sp.text_frame
    tf.word_wrap = True
    tf.margin_left = tf.margin_right = tf.margin_top = tf.margin_bottom = 0
    return sp

def badge(slide, x, y, d, letter, fill, color, size):
    sp = rect(slide, x, y, d, d, fill=fill, shape=MSO_SHAPE.OVAL)
    sp.text_frame.vertical_anchor = MSO_ANCHOR.MIDDLE
    para(sp.text_frame, letter, size, color, bold=True, first=True,
         align=PP_ALIGN.CENTER)
    return sp

def dash(slide, x, y, w=0.69, color=LIME, lw=2.25):
    """The tick the template itself sets above a section title."""
    ln = slide.shapes.add_connector(MSO_CONNECTOR.STRAIGHT, Inches(x), Inches(y),
                                    Inches(x + w), Inches(y))
    ln.line.color.rgb = color
    ln.line.width = Pt(lw)
    return ln

def chev(slide, x, y, size=0.22, color=LIME, n=2, gap=0.16):
    """Brand chevron motif, used as the marker that carries the eye forward."""
    for i in range(n):
        sp = rect(slide, x + i * gap, y, size, size, fill=color,
                  shape=MSO_SHAPE.CHEVRON)
    return sp

# ---------------------------------------------------------------- deck
prs = Presentation(SRC)
layouts = {l.name: l for l in prs.slide_masters[0].slide_layouts}

sld_lst = prs.slides._sldIdLst
RELNS = '{http://schemas.openxmlformats.org/officeDocument/2006/relationships}id'
for sid in list(sld_lst)[1:]:            # keep slide 1, drop the sample deck
    prs.part.drop_rel(sid.get(RELNS))
    sld_lst.remove(sid)

def new(layout_name, number, dark, footer=True):
    s = prs.slides.add_slide(layouts[layout_name])
    for ph in list(s.placeholders):      # start from a clean canvas
        ph.element.getparent().remove(ph.element)
    if footer:
        tb, tf = txbox(s, 0.28, 7.24, 4.0, 0.2)
        para(tf, "TENNECO CONFIDENTIAL", 8, MUTEDD if dark else MUTED, first=True)
        tb, tf = txbox(s, 12.10, 7.22, 0.55, 0.24, align=PP_ALIGN.RIGHT)
        para(tf, str(number), 9, MUTEDD if dark else MUTED, first=True)
    return s

def head(s, title, sub, dark):
    dash(s, 0.53, 0.66)
    tb, tf = txbox(s, 0.5, 0.86, 12.4, 0.72)
    para(tf, title, 34, LIME if dark else NAVY, bold=True, first=True)
    if sub:
        tb, tf = txbox(s, 0.5, 1.54, 11.6, 0.4)
        para(tf, sub, 14, MUTEDD if dark else MUTED, first=True)

notes = {}

# ============================================================ 1 · title
s1 = prs.slides[0]
ph = {p.placeholder_format.idx: p for p in s1.placeholders}

def only_run(p_ph, text):
    tf = p_ph.text_frame
    for pa in list(tf.paragraphs)[1:]:
        pa._p.getparent().remove(pa._p)
    p0 = tf.paragraphs[0]
    for r in list(p0.runs)[1:]:
        r._r.getparent().remove(r._r)
    p0.runs[0].text = text
    return tf, p0

only_run(ph[13], "Technical Setter Interns")
only_run(ph[14], "Diego Adair de León Márquez")
tf, p0 = only_run(ph[15], "Universidad Politécnica de Aguascalientes")
p2 = tf.add_paragraph()
p2.alignment = p0.alignment
r = p2.add_run()
r.text = "Mechatronics Engineering  ·  9th term"
r.font.size = Pt(14); r.font.name = FONT; r.font.color.rgb = GRAYL

# lime kicker above the title, so the discipline reads before the name
tb, tf = txbox(s1, 0.0, 2.02, SW, 0.3, align=PP_ALIGN.CENTER)
para(tf, "ROBOTIC WELDING   ·   ILUO SKILL PATH", 12, LIME, bold=True,
     first=True)

tb, tf = txbox(s1, 0.0, 5.52, SW, 0.28, align=PP_ALIGN.CENTER)
para(tf, "Plant tutor:  Fernando Robledo", 13, WHITE, first=True)
tb, tf = txbox(s1, 0.0, 5.94, SW, 0.28, align=PP_ALIGN.CENTER)
para(tf, "Tenneco Aguascalientes  ·  Clean Air  ·  September 25, 2026",
     11, GRAYL, first=True)

notes[0] = (
    "Good morning. My name is Diego de Leon, ninth term Mechatronics at UPA. "
    "For the last two months I have been the welding intern in the robotic "
    "cells here in Aguascalientes, with Fernando Robledo as my plant tutor. In "
    "the next fifteen minutes I want to show you exactly where I stand on the "
    "welding skill path, what I actually learned, and the four things I need "
    "from you to finish it before January.")

# ============================================================ 2 · WIN
s = new("Standard_Dark", 2, dark=True)
head(s, "Our value: WIN", None, dark=True)
tb, tf = txbox(s, 0.5, 1.56, 11.2, 0.4)
para(tf, "“We must earn the trust of our employees and customers.”",
     16, WHITE, italic=True, first=True)

y = 2.5
for title, body in [
    ("Trust comes before access",
     "On the floor nobody lets you touch a welding cell because of a job "
     "title. You get there by being useful first, and by being useful again "
     "the next day."),
    ("I learned by answering, not by asking",
     "Most of what I know came from being the one who showed up when a cell "
     "went down — not from waiting for a training session to be "
     "scheduled."),
    ("Winning means the line runs",
     "My progress only counts if the cell keeps welding. That is the "
     "scoreboard I use on myself, and it is the one this plant is paid on."),
]:
    chev(s, 0.55, y + 0.04, size=0.2, n=2, gap=0.15)
    tb, tf = txbox(s, 1.1, y, 9.4, 0.4)
    para(tf, title, 19, LIME, bold=True, first=True)
    para(tf, body, 13.5, GRAYL, space_before=6, spacing=1.2)
    y += 1.42

rect(s, 0.5, 6.42, 12.33, 0.02, fill=RULE)
tb, tf = txbox(s, 0.5, 6.62, 12.33, 0.32)
para(tf, "Trust is the only currency an intern actually has.", 15, LIME,
     bold=True, italic=True, first=True)

notes[1] = (
    "The value I identify with is WIN. Not because it sounds ambitious, but "
    "because of how it actually works on the floor. Nobody hands a welding "
    "torch to an intern because of a title. You earn it. I earned mine by "
    "being the one who showed up when a cell went down, and the day the "
    "technicians started calling me instead of waiting for the engineer, that "
    "was the moment I felt I had won something real.")

# ============================================================ 3 · context
s = new("Standard_Light", 3, dark=False)
head(s, "Where I work", "Robotic GMAW welding — exhaust systems",
     dark=False)

CW, CSTEP = 2.8575, 3.1575
for i, (lbl, big, small) in enumerate([
        ("PRODUCT",   "Exhaust systems", "Clean Air business unit"),
        ("PROCESS",   "GMAW",            "MIG / MAG · robotic cells"),
        ("ROBOTS",    "Yaskawa",         "Most of the cells · one Fanuc"),
        ("EQUIPMENT", "SKS · Miller","Welding systems and controls")]):
    x = 0.5 + i * CSTEP
    rect(s, x, 2.48, CW, 2.38, fill=TINT)
    tb, tf = txbox(s, x + 0.3, 2.8, CW - 0.6, 0.3)
    para(tf, lbl, 9.5, BLUE, bold=True, first=True)
    para(tf, big, 20, NAVY, bold=True, space_before=10, spacing=1.05)
    para(tf, small, 11.5, MUTED, space_before=10, spacing=1.15)

rect(s, 0.5, 5.32, 12.33, 1.14, fill=NAVY)
tb, tf = txbox(s, 0.92, 5.56, 11.5, 0.7, anchor=MSO_ANCHOR.MIDDLE)
para(tf, "My zone has few welding cells and they run stable.", 15, LIME,
     bold=True, first=True)
para(tf, "That single fact shaped how my learning actually happened — "
         "I could not learn by waiting for them to fail.",
     13, WHITE, space_before=6)

notes[2] = (
    "Quick context. We weld exhaust systems, robotic GMAW, mostly Yaskawa "
    "robots with SKS and Miller equipment. There is one Fanuc I have not "
    "touched yet. The line at the bottom matters more than it looks: my zone "
    "has few welding cells and they are stable. That is good for the plant, "
    "but it means I could not learn by waiting for them to fail.")

# ============================================================ 4 · ILUO path
s = new("Standard_Light", 4, dark=False)
head(s, "The path I am expected to walk",
     "ILUO — 4 levels  ·  23 skills  ·  40 hours", dark=False)

for i, (letter, name, meaning, vol, ev) in enumerate([
        ("I", "Instructed", "I know the theory",
         "9 skills · 4 h",  "Written exam, min. 8.0"),
        ("L", "Learning",   "I do it with support",
         "5 skills · 8 h",  "Validation on the floor"),
        ("U", "Uses",       "I do it on my own",
         "3 skills · 8 h",  "Practical exam"),
        ("O", "Others",     "I teach it and improve it",
         "6 skills · 20 h", "Implementation review")]):
    x = 0.5 + i * CSTEP
    rect(s, x, 2.34, CW, 3.44, fill=NAVY)
    badge(s, x + 0.3, 2.66, 0.72, letter, LIME, NAVY, 26)
    tb, tf = txbox(s, x + 0.3, 3.66, CW - 0.6, 0.3)
    para(tf, name, 18, WHITE, bold=True, first=True)
    para(tf, meaning, 13, GRAYL, space_before=6, spacing=1.15)
    para(tf, vol, 12.5, LIME, bold=True, space_before=16)
    para(tf, ev, 11, MUTEDD, space_before=5, spacing=1.15)
    if i < 3:
        chev(s, x + CW + 0.04, 3.94, size=0.22, n=1)

tb, tf = txbox(s, 0.5, 6.08, 12.33, 0.5)
para(tf, "Each level assumes the one before it. Half of the total hours sit in "
         "level O — this program is built to multiply knowledge, not just "
         "to certify individuals.", 12.5, MUTED, first=True, spacing=1.2)

notes[3] = (
    "This is the ILUO path the plant defines for a welding technician. Four "
    "levels, twenty three specific skills, forty hours. I is knowing the "
    "theory. L is doing it with support. U is doing it alone. O is teaching it "
    "and improving the process. Notice that twenty of the forty hours sit in "
    "level O. This program is not designed to certify one person. It is "
    "designed so that one person multiplies.")

# ============================================================ 5 · scorecard
s = new("Standard_Light", 5, dark=False)
head(s, "Where I stand today",
     "4 of 23 skills unsupervised  ·  12 in progress  ·  7 not "
     "started", dark=False)

rows = [("I", "Welding fundamentals",  [S_WIP] * 9),
        ("L", "Execution in the cell", [S_DONE, S_DONE, S_DONE, S_WIP, S_WIP]),
        ("U", "Robot programming",     [S_WIP, S_NOT, S_NOT]),
        ("O", "Teach and improve",     [S_DONE] + [S_NOT] * 5)]
cw, ch, cg = 0.44, 0.34, 0.09
y = 2.5
for letter, label, states in rows:
    badge(s, 0.5, y - 0.05, 0.44, letter, NAVY, LIME, 15)
    tb, tf = txbox(s, 1.06, y + 0.04, 2.1, 0.3)
    para(tf, label, 11.5, NAVY, bold=True, first=True)
    bx = 3.3
    for st in states:
        rect(s, bx, y, cw, ch, fill=FILL[st])
        bx += cw + cg
    tb, tf = txbox(s, bx + 0.12, y + 0.06, 1.1, 0.3)
    para(tf, "%d skills" % len(states), 10.5, MUTED, first=True)
    y += 0.66

lx = 3.3
for lbl, st in (("Unsupervised", S_DONE), ("In progress", S_WIP),
                ("Not started", S_NOT)):
    rect(s, lx, 5.3, 0.2, 0.2, fill=FILL[st])
    tb, tf = txbox(s, lx + 0.3, 5.27, 1.6, 0.26)
    para(tf, lbl, 10.5, MUTED, first=True)
    lx += 1.6

rect(s, 8.72, 2.34, 4.11, 3.22, fill=TINT)
tb, tf = txbox(s, 9.04, 2.64, 3.47, 2.6)
para(tf, "WHAT I DO UNSUPERVISED", 9.5, BLUE, bold=True, first=True)
for it in ("Consumable changes — contact tip, liner, nozzle, diffuser, "
           "rollers",
           "Essential variable adjustment — current, wire feed, voltage, "
           "travel speed",
           "Welding symbol interpretation",
           "Closing ANDON orders"):
    para(tf, it, 12.5, NAVY, space_before=14, spacing=1.16)

rect(s, 0.5, 5.78, 12.33, 1.1, fill=NAVY)
tb, tf = txbox(s, 0.92, 5.98, 11.5, 0.75)
para(tf, "The honest part", 12, LIME, bold=True, first=True)
para(tf, "Level I content I picked up in the cell, not in a classroom. The "
         "theory session and the 8.0 written exam have not been delivered yet "
         "— so none of it is formally validated.",
     13, WHITE, space_before=6, spacing=1.16)

notes[4] = (
    "This is the honest picture. Each square is one skill. Four of the twenty "
    "three I execute with nobody standing next to me: consumable changes, "
    "essential variable adjustment, reading welding symbols, and closing ANDON "
    "orders. Twelve are in progress. Seven I have not started. And the line at "
    "the bottom is the part I want to be direct about. I know the level I "
    "content because I picked it up in the cell, but the theory session and "
    "the exam have not happened, so formally I am validated at zero. That is "
    "not a small detail and I am not going to hide it.")

# ============================================================ 6 · plan vs reality
s = new("Standard_Dark", 6, dark=True)
head(s, "The plan and the reality", None, dark=True)

for i, (title, col, items) in enumerate([
        ("WHAT THE PLAN ASSUMED", MUTEDD,
         ["40 scheduled hours", "Classroom first, then the cell",
          "One level at a time, in order", "A formal exam at every gate"]),
        ("WHAT ACTUALLY HAPPENED", LIME,
         ["Hours driven by demand, not by schedule",
          "Straight to the floor from day one",
          "Levels touched out of order", "No formal evaluation yet"])]):
    x = 0.5 + i * 6.31
    rect(s, x, 1.76, 6.02, 2.72, fill=CARD)
    tb, tf = txbox(s, x + 0.36, 2.04, 5.3, 2.2)
    para(tf, title, 10, col, bold=True, first=True)
    for it in items:
        para(tf, it, 14, WHITE, space_before=14)

rect(s, 0.5, 4.76, 7.7, 1.0, fill=LIME)
tb, tf = txbox(s, 0.86, 4.96, 7.0, 0.6, anchor=MSO_ANCHOR.MIDDLE)
para(tf, "60% integration project   │   40% breakdowns and support",
     14.5, NAVY, bold=True, first=True)
tb, tf = txbox(s, 8.5, 4.78, 4.33, 1.0)
para(tf, "HOW MY TIME REALLY SPLIT", 9.5, LIME, bold=True, first=True)
para(tf, "Most of my hours went into an integration project with my engineer "
         "— work that is not on the welding matrix at all.",
     12.5, GRAYL, space_before=7, spacing=1.16)

rect(s, 0.5, 6.18, 12.33, 0.02, fill=RULE)
tb, tf = txbox(s, 0.5, 6.4, 12.33, 0.4)
para(tf, "This is not a gap in effort. It is a gap in structure — and it "
         "is fixable before January.", 16, WHITE, bold=True, first=True)

notes[5] = (
    "Now the part that explains the previous slide. The plan assumed forty "
    "scheduled hours, classroom first, one level at a time. What happened was "
    "different. We went straight to the floor, we learned what the day "
    "demanded, and about sixty percent of my hours went into an integration "
    "project with my engineer, which is not on the welding matrix at all. I "
    "want to be clear about how I am framing this. It is not a gap in effort. "
    "It is a gap in structure, and structure is something we can fix in the "
    "time I have left.")

# ============================================================ 7 · integration
s = new("Standard_Light", 7, dark=False)
head(s, "The integration project",
     "Where 60% of my time went — and none of it is on the ILUO matrix",
     dark=False)
y = 2.42
for lbl, val in [("WHAT IT IS",        "[ describe the integration in one line ]"),
                 ("MY ROLE",           "[ what you own day to day ]"),
                 ("WHAT IT TAUGHT ME", "[ three skills outside the welding matrix ]"),
                 ("STATUS",            "[ % complete · next milestone ]")]:
    rect(s, 0.5, y, 12.33, 0.94, fill=TINT)
    tb, tf = txbox(s, 0.9, y + 0.2, 2.6, 0.55, anchor=MSO_ANCHOR.MIDDLE)
    para(tf, lbl, 10, BLUE, bold=True, first=True)
    tb, tf = txbox(s, 3.6, y + 0.2, 8.8, 0.55, anchor=MSO_ANCHOR.MIDDLE)
    para(tf, val, 15, NAVY, first=True)
    y += 1.08

tb, tf = txbox(s, 0.5, 6.84, 12.33, 0.3)
para(tf, "Worked directly with my engineering lead — this is the "
         "experience the matrix does not capture.", 12.5, MUTED, first=True)

notes[6] = (
    "This is the project that took most of my time. Explain it here in your "
    "own words: what the integration is, what you own day to day, what it "
    "taught you that is not welding, and where it stands. Keep it to about "
    "ninety seconds. The reason it belongs in this presentation is that it is "
    "real engineering work — it just does not appear anywhere on the "
    "ILUO matrix.")

# ============================================================ 8 · gantt
s = new("Standard_Light", 8, dark=False)
head(s, "Timeline", "August 1, 2026  →  January 1, 2027", dark=False)

GX, GW, GY, GH = 3.62, 9.2, 2.56, 3.7
mw = GW / 5.0
def at(month):                      # 0.0 = Aug 1 · 5.0 = Jan 1
    return GX + month * mw

for i, m in enumerate(["AUG", "SEP", "OCT", "NOV", "DEC"]):
    tb, tf = txbox(s, at(i), GY - 0.32, mw, 0.24, align=PP_ALIGN.CENTER)
    para(tf, m, 10, MUTED, bold=True, first=True)
    if i:
        rect(s, at(i), GY - 0.04, 0.008, GH, fill=RGBColor(0xE6, 0xEA, 0xEC))

TODAY = 1.8                         # September 25
by, bh = GY + 0.18, 0.4
for label, meta, segs in [
        ("Level I — welding fundamentals", "9 skills · informal so far",
         [(0.0, TODAY, S_DONE), (2.0, 3.0, S_NOT)]),
        ("Level L — execution in the cell", "5 skills",
         [(0.0, TODAY, S_DONE), (TODAY, 4.0, S_NOT)]),
        ("Level U — robot programming", "3 skills",
         [(2.0, 4.7, S_NOT)]),
        ("Level O — ANDON and improvement", "6 skills",
         [(0.0, TODAY, S_DONE), (3.0, 4.7, S_NOT)]),
        ("Integration project", "off-matrix",
         [(0.0, TODAY, S_DONE), (TODAY, 4.0, S_NOT)])]:
    tb, tf = txbox(s, 0.5, by + 0.02, 3.0, 0.5)
    para(tf, label, 11.5, NAVY, bold=True, first=True, spacing=1.1)
    para(tf, meta, 9.5, MUTED, space_before=2)
    for a, b, st in segs:
        rect(s, at(a), by, at(b) - at(a), bh, fill=FILL[st])
    by += 0.72

rect(s, at(TODAY) - 0.015, GY - 0.04, 0.03, GH, fill=LIME)
tb, tf = txbox(s, at(TODAY) - 0.56, GY - 0.62, 1.12, 0.24, align=PP_ALIGN.CENTER)
para(tf, "TODAY", 9.5, NAVY, bold=True, first=True)

lx = 3.62
for lbl, st in (("Actual", S_DONE), ("Planned", S_NOT)):
    rect(s, lx, 6.36, 0.2, 0.2, fill=FILL[st])
    tb, tf = txbox(s, lx + 0.3, 6.33, 1.6, 0.26)
    para(tf, lbl, 10.5, MUTED, first=True)
    lx += 1.6

tb, tf = txbox(s, 0.5, 6.82, 12.33, 0.3)
para(tf, "14 weeks left. Levels U and O are the ones still fully ahead of me.",
     12.5, MUTED, first=True)

notes[7] = (
    "Here is the same story on a calendar. The lime line is today, September "
    "twenty fifth. Everything to the left is what actually happened: level I "
    "content and level L execution built up on the floor, ANDON closings, and "
    "the integration project running alongside all of it. Everything to the "
    "right is what I am proposing. The level I exam in October, level L closed "
    "by the end of November, level U programming from October to December, and "
    "one level O improvement before I finish. Fourteen weeks left.")

# ============================================================ 9 · results
s = new("Standard_Dark", 9, dark=True)
head(s, "Results to date", None, dark=True)

for i, (big, lbl) in enumerate([("8",       "weeks on the floor"),
                                ("4 / 23",  "skills unsupervised"),
                                ("12 / 23", "skills in progress"),
                                ("60%",     "of my time on the integration "
                                            "project")]):
    x = 0.5 + i * CSTEP
    rect(s, x, 1.72, CW, 1.78, fill=CARD)
    tb, tf = txbox(s, x + 0.3, 1.98, CW - 0.6, 0.8)
    para(tf, big, 40, LIME, bold=True, first=True)
    para(tf, lbl, 12, GRAYL, space_before=8, spacing=1.14)

tb, tf = txbox(s, 0.5, 4.0, 12.33, 0.3)
para(tf, "WHAT CHANGED IN EIGHT WEEKS", 10, LIME, bold=True, first=True)

y = 4.46
for i, (topic, before, after) in enumerate([
        ("Consumable change", "I watched someone do it", "I do it alone"),
        ("Welding symbols", "I could not read a joint callout",
         "I read the print and find the joint"),
        ("ANDON", "I was the one who called it in",
         "I am the one who closes it")]):
    tb, tf = txbox(s, 0.5, y, 2.7, 0.3)
    para(tf, topic, 13, WHITE, bold=True, first=True)
    tb, tf = txbox(s, 3.3, y, 4.0, 0.3)
    para(tf, before, 12.5, MUTEDD, first=True)
    chev(s, 7.5, y + 0.02, size=0.16, n=2, gap=0.13)
    tb, tf = txbox(s, 8.18, y, 4.65, 0.3)
    para(tf, after, 12.5, LIME, bold=True, first=True)
    if i < 2:
        rect(s, 0.5, y + 0.5, 12.33, 0.008, fill=RULE)
    y += 0.84

notes[8] = (
    "Eight weeks on the floor. Four skills I own unsupervised, twelve in "
    "progress. But the numbers are not the part I am proud of. The bottom "
    "three lines are. Two months ago I watched someone change consumables, now "
    "I do it alone. Two months ago I could not read a joint callout on a "
    "print, now I find the joint. And two months ago I was the intern who "
    "called the ANDON. Now I am the one who closes it.")

# ============================================================ 10 · the ask
s = new("Standard_Light", 10, dark=False)
head(s, "What is missing, and how I close it",
     "Four commitments before January 1", dark=False)

for x, w, lbl in ((1.18, 4.0, "COMMITMENT"), (5.3, 3.4, "WHAT IT UNLOCKS"),
                  (8.9, 1.5, "WHEN"), (10.5, 2.4, "WHAT I NEED")):
    tb, tf = txbox(s, x, 2.3, w, 0.26)
    para(tf, lbl, 9, BLUE, bold=True, first=True)

y = 2.7
for i, (a, b, c, d) in enumerate([
        ("Sit the Level I theory session and the written exam",
         "Level I formally validated", "October", "Two half-days scheduled"),
        ("Document 10 parameter validations against the WPS",
         "Closes Level L", "Oct – Nov", "Access to parameter sheets"),
        ("Create one welding program in SKS, supervised",
         "Opens Level U", "November", "Cell time and supervision"),
        ("Take one improvement idea to implementation",
         "First Level O evidence", "Nov – Dec", "A sponsor for the idea")]):
    if i % 2 == 0:
        rect(s, 0.5, y - 0.11, 12.33, 0.94, fill=TINT)
    badge(s, 0.62, y + 0.1, 0.4, str(i + 1), NAVY, LIME, 14)
    for x, w, txt, sz, col, bd in (
            (1.18, 3.95, a, 13,   NAVY,  True),
            (5.3,  3.4,  b, 12.5, NAVY,  False),
            (8.9,  1.5,  c, 12.5, BLUE,  True),
            (10.5, 2.4,  d, 12,   MUTED, False)):
        tb, tf = txbox(s, x, y + 0.1, w, 0.62, anchor=MSO_ANCHOR.MIDDLE)
        para(tf, txt, sz, col, bold=bd, first=True, spacing=1.14)
    y += 0.99

tb, tf = txbox(s, 0.5, 6.72, 12.33, 0.3)
para(tf, "None of this needs budget. It needs calendar.", 13, NAVY, bold=True,
     first=True)

notes[9] = (
    "So here is what I am asking for, and it is small. Four things. Give me "
    "two half-days for the level I session and the exam, and the theory stops "
    "being informal. Give me access to the parameter sheets and I will "
    "document ten validations, which closes level L. Give me supervised cell "
    "time and I will build one program in SKS, which opens level U. And give "
    "me a sponsor for one improvement idea and I leave you my first piece of "
    "level O evidence. None of this needs budget. It needs calendar.")

# ============================================================ 11 · conclusions
s = new("Standard_Dark", 11, dark=True)
head(s, "Conclusions", None, dark=True)
y = 1.86
for i, (t, d) in enumerate([
        ("I am real at Level L, and honest about Level I.",
         "Four skills I execute with nobody watching. But nothing is formally "
         "validated yet, and that is the gap that actually matters."),
        ("The floor taught me faster than the schedule would have.",
         "Demand-driven learning built troubleshooting judgment a classroom "
         "does not give you. It just did not produce paperwork."),
        ("Fourteen weeks left, and a plan for them.",
         "The four commitments take me from informally capable to formally "
         "certified before January 1.")]):
    tb, tf = txbox(s, 0.5, y, 0.8, 0.5)
    para(tf, "0%d" % (i + 1), 26, LIME, bold=True, first=True)
    tb, tf = txbox(s, 1.42, y + 0.02, 10.6, 0.5)
    para(tf, t, 20, WHITE, bold=True, first=True)
    para(tf, d, 13.5, GRAYL, space_before=7, spacing=1.2)
    y += 1.56

rect(s, 0.5, 6.42, 12.33, 0.02, fill=RULE)
tb, tf = txbox(s, 0.5, 6.62, 12.33, 0.34)
para(tf, "Trust first, then the torch. That is how I read WIN.", 15, LIME,
     bold=True, italic=True, first=True)

notes[10] = (
    "Three conclusions. First, I am genuinely capable at level L and I am "
    "being honest that level I is not validated. Second, learning on the floor "
    "made me faster at diagnosing problems than a classroom would have, it "
    "just did not generate paperwork. Third, I have fourteen weeks and a "
    "concrete plan for them. And if I go back to the value I picked: trust "
    "first, then the torch. That is what these two months taught me.")

# ============================================================ 12 · closing
# the Closing layout carries a large centred logo at y 3.39-4.10 - leave it clear
s = new("Closing Slide", 12, dark=True, footer=False)
tb, tf = txbox(s, 0.0, 1.72, SW, 0.8, align=PP_ALIGN.CENTER)
para(tf, "Thank you", 44, WHITE, bold=True, first=True)
tb, tf = txbox(s, 0.0, 2.62, SW, 0.34, align=PP_ALIGN.CENTER)
para(tf, "Questions?", 17, LIME, bold=True, first=True)
tb, tf = txbox(s, 0.0, 4.56, SW, 0.62, align=PP_ALIGN.CENTER)
para(tf, "Diego Adair de León Márquez", 15, WHITE, first=True,
     align=PP_ALIGN.CENTER)
para(tf, "Robotic Welding  ·  Engineering  ·  Tenneco Aguascalientes",
     11.5, GRAYL, space_before=6, align=PP_ALIGN.CENTER)

notes[11] = (
    "Thank you. I am happy to take questions, and if anyone wants the detail "
    "behind any of the twenty three skills, I can walk through it.")

# ---------------------------------------------------------------- notes
for i, slide in enumerate(prs.slides):
    if i in notes:
        slide.notes_slide.notes_text_frame.text = notes[i]

prs.save(OUT)
print("saved", OUT)
