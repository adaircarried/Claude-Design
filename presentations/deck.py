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
from pptx.oxml.ns import qn
from pptx.oxml import parse_xml

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

def head(s, eyebrow, title, sub, dark):
    """Message title: the slide asserts its conclusion, the eyebrow says which
    section we are in, the support line carries the evidence."""
    dash(s, 0.53, 0.62)
    tb, tf = txbox(s, 0.5, 0.78, 12.4, 0.26)
    para(tf, eyebrow, 10, MUTEDD if dark else MUTED, bold=True, first=True)
    if title:
        tb, tf = txbox(s, 0.5, 1.02, 12.4, 0.5)
        para(tf, title, 30, LIME if dark else NAVY, bold=True, first=True)
    if sub:
        tb, tf = txbox(s, 0.5, 1.62, 11.6, 0.34)
        para(tf, sub, 13, MUTEDD if dark else MUTED, first=True)

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
# Deliberately almost empty: one word, one definition, one line of his own.
s = new("Standard_Dark", 2, dark=True)
head(s, "OUR VALUES", None, None, dark=True)

tb, tf = txbox(s, 0.5, 1.58, 8.0, 1.6)
para(tf, "WIN", 108, LIME, bold=True, first=True)

tb, tf = txbox(s, 0.56, 3.5, 10.4, 0.4)
para(tf, "“We must earn the trust of our employees and customers.”",
     17, GRAYL, italic=True, first=True)

rect(s, 0.5, 4.5, 12.33, 0.02, fill=RULE)
tb, tf = txbox(s, 0.56, 4.96, 11.4, 0.9)
para(tf, "Nobody hands an intern a welding torch.", 26, WHITE, bold=True,
     first=True)
para(tf, "You earn it.", 26, LIME, bold=True, space_before=6)

notes[1] = (
    "The value I identify with is WIN, and I want to explain why in one idea. "
    "On the floor, nobody lets an intern touch a welding cell because of a job "
    "title. You earn it. I earned mine by being the one who showed up when a "
    "cell went down, not by waiting for a training session to be scheduled. "
    "And the day the technicians started calling me instead of waiting for the "
    "engineer, that was the moment I felt I had won something real. Winning "
    "here is not about being right. It is about the line running.")

# ============================================================ 3 · summary
# Bottom line up front: if the room only hears three minutes, it hears this.
s = new("Standard_Light", 3, dark=False)
head(s, "EXECUTIVE SUMMARY", "Capable on the floor, not yet on paper", None,
     dark=False)

SW3, SS3 = 3.95, 4.19
for i, (label, big, body) in enumerate([
        ("WHERE I STAND", "4 of 23", "skills I run unsupervised"),
        ("WHAT HAPPENED", "60%", "of my hours went to an integration project"),
        ("WHAT I NEED", "4", "commitments, and none of them cost money")]):
    x = 0.5 + i * SS3
    rect(s, x, 2.24, SW3, 2.3, fill=NAVY)
    tb, tf = txbox(s, x + 0.36, 2.56, SW3 - 0.72, 1.7)
    para(tf, label, 10, MUTEDD, bold=True, first=True)
    para(tf, big, 40, LIME, bold=True, space_before=10)
    para(tf, body, 14, GRAYL, space_before=14, spacing=1.2)

rect(s, 0.5, 5.06, 12.33, 1.1, fill=LIME)
tb, tf = txbox(s, 0.92, 5.28, 11.5, 0.7, anchor=MSO_ANCHOR.MIDDLE)
para(tf, "By January 1 I can be certified at Levels I and L.", 20, NAVY,
     bold=True, first=True)

notes[2] = (
    "Before anything else, here is the whole presentation in three numbers, in "
    "case we run short on time. Four of the twenty three skills I do "
    "unsupervised, and none of them are formally validated. Sixty percent of "
    "my hours went to an integration project instead of to the matrix. And I "
    "need four things from you before January, none of which cost money. If "
    "you only remember one line today, make it the green one: by January "
    "first I can be certified at levels I and L, if those four commitments get "
    "a date on a calendar.")

# ============================================================ 4 · context
s = new("Standard_Light", 4, dark=False)
head(s, "WHERE I WORK", "Few welding cells, and they run stable", None,
     dark=False)

