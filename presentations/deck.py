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

ASSETS = os.path.join(HERE, "assets")

def img(slide, name, x, y, w=None, h=None):
    """Imagen generica del template. Diego las reemplaza por fotos reales."""
    kw = {}
    if w: kw["width"] = Inches(w)
    if h: kw["height"] = Inches(h)
    return slide.shapes.add_picture(os.path.join(ASSETS, name),
                                    Inches(x), Inches(y), **kw)

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

img(s, "valores_tenneco.png", 9.42, 1.42, h=4.9)
tb, tf = txbox(s, 0.5, 1.58, 8.0, 1.6)
para(tf, "WIN", 108, LIME, bold=True, first=True)

tb, tf = txbox(s, 0.56, 3.5, 8.4, 0.4)
para(tf, "“We must earn the trust of our employees and customers.”",
     17, GRAYL, italic=True, first=True)

rect(s, 0.5, 4.5, 8.6, 0.02, fill=RULE)
tb, tf = txbox(s, 0.56, 4.96, 8.5, 0.9)
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
        ("WHAT I LEARNED", "9 of 9",
         "Level I topics studied, none of them validated yet"),
        ("WHERE IT CAME FROM", "616",
         "pages of manual and customer standard, audited page by page"),
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
    "case we run short on time. I have studied all nine topics of level one, "
    "and not one of them is formally validated yet, because the exam has not "
    "happened. That study came out of six hundred and sixteen pages, the "
    "inspector manual and the customer standard, which I went through page by "
    "page. And I need four things from you before January, none of which cost "
    "money. If you only remember one line today, make it the green one: by "
    "January first I can be certified at levels I and L, if those four "
    "commitments get a date on a calendar.")

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
    rect(s, x, 2.28, CW, 1.9, fill=TINT)
    tb, tf = txbox(s, x + 0.3, 2.6, CW - 0.6, 1.3)
    para(tf, lbl, 9.5, BLUE, bold=True, first=True)
    para(tf, big, 20, NAVY, bold=True, space_before=12, spacing=1.08)

# imagen generica del template: Diego la cambia por una foto de su celda
img(s, "producto.jpg", 0.5, 4.5, h=1.95)
rect(s, 6.72, 4.5, 6.11, 1.95, fill=NAVY)
tb, tf = txbox(s, 7.1, 4.78, 5.4, 1.4, anchor=MSO_ANCHOR.MIDDLE)
para(tf, "So I could not learn by waiting for a breakdown.", 19, LIME,
     bold=True, first=True, spacing=1.14)
para(tf, "My hours went wherever the work was.", 13, GRAYL, space_before=10)

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

# ============================================================ 6 · nivel I


# El supervisor pidio enfocar el nivel I. Estas dos laminas son ese enfoque.
s = new("Standard_Light", 6, dark=False)
head(s, "LEVEL I IN DETAIL", "Nine topics, one exam, one passing mark",
     "4 hours  ·  written exam  ·  minimum 8.0", dark=False)
b = badge(s, 11.95, 0.86, 0.88, "I", NAVY, LIME, 32)
b.name = "!!iluo_I"

TEMAS = ["Welding safety", "GMAW equipment", "Metal transfer modes",
         "Process variables", "GMAW discontinuities", "Joint geometry",
         "Welding positions", "Welding symbols", "Welding control documents"]