CW, CSTEP = 2.8575, 3.1575
for i, (lbl, big) in enumerate([("PRODUCT",   "Exhaust systems"),
                                ("PROCESS",   "GMAW (MIG/MAG)"),
                                ("ROBOTS",    "Yaskawa"),
                                ("EQUIPMENT", "SKS, Miller")]):
    x = 0.5 + i * CSTEP
    rect(s, x, 2.3, CW, 2.0, fill=TINT)
    tb, tf = txbox(s, x + 0.3, 2.66, CW - 0.6, 1.3)
    para(tf, lbl, 9.5, BLUE, bold=True, first=True)
    para(tf, big, 20, NAVY, bold=True, space_before=12, spacing=1.08)

rect(s, 0.5, 4.86, 12.33, 1.3, fill=NAVY)
tb, tf = txbox(s, 0.92, 5.16, 11.5, 0.7, anchor=MSO_ANCHOR.MIDDLE)
para(tf, "So I could not learn by waiting for a breakdown.", 20, LIME,
     bold=True, first=True)

notes[3] = (
    "Quick context. We weld exhaust systems, robotic GMAW, mostly Yaskawa "
    "robots with SKS and Miller equipment. There is one Fanuc I have not "
    "touched yet. The line at the bottom is the one that matters: my zone has "
    "few welding cells and they are stable. That is good news for the plant, "
    "but it means I could not learn by waiting for something to break. My "
    "hours went wherever the work was, and that is exactly why my progress "
    "looks the way it does on the next slides.")

# ============================================================ 5 · ILUO path
s = new("Standard_Light", 5, dark=False)
head(s, "THE ILUO SKILL PATH", "Four levels, and half the hours are for teaching",
     None, dark=False)

for i, (letter, name, meaning, vol) in enumerate([
        ("I", "Instructed", "I know the theory",         "9 skills · 4 h"),
        ("L", "Learning",   "I do it with support",      "5 skills · 8 h"),
        ("U", "Uses",       "I do it on my own",         "3 skills · 8 h"),
        ("O", "Others",     "I teach it and improve it", "6 skills · 20 h")]):
    x = 0.5 + i * CSTEP
    rect(s, x, 2.24, CW, 3.06, fill=NAVY)
    b = badge(s, x + 0.3, 2.56, 0.78, letter, LIME, NAVY, 28)
    b.name = "!!iluo_%s" % letter          # morph anchor into the next slide
    tb, tf = txbox(s, x + 0.3, 3.66, CW - 0.6, 1.4)
    para(tf, name, 19, WHITE, bold=True, first=True)
    para(tf, meaning, 13.5, GRAYL, space_before=8, spacing=1.15)
    para(tf, vol, 13, LIME, bold=True, space_before=16)
    if i < 3:
        chev(s, x + CW + 0.04, 3.86, size=0.22, n=1)

tb, tf = txbox(s, 0.5, 5.66, 12.33, 0.5)
para(tf, "Every level has its own gate: a written exam, a validation on the "
         "floor, a practical exam, an implementation review.",
     13, MUTED, first=True, spacing=1.2)

notes[4] = (
    "This is the path the plant defines for a welding technician. Four levels, "
    "twenty three skills, forty hours. I is knowing the theory. L is doing it "
    "with support. U is doing it alone. O is teaching it and improving the "
    "process. Two things worth noticing. First, each level assumes the one "
    "before it. Second, twenty of the forty hours sit in level O. This program "
    "is not designed to certify one person. It is designed so that one person "
    "multiplies.")

# ============================================================ 6 · scorecard
s = new("Standard_Light", 6, dark=False)
head(s, "WHERE I STAND TODAY",
     "Four skills unsupervised, none of them validated", None, dark=False)

rows = [("I", "Welding fundamentals",  [S_WIP] * 9),
        ("L", "Execution in the cell", [S_DONE, S_DONE, S_DONE, S_WIP, S_WIP]),
        ("U", "Robot programming",     [S_WIP, S_NOT, S_NOT]),
        ("O", "Teach and improve",     [S_DONE] + [S_NOT] * 5)]
cw, ch, cg = 0.44, 0.36, 0.09
y = 2.42
for letter, label, states in rows:
    b = badge(s, 0.5, y - 0.04, 0.44, letter, NAVY, LIME, 15)
    b.name = "!!iluo_%s" % letter          # morphs in from the previous slide
    tb, tf = txbox(s, 1.06, y + 0.06, 2.1, 0.3)
    para(tf, label, 12, NAVY, bold=True, first=True)
    bx = 3.3
    for st in states:
        rect(s, bx, y, cw, ch, fill=FILL[st])
        bx += cw + cg
    y += 0.7

lx = 3.3
for lbl, st in (("Unsupervised", S_DONE), ("In progress", S_WIP),
                ("Not started", S_NOT)):
    rect(s, lx, 5.34, 0.2, 0.2, fill=FILL[st])
    tb, tf = txbox(s, lx + 0.3, 5.31, 1.6, 0.26)
    para(tf, lbl, 10.5, MUTED, first=True)
    lx += 1.6

rect(s, 8.72, 2.26, 4.11, 2.92, fill=TINT)
tb, tf = txbox(s, 9.04, 2.58, 3.47, 2.3)
para(tf, "WHAT I DO UNSUPERVISED", 9.5, BLUE, bold=True, first=True)
for it in ("Consumable changes", "Essential variable adjustment",
           "Welding symbol interpretation", "Closing ANDON orders"):
    para(tf, it, 14, NAVY, space_before=18)

rect(s, 0.5, 5.86, 12.33, 1.06, fill=NAVY)
tb, tf = txbox(s, 0.92, 6.1, 11.5, 0.62, anchor=MSO_ANCHOR.MIDDLE)
para(tf, "I learned Level I in the cell, not in a classroom. The exam has not "
         "happened, so nothing is validated yet.", 15, WHITE, first=True,
     spacing=1.16)

notes[5] = (
    "Each square is one skill. Four of the twenty three I execute with nobody "
    "standing next to me: consumable changes, which means contact tip, liner, "
    "nozzle, diffuser and rollers; essential variable adjustment, so current, "
    "wire feed, voltage and travel speed; reading welding symbols; and closing "
    "ANDON orders. Twelve are in progress. Seven I have not started. And then "
    "the line at the bottom, which is the part I want to be direct about. I "
    "know the level I content because I picked it up in the cell, but the "
    "theory session and the exam have not happened, so formally I am validated "
    "at zero. I am not going to hide that.")

# ============================================================ 7 · plan vs reality
s = new("Standard_Dark", 7, dark=True)
head(s, "PLAN VS REALITY", "Not a gap in effort, a gap in structure", None,
     dark=True)

for i, (title, col, items) in enumerate([
        ("WHAT THE PLAN ASSUMED", MUTEDD,
         ["40 scheduled hours", "Classroom, then the cell",
          "One level at a time"]),
        ("WHAT ACTUALLY HAPPENED", LIME,
         ["Hours driven by demand", "The floor from day one",
          "Levels out of order"])]):
    x = 0.5 + i * 6.31
    rect(s, x, 2.14, 6.02, 2.5, fill=CARD)
    tb, tf = txbox(s, x + 0.36, 2.46, 5.3, 1.9)
    para(tf, title, 10, col, bold=True, first=True)
    for it in items:
        para(tf, it, 16, WHITE, space_before=18)

rect(s, 0.5, 5.12, 12.33, 1.2, fill=LIME)
rect(s, 6.66, 5.42, 0.02, 0.6, fill=NAVY)
tb, tf = txbox(s, 0.92, 5.42, 5.5, 0.6, anchor=MSO_ANCHOR.MIDDLE)
para(tf, "60% integration project", 20, NAVY, bold=True, first=True)
tb, tf = txbox(s, 6.94, 5.42, 5.6, 0.6, anchor=MSO_ANCHOR.MIDDLE)
para(tf, "40% breakdowns and support", 20, NAVY, bold=True, first=True)

notes[6] = (
    "This is what explains the previous slide. The plan assumed forty "
    "scheduled hours, classroom first, one level at a time. What happened was "
    "different. We went straight to the floor, we learned what the day "
    "demanded, and the levels got touched out of order. Sixty percent of my "
    "hours went into an integration project with my engineer, which is not on "
    "the welding matrix at all. I want to be careful about how I frame this. "
    "It is not a gap in effort. Every hour I worked was useful. It is a gap in "
    "structure, and structure is the part you can unblock.")

# ============================================================ 8 · integration
s = new("Standard_Light", 8, dark=False)
head(s, "THE INTEGRATION PROJECT", "60% of my hours, and none of it on the matrix",
     None, dark=False)