TW, TS, TH = 3.95, 4.19, 1.28
for i, t in enumerate(TEMAS):
    x = 0.5 + (i % 3) * TS
    y = 2.3 + (i // 3) * 1.46
    rect(s, x, y, TW, TH, fill=TINT)
    tb, tf = txbox(s, x + 0.32, y + 0.2, TW - 0.64, 0.9)
    para(tf, "%02d" % (i + 1), 15, BLUE, bold=True, first=True)
    para(tf, t, 16, NAVY, bold=True, space_before=6, spacing=1.12)

tb, tf = txbox(s, 0.5, 6.76, 12.33, 0.3)
para(tf, "Twenty of the forty ILUO hours sit above this level, but none of "
         "them open until this exam is passed.", 12.5, MUTED, first=True)

notes[5] = (
    "My supervisor asked me to focus this review on level I, so here it is in "
    "full. Nine topics, four hours, and a single written exam with a passing "
    "mark of eight out of ten. Safety, equipment, transfer modes, process "
    "variables, discontinuities, joint geometry, positions, symbols and "
    "control documents. The reason this level matters more than it looks is "
    "the line at the bottom: twenty of the forty hours in the whole matrix "
    "sit above it, and none of them open until this exam is passed. Level I "
    "is the gate.")

# ============================================================ 7 · seguridad
s = new("Standard_Light", 7, dark=False)
head(s, "TOPIC 1 OF 9", "Protecting myself, and the person next to me",
     "Welding safety, per ANSI Z49.1", dark=False)

SEG = [("RADIATION", "Screens, helmet and welder's clothing. Arc flash does "
        "not spare the person who is only watching."),
       ("FUMES AND GAS", "The biggest factor under my own control is where I "
        "put my head relative to the fume column."),
       ("COMPRESSED GAS", "Cylinders upright and secured, always. Cap on "
        "before moving one."),
       ("THE ROBOTIC CELL", "Emergency stops, interlocks, teach mode at "
        "reduced speed, and lockout before anyone goes in.")]
for i, (lbl, cuerpo) in enumerate(SEG):
    x = 0.5 + i * CSTEP
    rect(s, x, 2.3, CW, 2.7, fill=TINT)
    tb, tf = txbox(s, x + 0.3, 2.62, CW - 0.6, 2.1)
    para(tf, lbl, 9.5, BLUE, bold=True, first=True)
    para(tf, cuerpo, 13.5, NAVY, space_before=12, spacing=1.2)

rect(s, 0.5, 5.32, 12.33, 1.1, fill=NAVY)
tb, tf = txbox(s, 0.92, 5.56, 11.5, 0.62, anchor=MSO_ANCHOR.MIDDLE)
para(tf, "The cell safety column is the one neither of my two study documents "
         "covers.", 16, LIME, bold=True, first=True)

notes[6] = (
    "Safety, per ANSI Z49.1. The arc flash does not care whether you are "
    "welding or watching, which is why the screens matter as much as the "
    "helmet. On fumes, the biggest factor I actually control is where I put "
    "my head relative to the column. And the fourth card is the cell itself, "
    "which neither of my two documents covers, so I had to build it myself.")
# ============================================================ 8 · equipo MIG
s = new("Standard_Light", 8, dark=False)
head(s, "TOPICS 2 AND 4 OF 9", "Five blocks, and the four that wear out",
     "GMAW equipment and process variables", dark=False)

rect(s, 0.5, 2.28, 6.02, 3.0, fill=TINT)
tb, tf = txbox(s, 0.86, 2.58, 5.3, 2.4)
para(tf, "THE EQUIPMENT", 9.5, BLUE, bold=True, first=True)
for it in ("Power source, constant voltage, DCEP",
           "Wire feeder and drive rolls", "Torch", "Gas supply and flowmeter",
           "Work lead"):
    para(tf, it, 13.5, NAVY, space_before=13)

rect(s, 6.81, 2.28, 6.02, 3.0, fill=NAVY)
tb, tf = txbox(s, 7.17, 2.58, 5.3, 2.4)
para(tf, "WHAT I CHANGE MYSELF", 9.5, LIME, bold=True, first=True)
for it in ("Contact tip", "Nozzle", "Diffuser", "Liner", "Drive rolls"):
    para(tf, it, 13.5, WHITE, space_before=13)

rect(s, 0.5, 5.6, 12.33, 1.16, fill=LIME)
tb, tf = txbox(s, 0.92, 5.86, 11.5, 0.66, anchor=MSO_ANCHOR.MIDDLE)
para(tf, "There is no amperage knob. I set wire feed speed and voltage, and "
         "the current follows.", 18, NAVY, bold=True, first=True)

notes[7] = (
    "Equipment and variables together, because on the floor they are one "
    "conversation. Five blocks; four of them wear out and I change them "
    "myself, which is the level L skill I already own. The bottom line took "
    "me longest to understand. There is no amperage knob on a MIG feeder. "
    "The source holds the voltage, I set wire feed speed, and the current "
    "comes out of that. Once that clicked, adjusting a cell stopped being "
    "guesswork.")
# ============================================================ 9 · TIG
s = new("Standard_Dark", 9, dark=True)
head(s, "THE OTHER MANUAL PROCESS", "TIG, where nothing about MIG applies",
     "The plant runs two manual processes, and they share almost no parts",
     dark=True)

TIGC = [("NON-CONSUMABLE ELECTRODE",
         "Tungsten only holds the arc. Filler goes in by hand, as a rod."),
        ("100 % ARGON", "Any CO₂ would consume the tungsten. 15 to 25 CFH, "
         "read on the flowmeter and nowhere else."),
        ("ITS OWN CONSUMABLES",
         "Collet, collet body, cup and back cap. None of the MIG set fits.")]
for i, (lbl, cuerpo) in enumerate(TIGC):
    x = 0.5 + i * SS3
    rect(s, x, 2.28, SW3, 2.6, fill=CARD)
    tb, tf = txbox(s, x + 0.36, 2.6, SW3 - 0.72, 2.0)
    para(tf, lbl, 9.5, LIME, bold=True, first=True)
    para(tf, cuerpo, 13.5, GRAYL, space_before=12, spacing=1.2)

rect(s, 0.5, 5.2, 12.33, 1.16, fill=CARD)
tb, tf = txbox(s, 0.92, 5.44, 11.5, 0.7, anchor=MSO_ANCHOR.MIDDLE)
para(tf, "A tungsten that touches the pool is cut back and reground, never "
         "cleaned.", 17, WHITE, bold=True, first=True)

notes[8] = (
    "The plant runs two manual processes, and almost nothing carries over "
    "from MIG to TIG. The tungsten does not melt, it only holds the arc. The "
    "gas is pure argon, because any carbon dioxide would eat the tungsten, "
    "and the flow is read on the flowmeter, never on the tank gauge. The "
    "consumables are a different set entirely. And a contaminated tungsten is "
    "cut back and reground, not cleaned.")
# ============================================================ 10 · transferencia
s = new("Standard_Light", 10, dark=False)
head(s, "TOPIC 3 OF 9", "How the metal crosses the arc",
     "Four transfer modes, and what picks between them", dark=False)

MODOS = [("SHORT CIRCUIT", "Low current", "The wire touches the work. Lowest "
          "heat, for thin sheet. Its risk is lack of fusion."),
         ("GLOBULAR", "Mid current", "Large drops fall by gravity. Heavy "
          "spatter, so we avoid it."),
         ("SPRAY", "High current", "Fine droplets, no contact. Needs at least "
          "80 % argon. Flat and horizontal only."),
         ("PULSED", "Controlled", "The source alternates high and low. Spray "
          "quality at low heat input.")]
for i, (lbl, cur, cuerpo) in enumerate(MODOS):
    x = 0.5 + i * CSTEP
    rect(s, x, 2.3, CW, 2.84, fill=TINT)
    tb, tf = txbox(s, x + 0.3, 2.6, CW - 0.6, 2.3)
    para(tf, lbl, 9.5, BLUE, bold=True, first=True)
    para(tf, cur, 15, NAVY, bold=True, space_before=8)
    para(tf, cuerpo, 12.5, MUTED, space_before=10, spacing=1.2)

rect(s, 0.5, 5.46, 12.33, 1.1, fill=NAVY)
tb, tf = txbox(s, 0.92, 5.7, 11.5, 0.62, anchor=MSO_ANCHOR.MIDDLE)
para(tf, "Changing the transfer mode is an essential variable. It invalidates "
         "the procedure.", 16, LIME, bold=True, first=True)

notes[9] = (
    "How the metal crosses the arc. Four modes, and the thing to hold on to "
    "is that short circuit is the low-heat one for thin sheet, and spray is "
    "the high-current one that needs at least eighty percent argon. The "
    "bottom line is the commercial part: changing the mode is an essential "
    "variable, so it invalidates the procedure.")
# ============================================================ 11 · plano
s = new("Standard_Light", 11, dark=False)
head(s, "TOPICS 6, 7 AND 8 OF 9", "Reading the print: joint, position, symbol",
     "Three short topics that always travel together", dark=False)

PLANO = [("JOINT GEOMETRY",
          "Butt, lap, T, corner and edge. The throat is what carries the "
          "load, not the leg."),
         ("POSITIONS",
          "1G to 4G for groove welds, 1F to 4F for fillet welds. Cummins adds "
          "P for plug and S for slot."),
         ("SYMBOLS",
          "AWS A2.4 by default. Arrow side goes below the reference line, the "
          "other side above.")]
for i, (lbl, cuerpo) in enumerate(PLANO):
    x = 0.5 + i * SS3
    rect(s, x, 2.3, SW3, 2.5, fill=TINT)
    tb, tf = txbox(s, x + 0.36, 2.62, SW3 - 0.72, 1.9)
    para(tf, lbl, 9.5, BLUE, bold=True, first=True)
    para(tf, cuerpo, 14, NAVY, space_before=12, spacing=1.2)

rect(s, 0.5, 5.12, 12.33, 1.3, fill=NAVY)
tb, tf = txbox(s, 0.92, 5.4, 11.5, 0.8, anchor=MSO_ANCHOR.MIDDLE)
para(tf, "Our prints carry two things AWS does not: the weld class and the "
         "WDR, both in the tail of the symbol.", 16, LIME, bold=True,
     first=True, spacing=1.16)
para(tf, "If no class is marked, it is Class 2 by default.", 12.5, GRAYL,
     space_before=8)

notes[10] = (
    "Three short topics that travel together, because you meet all three on "
    "the same print. The throat carries the load, not the leg. Positions one "
    "through four, G for groove and F for fillet. And on the symbol, the rule "
    "people get backwards: arrow side goes below the reference line. What is "
    "ours is the bottom line: our prints carry the weld class and the WDR in "
    "the tail, and with no class marked it is class two by default.")
# ============================================================ 12 · discontinuidades
s = new("Standard_Dark", 12, dark=True)
head(s, "TOPICS 5 AND 9 OF 9",
     "A defect is a discontinuity that went too far",
     "Discontinuities, acceptance limits and the documents that set them",
     dark=True)

rect(s, 0.5, 2.28, 6.02, 2.9, fill=CARD)
tb, tf = txbox(s, 0.86, 2.56, 5.3, 2.3)
para(tf, "WHAT WE SEE IN THE CELL", 9.5, LIME, bold=True, first=True)
for it in ("Porosity, from poor gas coverage",
           "Undercut, from too much current or speed",
           "Lack of fusion, from too little heat",
           "Spatter, burn-through, cracks"):
    para(tf, it, 13.5, WHITE, space_before=13)

rect(s, 6.81, 2.28, 6.02, 2.9, fill=CARD)
tb, tf = txbox(s, 7.17, 2.56, 5.3, 2.3)
para(tf, "WHAT SETS THE LIMIT", 9.5, LIME, bold=True, first=True)
for it in ("The weld class on the print, 1, 2 or 3",
           "Class 1 allows no undercut at all",
           "Cracks are never allowed, in any class",
           "The WPS is the control plan"):
    para(tf, it, 13.5, WHITE, space_before=13)

rect(s, 0.5, 5.5, 12.33, 1.1, fill=LIME)
tb, tf = txbox(s, 0.92, 5.74, 11.5, 0.62, anchor=MSO_ANCHOR.MIDDLE)
para(tf, "The same bead passes in Class 3 and fails in Class 1.", 18, NAVY,
     bold=True, first=True)

notes[11] = (
    "These two topics are meaningless apart. On the left, what we see in the "
    "cell and what causes it. On the right, what decides whether it is "
    "acceptable, and it is not my opinion: it is the weld class on the print. "
    "Class one allows no undercut at all, and cracks are never allowed in any "
    "class. The whole idea is the bottom line: the same bead passes in class "
    "three and fails in class one.")
# ============================================================ 13 · numeros
s = new("Standard_Dark", 13, dark=True)
head(s, "WHAT THE CUSTOMER ADDS",
     "The customer turns that theory into numbers",
     "From the Cummins standard for aftertreatment and exhaust parts",
     dark=True)

CIFRAS = [("± 5 %", "wire feed speed, current and voltage"),
          ("± 10 %", "travel speed"),
          ("10 – 16 mm", "contact tip to work distance, every GMAW weld")]
for i, (big, lbl) in enumerate(CIFRAS):
    x = 0.5 + i * SS3
    rect(s, x, 2.26, SW3, 2.34, fill=CARD)
    tb, tf = txbox(s, x + 0.36, 2.6, SW3 - 0.72, 1.7)
    para(tf, big, 34, LIME, bold=True, first=True)
    para(tf, lbl, 14, GRAYL, space_before=14, spacing=1.2)

rect(s, 0.5, 4.92, 12.33, 1.24, fill=LIME)
tb, tf = txbox(s, 0.92, 5.2, 11.5, 0.7, anchor=MSO_ANCHOR.MIDDLE)
para(tf, "Outside the band, the procedure has to be revalidated.", 20, NAVY,
     bold=True, first=True)

tb, tf = txbox(s, 0.5, 6.44, 12.33, 0.3)
para(tf, "Source: CES-S-MANF-150, sections 9 and 13.", 11, MUTEDD, first=True)

notes[12] = (
    "And this is what the customer standard adds on top of all of it. Our "
    "matrix says adjust the essential variables. It does not say by how much. "
    "Cummins does. Wire feed speed, current and voltage within five percent. "
    "Travel speed within ten. Contact tip to work distance between ten and "
    "sixteen millimetres on every GMAW weld, no matter the transfer mode. Go "
    "outside those bands and the procedure has to be revalidated. That is the "
    "difference between adjusting a machine and running a qualified process.")
# ============================================================ 14 · preparacion


s = new("Standard_Dark", 14, dark=True)
head(s, "HOW I AM PREPARING",
     "616 pages, and five gaps neither of them closes", None, dark=True)

FUENTES = [("CWI MANUAL, COMIMSA", "440 pages",
            "Covers five of the nine topics well: symbols, joint geometry, "
            "discontinuities, positions and safety."),
           ("CES-S-MANF-150, CUMMINS", "176 pages",
            "Turns that theory into numbers I can be held to: essential "
            "variables, work angle, CTWD, acceptance limits.")]
y = 1.9
for titulo, vol, cuerpo in FUENTES:
    rect(s, 0.5, y, 7.3, 2.24, fill=CARD)
    tb, tf = txbox(s, 0.86, y + 0.3, 6.6, 1.7)
    para(tf, titulo, 10, MUTEDD, bold=True, first=True)
    para(tf, vol, 26, LIME, bold=True, space_before=6)
    para(tf, cuerpo, 13, GRAYL, space_before=10, spacing=1.2)
    y += 2.44

rect(s, 8.14, 1.9, 4.69, 4.44, fill=CARD)
tb, tf = txbox(s, 8.5, 2.2, 4.0, 3.8)
para(tf, "WHAT NEITHER ONE COVERS", 10, LIME, bold=True, first=True)
for hueco in ("Robotic cell safety", "Constant voltage V-A curve",
              "Transition current", "ISO 2553 symbols",
              "The Tenneco parameter sheet"):
    para(tf, hueco, 14, WHITE, space_before=20)

tb, tf = txbox(s, 0.5, 6.62, 12.33, 0.34)
para(tf, "I mapped both documents page by page. The gaps are mine to close, "
         "and I know exactly what they are.", 15, LIME, bold=True, first=True)

notes[13] = (
    "I did not just receive the material, I audited it. Six hundred and "
    "sixteen pages between the two. The COMIMSA manual is a welding inspector "
    "course, so it is excellent at what you look at and measure: symbols, "
    "joint geometry, discontinuities, positions. The Cummins standard is the "
    "other half. It takes that theory and puts numbers on it that we are "
    "contractually held to. For example, our matrix says adjust the essential "
    "variables. The standard says wire feed speed, current and voltage within "
    "five percent, travel speed within ten, or the procedure has to be "
    "revalidated. And then the panel on the right is what I found missing in "
    "both, including robotic cell safety, which neither document mentions "
    "once. Those five are mine to close, and I already know what they are.")

# ============================================================ 15 · scorecard


s = new("Standard_Light", 15, dark=False)
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

notes[14] = (
    "Each square is one skill. Four of the twenty three I execute with nobody "
    "standing next to me: consumable changes, which means contact tip, liner, "
    "nozzle, diffuser and rollers; essential variable adjustment, so current, "
    "wire feed, voltage and travel speed; reading welding symbols; and closing "
    "ANDON orders. Twelve are in progress. Seven I have not started. And then "
    "the line at the bottom, which is the part I want to be direct about. I "
    "know the level I content because I picked it up in the cell, but the "
    "theory session and the exam have not happened, so formally I am validated "
    "at zero. I am not going to hide that.")

# ============================================================ 16 · lo que sigue

s = new("Standard_Light", 16, dark=False)
head(s, "WHAT COMES NEXT", "Three levels ahead, and what each one asks",
     None, dark=False)

SIGUE = [("L", "Execution in the cell",
          "Validate parameters against the WPS and adjust the essential "
          "variables on my own.", "Validation on the floor"),
         ("U", "Robot programming",
          "Create a welding program in SKS and assign its parameters.",
          "Practical exam"),
         ("O", "Teach and improve",
          "Train a level 2 or 3 technician and take one improvement idea to "
          "implementation.", "Implementation review")]
for i, (letra, nombre, objetivo, puerta) in enumerate(SIGUE):
    x = 0.5 + i * SS3
    rect(s, x, 2.2, SW3, 3.3, fill=NAVY)
    b = badge(s, x + 0.36, 2.52, 0.78, letra, LIME, NAVY, 28)
    b.name = "!!iluo_%s" % letra
    tb, tf = txbox(s, x + 0.36, 3.54, SW3 - 0.72, 1.7)
    para(tf, nombre, 18, WHITE, bold=True, first=True)
    para(tf, objetivo, 13, GRAYL, space_before=10, spacing=1.2)
    para(tf, puerta, 12, LIME, bold=True, space_before=14)

rect(s, 0.5, 5.82, 12.33, 1.1, fill=TINT)
tb, tf = txbox(s, 0.92, 6.06, 11.5, 0.62, anchor=MSO_ANCHOR.MIDDLE)
para(tf, "Twenty of the forty hours live in these three levels, and none of "
         "them open until Level I is signed.", 16, NAVY, bold=True, first=True)

notes[15] = (
    "And this is where I am going. Level L is execution: validating parameters "
    "against the WPS and adjusting the essential variables without anyone "
    "standing next to me, closed by a validation on the floor. Level U is "
    "programming: building a welding program in SKS and assigning its "
    "parameters, closed by a practical exam. Level O is multiplying: training "
    "a level two or three technician and taking one improvement idea all the "
    "way to implementation. Twenty of the forty hours in the whole matrix live "
    "in these three. And the line at the bottom is why level I matters so "
    "much: none of them open until it is signed.")

# ============================================================ 17 · integration


s = new("Standard_Light", 17, dark=False)
head(s, "THE INTEGRATION PROJECT", "60% of my hours, and none of it on the matrix",
     None, dark=False)
y = 2.4
for lbl, val in [("WHAT IT IS",        "[ describe the integration in one line ]"),
                 ("MY ROLE",           "[ what you own day to day ]"),
                 ("WHAT IT TAUGHT ME", "[ three skills outside the matrix ]"),
                 ("STATUS",            "[ % complete, next milestone ]")]:
    rect(s, 0.5, y, 8.0, 1.0, fill=TINT)
    tb, tf = txbox(s, 0.9, y + 0.22, 2.5, 0.56, anchor=MSO_ANCHOR.MIDDLE)
    para(tf, lbl, 10, BLUE, bold=True, first=True)
    tb, tf = txbox(s, 3.5, y + 0.22, 4.7, 0.56, anchor=MSO_ANCHOR.MIDDLE)
    para(tf, val, 14, NAVY, first=True)
    y += 1.14

# imagen generica del template: Diego la cambia por una foto de la integracion
img(s, "carretera.jpg", 8.8, 2.4, w=4.03)
rect(s, 8.8, 4.26, 4.03, 2.3, fill=NAVY)
tb, tf = txbox(s, 9.14, 4.56, 3.35, 1.8)
para(tf, "WHY IT BELONGS HERE", 9.5, LIME, bold=True, first=True)
para(tf, "It is real engineering work. It simply does not appear anywhere on "
         "the ILUO matrix, so the scorecard cannot see it.",
     12.5, GRAYL, space_before=10, spacing=1.18)

notes[16] = (
    "This is the project that took most of my time, and I worked on it "
    "directly with my engineering lead. Explain it here in your own words: "
    "what the integration is, what you own day to day, what it taught you that "
    "is not welding, and where it stands right now. Keep it to about ninety "
    "seconds. The reason it belongs in this presentation is simple. It is real "
    "engineering work. It just does not appear anywhere on the ILUO matrix, so "
    "the scorecard you saw two slides ago cannot see it.")

# ============================================================ 18 · gantt


s = new("Standard_Light", 18, dark=False)
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

notes[17] = (
    "The same story on a calendar. The green line is today, September twenty "
    "fifth. Everything to its left actually happened: level I content and "
    "level L execution built up in the cell, ANDON orders closed, and the "
    "integration project running alongside all of it. Everything to the right "
    "is what I am proposing. The level I exam in October, level L closed by "
    "the end of November, level U programming from October to December, and "
    "one level O improvement before I finish. Level I needs a scheduled "
    "session and level U needs supervised cell time. Both are calendar items, "
    "not budget items.")

# ============================================================ 19 · the ask


s = new("Standard_Light", 19, dark=False)
head(s, "WHAT I NEED FROM YOU", "Four commitments, and none of them cost money",
     None, dark=False)

for x, w, lbl in ((1.18, 5.2, "COMMITMENT"), (6.8, 3.6, "WHAT I NEED"),
                  (10.9, 1.9, "WHEN")):
    tb, tf = txbox(s, x, 2.3, w, 0.26)
    para(tf, lbl, 9, BLUE, bold=True, first=True)

y = 2.68
for i, (a, unlock, need, when) in enumerate([
        ("Sit the Level I written exam", "Validates Level I",
         "A date on the calendar", "October"),
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

notes[18] = (
    "So here is the ask, and it is small. Four things. Give me two half-days "
    "for the level I session and the exam, and the theory stops being "
    "informal. Give me access to the parameter sheets and I will document ten "
    "validations, which closes level L. Give me supervised cell time and I "
    "will build one program in SKS, which opens level U. And give me a sponsor "
    "for one improvement idea and I leave you my first piece of level O "
    "evidence. Every one of these produces a document you can audit. None of "
    "them needs budget. They need calendar.")

# ============================================================ 20 · conclusions


s = new("Standard_Dark", 20, dark=True)
head(s, "CONCLUSIONS", "Informally capable now, certified by January", None,
     dark=True)
y = 2.1
for i, t in enumerate([
        "I studied all of Level I. None of it is validated yet.",
        "The floor taught me faster than the schedule would have.",
        "Two months ago I called the ANDON in. Now I close it.",
        "Fourteen weeks left, and a plan for them."]):
    tb, tf = txbox(s, 0.5, y, 0.9, 0.5)
    para(tf, "0%d" % (i + 1), 26, LIME, bold=True, first=True)
    tb, tf = txbox(s, 1.52, y + 0.04, 10.8, 0.5)
    para(tf, t, 21, WHITE, bold=True, first=True)
    y += 1.1

rect(s, 0.5, 6.22, 12.33, 0.02, fill=RULE)
tb, tf = txbox(s, 0.5, 6.46, 12.33, 0.34)
para(tf, "Trust first, then the torch. That is how I read WIN.", 17, LIME,
     bold=True, italic=True, first=True)

notes[19] = (
    "Three conclusions. First, I am genuinely capable at level L, and I am "
    "being honest that level I is not validated. Second, learning on the floor "
    "made me faster at diagnosing problems than a classroom would have. It "
    "just did not generate paperwork. Third, I have fourteen weeks and a "
    "concrete plan for them. And if I go back to the value I picked at the "
    "start: trust first, then the torch. That is what these two months taught "
    "me.")

# ============================================================ 21 · closing


# the Closing layout carries a large centred logo at y 3.39-4.10, keep it clear
s = new("Closing Slide", 21, dark=True, footer=False)
tb, tf = txbox(s, 0.0, 1.72, SW, 0.8, align=PP_ALIGN.CENTER)
para(tf, "Thank you", 44, WHITE, bold=True, first=True)
tb, tf = txbox(s, 0.0, 2.62, SW, 0.34, align=PP_ALIGN.CENTER)
para(tf, "Questions?", 17, LIME, bold=True, first=True)
tb, tf = txbox(s, 0.0, 4.56, SW, 0.62, align=PP_ALIGN.CENTER)
para(tf, "Diego Adair de León Márquez", 15, WHITE, first=True,
     align=PP_ALIGN.CENTER)
para(tf, "Robotic Welding  ·  Engineering  ·  Tenneco Aguascalientes",
     11.5, GRAYL, space_before=6, align=PP_ALIGN.CENTER)

notes[20] = (
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