y = 2.4
for lbl, val in [("WHAT IT IS",        "[ describe the integration in one line ]"),
                 ("MY ROLE",           "[ what you own day to day ]"),
                 ("WHAT IT TAUGHT ME", "[ three skills outside the welding matrix ]"),
                 ("STATUS",            "[ % complete, next milestone ]")]:
    rect(s, 0.5, y, 12.33, 1.0, fill=TINT)
    tb, tf = txbox(s, 0.9, y + 0.22, 2.6, 0.56, anchor=MSO_ANCHOR.MIDDLE)
    para(tf, lbl, 10, BLUE, bold=True, first=True)
    tb, tf = txbox(s, 3.6, y + 0.22, 8.8, 0.56, anchor=MSO_ANCHOR.MIDDLE)
    para(tf, val, 16, NAVY, first=True)
    y += 1.14

notes[7] = (
    "This is the project that took most of my time, and I worked on it "
    "directly with my engineering lead. Explain it here in your own words: "
    "what the integration is, what you own day to day, what it taught you that "
    "is not welding, and where it stands right now. Keep it to about ninety "
    "seconds. The reason it belongs in this presentation is simple. It is real "
    "engineering work. It just does not appear anywhere on the ILUO matrix, so "
    "the scorecard you saw two slides ago cannot see it.")

# ============================================================ 9 · gantt
s = new("Standard_Light", 9, dark=False)
head(s, "TIMELINE", "14 weeks left, and Levels U and O are still ahead",
     "August 1, 2026  →  January 1, 2027", dark=False)

GX, GW, GY, GH = 3.62, 9.2, 2.68, 3.66
mw = GW / 5.0
def at(month):                      # 0.0 = Aug 1, 5.0 = Jan 1
    return GX + month * mw

for i, m in enumerate(["AUG", "SEP", "OCT", "NOV", "DEC"]):
    tb, tf = txbox(s, at(i), GY - 0.32, mw, 0.24, align=PP_ALIGN.CENTER)
    para(tf, m, 10, MUTED, bold=True, first=True)
    if i:
        rect(s, at(i), GY - 0.04, 0.008, GH, fill=RGBColor(0xE6, 0xEA, 0xEC))

TODAY = 1.8                         # September 25
by, bh = GY + 0.22, 0.42
for label, segs in [
        ("Level I: welding fundamentals",
         [(0.0, TODAY, S_DONE), (2.0, 3.0, S_NOT)]),
        ("Level L: execution in the cell",
         [(0.0, TODAY, S_DONE), (TODAY, 4.0, S_NOT)]),
        ("Level U: robot programming", [(2.0, 4.7, S_NOT)]),
        ("Level O: ANDON and improvement",
         [(0.0, TODAY, S_DONE), (3.0, 4.7, S_NOT)]),
        ("Integration project",
         [(0.0, TODAY, S_DONE), (TODAY, 4.0, S_NOT)])]:
    tb, tf = txbox(s, 0.5, by + 0.08, 3.0, 0.4)
    para(tf, label, 12, NAVY, bold=True, first=True, spacing=1.1)
    for a, b, st in segs:
        rect(s, at(a), by, at(b) - at(a), bh, fill=FILL[st])
    by += 0.72

rect(s, at(TODAY) - 0.015, GY - 0.04, 0.03, GH, fill=LIME)
tb, tf = txbox(s, at(TODAY) - 0.56, GY - 0.62, 1.12, 0.24, align=PP_ALIGN.CENTER)
para(tf, "TODAY", 9.5, NAVY, bold=True, first=True)

lx = 3.62
for lbl, st in (("Actual", S_DONE), ("Planned", S_NOT)):
    rect(s, lx, 6.5, 0.2, 0.2, fill=FILL[st])
    tb, tf = txbox(s, lx + 0.3, 6.47, 1.6, 0.26)
    para(tf, lbl, 10.5, MUTED, first=True)
    lx += 1.6

notes[8] = (
    "The same story on a calendar. The green line is today, September twenty "
    "fifth. Everything to its left actually happened: level I content and "
    "level L execution built up in the cell, ANDON orders closed, and the "
    "integration project running alongside all of it. Everything to the right "
    "is what I am proposing. The level I exam in October, level L closed by "
    "the end of November, level U programming from October to December, and "
    "one level O improvement before I finish. Level I needs a scheduled "
    "session and level U needs supervised cell time. Both are calendar items, "
    "not budget items.")

# ============================================================ 10 · results
s = new("Standard_Dark", 10, dark=True)
head(s, "RESULTS TO DATE", "From watching the cell to closing the ANDON", None,
     dark=True)

for i, (big, lbl) in enumerate([("8",       "weeks on the floor"),
                                ("4 of 23", "unsupervised"),
                                ("12",      "in progress"),
                                ("60%",     "on the integration project")]):
    x = 0.5 + i * CSTEP
    rect(s, x, 2.14, CW, 1.6, fill=CARD)
    tb, tf = txbox(s, x + 0.3, 2.42, CW - 0.6, 1.0)
    para(tf, big, 38, LIME, bold=True, first=True)
    para(tf, lbl, 12.5, GRAYL, space_before=10, spacing=1.14)

tb, tf = txbox(s, 0.5, 4.22, 12.33, 0.3)
para(tf, "WHAT CHANGED IN EIGHT WEEKS", 10, LIME, bold=True, first=True)

y = 4.7
for i, (topic, before, after) in enumerate([
        ("Consumable change", "I watched someone do it", "I do it alone"),
        ("Welding symbols", "I could not read a joint callout",
         "I read the print and find the joint"),
        ("ANDON", "I was the one who called it in",
         "I am the one who closes it")]):
    tb, tf = txbox(s, 0.5, y, 2.7, 0.3)
    para(tf, topic, 13.5, WHITE, bold=True, first=True)
    tb, tf = txbox(s, 3.3, y, 4.0, 0.3)
    para(tf, before, 13, MUTEDD, first=True)
    chev(s, 7.5, y + 0.03, size=0.16, n=2, gap=0.13)
    tb, tf = txbox(s, 8.18, y, 4.65, 0.3)
    para(tf, after, 13, LIME, bold=True, first=True)
    if i < 2:
        rect(s, 0.5, y + 0.52, 12.33, 0.008, fill=RULE)
    y += 0.86

notes[9] = (
    "Eight weeks on the floor. Four skills I own unsupervised, twelve in "
    "progress, and sixty percent of the time on the integration project. But "
    "the numbers are not the part I am proud of. The bottom three lines are. "
    "Two months ago I watched someone change consumables, now I do it alone. "
    "Two months ago I could not read a joint callout on a print, now I find "
    "the joint. And two months ago I was the intern who called in the ANDON. "
    "Now I am the one who closes it.")

# ============================================================ 11 · the ask
s = new("Standard_Light", 11, dark=False)
head(s, "WHAT I NEED FROM YOU", "Four commitments, and none of them cost money",
     None, dark=False)

for x, w, lbl in ((1.18, 5.2, "COMMITMENT"), (6.8, 3.6, "WHAT I NEED"),
                  (10.9, 1.9, "WHEN")):
    tb, tf = txbox(s, x, 2.3, w, 0.26)
    para(tf, lbl, 9, BLUE, bold=True, first=True)

y = 2.68
for i, (a, unlock, need, when) in enumerate([
        ("Level I theory session and written exam", "Validates Level I",
         "Two half-days on the calendar", "October"),
        ("Document parameter validations vs. the WPS", "Closes Level L",
         "Access to the parameter sheets", "Oct – Nov"),
        ("Create one welding program in SKS, supervised", "Opens Level U",
         "Cell time and supervision", "November"),
        ("Take one improvement idea to implementation",
         "First Level O evidence", "A sponsor for the idea", "Nov – Dec")]):
    if i % 2 == 0:
        rect(s, 0.5, y - 0.13, 12.33, 0.98, fill=TINT)
    badge(s, 0.62, y + 0.12, 0.4, str(i + 1), NAVY, LIME, 14)
    tb, tf = txbox(s, 1.18, y, 5.2, 0.8)
    para(tf, a, 14, NAVY, bold=True, first=True, spacing=1.14)
    para(tf, unlock, 11, BLUE, space_before=4)
    tb, tf = txbox(s, 6.8, y + 0.14, 3.6, 0.5, anchor=MSO_ANCHOR.MIDDLE)
    para(tf, need, 13, MUTED, first=True, spacing=1.14)
    tb, tf = txbox(s, 10.9, y + 0.14, 1.9, 0.5, anchor=MSO_ANCHOR.MIDDLE)
    para(tf, when, 13, BLUE, bold=True, first=True)
    y += 1.0

tb, tf = txbox(s, 0.5, 6.78, 12.33, 0.3)
para(tf, "Approve these four and I finish the program certified, not just "
         "experienced.", 14, NAVY, bold=True, first=True)

notes[10] = (
    "So here is the ask, and it is small. Four things. Give me two half-days "
    "for the level I session and the exam, and the theory stops being "
    "informal. Give me access to the parameter sheets and I will document ten "
    "validations, which closes level L. Give me supervised cell time and I "
    "will build one program in SKS, which opens level U. And give me a sponsor "
    "for one improvement idea and I leave you my first piece of level O "
    "evidence. Every one of these produces a document you can audit. None of "
    "them needs budget. They need calendar.")

# ============================================================ 12 · conclusions
s = new("Standard_Dark", 12, dark=True)
head(s, "CONCLUSIONS", "Informally capable now, certified by January", None,
     dark=True)
y = 2.26
for i, t in enumerate([
        "I am real at Level L, and honest about Level I.",
        "The floor taught me faster than the schedule would have.",
        "Fourteen weeks left, and a plan for them."]):
    tb, tf = txbox(s, 0.5, y, 0.9, 0.5)
    para(tf, "0%d" % (i + 1), 28, LIME, bold=True, first=True)
    tb, tf = txbox(s, 1.52, y + 0.06, 10.8, 0.5)
    para(tf, t, 22, WHITE, bold=True, first=True)
    y += 1.24

rect(s, 0.5, 6.22, 12.33, 0.02, fill=RULE)
tb, tf = txbox(s, 0.5, 6.46, 12.33, 0.34)
para(tf, "Trust first, then the torch. That is how I read WIN.", 17, LIME,
     bold=True, italic=True, first=True)

notes[11] = (
    "Three conclusions. First, I am genuinely capable at level L, and I am "
    "being honest that level I is not validated. Second, learning on the floor "
    "made me faster at diagnosing problems than a classroom would have. It "
    "just did not generate paperwork. Third, I have fourteen weeks and a "
    "concrete plan for them. And if I go back to the value I picked at the "
    "start: trust first, then the torch. That is what these two months taught "
    "me.")

# ============================================================ 13 · closing
# the Closing layout carries a large centred logo at y 3.39-4.10, keep it clear
s = new("Closing Slide", 13, dark=True, footer=False)
tb, tf = txbox(s, 0.0, 1.72, SW, 0.8, align=PP_ALIGN.CENTER)
para(tf, "Thank you", 44, WHITE, bold=True, first=True)
tb, tf = txbox(s, 0.0, 2.62, SW, 0.34, align=PP_ALIGN.CENTER)
para(tf, "Questions?", 17, LIME, bold=True, first=True)
tb, tf = txbox(s, 0.0, 4.56, SW, 0.62, align=PP_ALIGN.CENTER)
para(tf, "Diego Adair de León Márquez", 15, WHITE, first=True,
     align=PP_ALIGN.CENTER)
para(tf, "Robotic Welding  ·  Engineering  ·  Tenneco Aguascalientes",
     11.5, GRAYL, space_before=6, align=PP_ALIGN.CENTER)

notes[12] = (
    "Thank you. I am happy to take questions, and if anyone wants the detail "
    "behind any of the twenty three skills, I can walk through it.")

# ---------------------------------------------------------------- notes
for i, slide in enumerate(prs.slides):
    if i in notes:
        slide.notes_slide.notes_text_frame.text = notes[i]

# ---------------------------------------------------------------- morph
# PowerPoint stores Morph behind a markup-compatibility choice: 2016+ reads
# p159:morph, anything older falls back to a plain fade.
MORPH = (
    '<mc:AlternateContent '
    'xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/2006" '
    'xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main">'
    '<mc:Choice '
    'xmlns:p159="http://schemas.microsoft.com/office/powerpoint/2015/09/main" '
    'Requires="p159">'
    '<p:transition '
    'xmlns:p14="http://schemas.microsoft.com/office/powerpoint/2010/main" '
    'spd="slow" p14:dur="1250">'
    '<p159:morph option="byObject"/>'
    '</p:transition></mc:Choice>'
    '<mc:Fallback>'
    '<p:transition '
    'xmlns:p14="http://schemas.microsoft.com/office/powerpoint/2010/main" '
    'spd="slow"><p:fade/></p:transition>'
    '</mc:Fallback></mc:AlternateContent>')

def set_morph(slide):
    """Insert the transition after clrMapOvr, where CT_Slide expects it."""
    sld = slide._element
    frag = parse_xml(MORPH)
    anchor = sld.find(qn('p:clrMapOvr'))
    if anchor is None:
        anchor = sld.find(qn('p:cSld'))
    anchor.addnext(frag)

for slide in prs.slides:
    set_morph(slide)

prs.save(OUT)
print("saved", OUT)
