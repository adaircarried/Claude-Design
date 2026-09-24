# -*- coding: utf-8 -*-
"""Robotics Setter Technician - progress deck for the plant manager.

Diego Adair de Leon Marquez. Built on the corporate template, in the style of
Diego's previous Tenneco deck: dark road backgrounds, glass panels, 28 pt
white message titles with a lime support line, Morph between every slide.

Glass, the way Diego defines it: every panel is a copy of the exact piece of
background it sits on, lifted and saturated, then laid back on top at lower
opacity. Opacity stays a native PowerPoint setting (Format Picture >
Transparency), so it can be tuned by hand later.

No photos on purpose. Diego places his own photos by hand in the frames
marked PHOTO.
"""
from pptx import Presentation
from pptx.util import Inches, Pt
from pptx.dml.color import RGBColor
from pptx.enum.text import PP_ALIGN, MSO_ANCHOR
from pptx.enum.shapes import MSO_SHAPE
from pptx.oxml.ns import qn
from pptx.oxml import parse_xml
from PIL import Image, ImageFilter, ImageEnhance

import os
import sys
import zipfile
import io

HERE = os.path.dirname(os.path.abspath(__file__))
# Corporate template supplied by the internship program. It is marked
# TENNECO CONFIDENTIAL, so it is deliberately NOT committed here - drop it
# next to this script as template.pptx, or pass its path as argv[1].
SRC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "template.pptx")
OUT = os.path.join(HERE, "Tenneco_Robotics_Setter_Diego_de_Leon.pptx")
GLASS_DIR = os.path.join(HERE, "build", "glass")

# ---------------------------------------------------------------- palette
NAVY  = RGBColor(0x05, 0x1C, 0x2C)
LIME  = RGBColor(0xD5, 0xFB, 0x00)
WHITE = RGBColor(0xFF, 0xFF, 0xFF)
GRAY  = RGBColor(0xC6, 0xCD, 0xD1)
DIM   = RGBColor(0x8A, 0x9B, 0xA8)     # quiet ink for labels and legends
FONT  = "Segoe UI"
SW = 13.333

# ---------------------------------------------------------------- helpers
def txbox(slide, x, y, w, h, align=PP_ALIGN.LEFT, anchor=MSO_ANCHOR.TOP,
          name=None):
    tb = slide.shapes.add_textbox(Inches(x), Inches(y), Inches(w), Inches(h))
    if name:
        tb.name = name
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

def text(slide, x, y, w, h, s, size, color, bold=False, italic=False,
         align=PP_ALIGN.LEFT, name=None, anchor=MSO_ANCHOR.TOP):
    tb, tf = txbox(slide, x, y, w, h, align=align, name=name, anchor=anchor)
    para(tf, s, size, color, bold=bold, italic=italic, first=True)
    return tb, tf

def _alpha(clr_parent, pct):
    """Add <a:alpha> to the srgbClr inside a solidFill (pct = opacity)."""
    clr = clr_parent.find(qn('a:srgbClr'))
    clr.append(parse_xml(
        '<a:alpha xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/'
        'main" val="%d"/>' % int(pct * 1000)))

def rect(slide, x, y, w, h, fill=None, line=None, lw=0.75, opacity=100,
         line_opacity=100, shape=MSO_SHAPE.RECTANGLE, name=None):
    sp = slide.shapes.add_shape(shape, Inches(x), Inches(y), Inches(w), Inches(h))
    if name:
        sp.name = name
    if fill is None:
        sp.fill.background()
    else:
        sp.fill.solid()
        sp.fill.fore_color.rgb = fill
        if opacity < 100:
            _alpha(sp._element.spPr.find(qn('a:solidFill')), opacity)
    if line is None:
        sp.line.fill.background()
    else:
        sp.line.color.rgb = line
        sp.line.width = Pt(lw)
        if line_opacity < 100:
            ln = sp._element.spPr.find(qn('a:ln'))
            _alpha(ln.find(qn('a:solidFill')), line_opacity)
    # the theme style reference re-applies a drop shadow in some renderers
    st = sp._element.find(qn('p:style'))
    if st is not None:
        sp._element.remove(st)
    tf = sp.text_frame
    tf.word_wrap = True
    tf.margin_left = tf.margin_right = tf.margin_top = tf.margin_bottom = 0
    return sp

# ---------------------------------------------------------------- glass
# Layout backgrounds, read straight out of the template so the glass always
# copies the real pixels behind it (1920 x 1080, 144 px per inch).
_tz = zipfile.ZipFile(SRC)
BG = {
    "road":   Image.open(io.BytesIO(_tz.read("ppt/media/image4.jpg"))).convert("RGB"),
    "impact": Image.open(io.BytesIO(_tz.read("ppt/media/image6.jpg"))).convert("RGB"),
}
PX = 1920 / SW

# Lift colours. The panel is blend(background, lift) and then laid over the
# same background at GLASS_OPACITY, so the texture stays visible through it.
TONES = {
    "blue": ((98, 152, 192), 1.25),    # lands around RGB 32 65 93 on the road
    "gray": ((150, 158, 166), 0.35),   # neutral, for the "before" state
}
GLASS_OPACITY = 50                     # percent, native and editable
_glass_n = [0]

def glass(slide, x, y, w, h, bg="road", tone="blue", name=None, edge=True):
    """A panel whose fill is the background it covers, lifted and saturated."""
    crop = BG[bg].crop((round(x * PX), round(y * PX),
                        round((x + w) * PX), round((y + h) * PX)))
    crop = crop.filter(ImageFilter.GaussianBlur(5))        # frosted
    lift, sat = TONES[tone]
    crop = Image.blend(crop, Image.new("RGB", crop.size, lift), 0.5)
    crop = ImageEnhance.Color(crop).enhance(sat)
    os.makedirs(GLASS_DIR, exist_ok=True)
    _glass_n[0] += 1
    path = os.path.join(GLASS_DIR, "glass_%02d.jpg" % _glass_n[0])
    crop.save(path, quality=90)

    pic = slide.shapes.add_picture(path, Inches(x), Inches(y),
                                   Inches(w), Inches(h))
    pic.name = name or "Glass %d" % _glass_n[0]
    pic._element.nvPicPr.cNvPr.set("descr", "Decorative glass panel")
    blip = pic._element.blipFill.find(qn('a:blip'))
    blip.insert(0, parse_xml(
        '<a:alphaModFix xmlns:a="http://schemas.openxmlformats.org/'
        'drawingml/2006/main" amt="%d"/>' % (GLASS_OPACITY * 1000)))
    if edge:   # the thin light rim that separates glass from background
        rect(slide, x, y, w, h, line=WHITE, lw=0.75, line_opacity=16,
             name=(name + " rim") if name else None)
    return pic

def photo(slide, x, y, w, h, caption, name):
    """Designed space for one of Diego's photos. Delete the label, drop the
    photo on top, and crop it to this frame."""
    glass(slide, x, y, w, h, name=name)
    rect(slide, x + 0.18, y + 0.18, w - 0.36, h - 0.36, line=GRAY, lw=0.75,
         line_opacity=35)
    tb, tf = txbox(slide, x, y + h / 2 - 0.30, w, 0.6, align=PP_ALIGN.CENTER)
    para(tf, "PHOTO", 11, LIME, bold=True, first=True)
    para(tf, caption, 12, GRAY, space_before=2, align=PP_ALIGN.CENTER)

# ---------------------------------------------------------------- deck
prs = Presentation(SRC)
layouts = {l.name: l for l in prs.slide_masters[0].slide_layouts}

sld_lst = prs.slides._sldIdLst
RELNS = '{http://schemas.openxmlformats.org/officeDocument/2006/relationships}id'
for sid in list(sld_lst)[1:]:            # keep slide 1, drop the sample deck
    prs.part.drop_rel(sid.get(RELNS))
    sld_lst.remove(sid)

def fill_ph(ph, s, size, color, bold):
    tf = ph.text_frame
    tf.text = s
    r = tf.paragraphs[0].runs[0]
    r.font.size = Pt(size)
    r.font.bold = bold
    r.font.name = FONT
    r.font.color.rgb = color

def slide(title, sub):
    """Title Image_blank: road background, chevrons bottom left. Nothing may
    sit in the chevron zone (x 0-2.83, y 6.12 and below)."""
    s = prs.slides.add_slide(layouts["Title Image_blank"])
    ph = {p.placeholder_format.idx: p for p in s.placeholders}
    fill_ph(ph[20], title, 28, WHITE, True)
    ph[20].name = "!! Title 1"
    fill_ph(ph[21], sub, 13, LIME, False)
    ph[21].name = "!! Subtitle 1"
    return s

def label(slide, x, y, s, size=16, w=6.0, color=LIME):
    return text(slide, x, y, w, 0.3, s, size, color, bold=True)

def lime_rule(slide, x, y, w=1.20):
    return rect(slide, x, y, w, 0.03, fill=LIME)

notes = {}

# ============================================================ 1 · title
s1 = prs.slides[0]
ph = {p.placeholder_format.idx: p for p in s1.placeholders}

def only_run(p_ph, s):
    tf = p_ph.text_frame
    for pa in list(tf.paragraphs)[1:]:
        pa._p.getparent().remove(pa._p)
    p0 = tf.paragraphs[0]
    for r in list(p0.runs)[1:]:
        r._r.getparent().remove(r._r)
    p0.runs[0].text = s
    return p0.runs[0]

r = only_run(ph[13], "Robotics Setter Technician Program")
r.font.size = Pt(30); r.font.bold = True; r.font.color.rgb = WHITE
ph[13].name = "!! Title 1"
r = only_run(ph[14], "Robotic welding, from learning to improvement")
r.font.size = Pt(20); r.font.bold = True; r.font.color.rgb = LIME
r = only_run(ph[15], "Diego Adair de León Márquez")
r.font.size = Pt(13); r.font.color.rgb = WHITE

tb, tf = txbox(s1, 0.0, 5.35, SW, 1.0, align=PP_ALIGN.CENTER)
para(tf, "Universidad Politécnica de Aguascalientes  |  Mechatronics "
     "Engineering, 9th term", 11, GRAY, first=True)
para(tf, "Plant Tutor: Fernando Robledo", 11, GRAY, space_before=2,
     align=PP_ALIGN.CENTER)
para(tf, "Engineering Department  |  August 1, 2026 to January 1, 2027",
     11, LIME, space_before=2, align=PP_ALIGN.CENTER)

notes[0] = (
    "Good morning. My name is Diego de León, and I study Mechatronics "
    "Engineering at the Universidad Politécnica de Aguascalientes. Since "
    "August I have been part of the Robotics Setter Technician program here "
    "in Aguascalientes, with Fernando Robledo as my plant tutor. In the next "
    "fifteen minutes I will show you what the program asks of me, what I have "
    "learned so far, and the improvement we already put on the floor.")

# ============================================================ 2 · WIN
s = prs.slides.add_slide(layouts["Impact Slide"])
for p_ in list(s.placeholders):
    p_.element.getparent().remove(p_.element)
glass(s, 0.92, 2.00, 11.50, 4.00, bg="impact", name="!! Glass A")
text(s, 1.40, 2.45, 10.6, 0.3, "THE TENNECO VALUE I IDENTIFY WITH", 12, LIME,
     bold=True)
text(s, 1.40, 3.00, 10.6, 1.0, "Win", 42, WHITE, bold=True, name="!! Title 1")
lime_rule(s, 1.40, 4.25, 1.5)
tb, tf = txbox(s, 1.40, 4.60, 9.9, 1.4)
para(tf, "We must earn the trust of our employees and customers.", 15, GRAY,
     italic=True, first=True)
para(tf, "For me, winning meant taking the longest, hardest weld off a team "
     "member’s hands.", 14, LIME, space_before=10)

notes[1] = (
    "The Tenneco value I identify with is Win. Win means earning the trust of "
    "our people and our customers. On the floor, trust is not given because "
    "of a title. You earn it by showing up when a robot stops, by learning "
    "the process properly, and by making someone else’s job better. The "
    "clearest moment for me was the improvement I will show you later: a "
    "long, uncomfortable weld that a team member did by hand now runs on a "
    "robot. That is what winning looks like to me.")

# ============================================================ 3 · scope
s = slide("Program scope and objective",
          "Robotics Setter Technician  |  August 1, 2026 to January 1, 2027")
glass(s, 0.28, 1.75, 7.90, 4.25, name="!! Glass A")
label(s, 0.68, 2.12, "OBJECTIVE")
tb, tf = txbox(s, 0.68, 2.55, 7.1, 0.9)
para(tf, "Become a specialist in robotic welding and advanced technical "
     "support.", 17, WHITE, first=True)
lime_rule(s, 0.68, 3.62)
label(s, 0.68, 3.88, "WHY IT MATTERS")
tb, tf = txbox(s, 0.68, 4.30, 7.1, 1.4)
para(tf, "When a welding robot stops, the line stops with it. A setter who "
     "can find the cause and adjust the cell on the spot keeps parts moving "
     "and quality under control.", 14, GRAY, first=True, spacing=1.1)

glass(s, 8.55, 1.75, 4.50, 4.25, name="!! Glass B")
label(s, 8.95, 2.12, "FINAL PROJECT")
tb, tf = txbox(s, 8.95, 2.60, 3.75, 3.2)
para(tf, "An implemented improvement", 14, WHITE, bold=True, first=True)
para(tf, "In quality, availability or productivity.", 12, GRAY,
     space_before=2)
para(tf, "Mine: a manual weld moved to the 4M robotic cell.", 12, LIME,
     space_before=4)
para(tf, "An updated training process", 14, WHITE, bold=True,
     space_before=20)
para(tf, "How the next setters learn.", 12, GRAY, space_before=2)
para(tf, "Mine: study material reviewed, schedule rebuilt.", 12, LIME,
     space_before=4)

notes[2] = (
    "This is the program in one sentence: become a specialist in robotic "
    "welding and advanced technical support. It matters because when a "
    "welding robot stops, the line stops with it. The program closes with a "
    "final project in two parts. First, an improvement that is actually "
    "implemented, in quality, availability or productivity. Second, an update "
    "to the training process, so the next setters learn faster. I already "
    "have something real for both, and I will show you each one.")

# ============================================================ 4 · ILUO path
s = slide("Four levels, from understanding to improving",
          "The skill path of the program")
LEVELS = [
    ("I", "Understands",
     "Safety, the welding process, the procedure (WPS), defects and settings."),
    ("L", "Does it with support",
     "Basic adjustments, checking settings and changing consumables."),
    ("U", "Does it alone",
     "Programming the robot (SKS), creating programs and setting them up."),
    ("O", "Improves and teaches",
     "Troubleshooting, training others, Lean and continuous improvement."),
]
CW, CG = 2.97, 0.30
for i, (letter, title, body) in enumerate(LEVELS):
    x = 0.28 + i * (CW + CG)
    glass(s, x, 1.85, CW, 3.35, name="!! Card %d" % (i + 1))
    text(s, x + 0.34, 2.15, CW - 0.6, 0.6, letter, 30, LIME, bold=True)
    text(s, x + 0.34, 2.90, CW - 0.6, 0.7, title, 16, WHITE, bold=True)
    tb, tf = txbox(s, x + 0.34, 3.72, CW - 0.62, 1.4)
    para(tf, body, 12, GRAY, first=True, spacing=1.1)
glass(s, 0.28, 5.42, 12.77, 0.62, name="!! Strip")
text(s, 0.62, 5.62, 2.2, 0.25, "HOW IT WORKS", 11, LIME, bold=True)
text(s, 2.75, 5.60, 10.0, 0.3, "Each level builds on the one before: first "
     "understand, then do, then own it, then improve it.", 12, WHITE)

notes[3] = (
    "The program follows four levels, the same ILUO path the plant uses for "
    "any skill. I is understanding: safety, how the welding process works, "
    "the approved procedure, defects and settings. L is doing it with "
    "support: adjustments, checking settings and changing consumables. U is "
    "doing it alone, which here means programming the robot. And O is the "
    "highest level: solving problems, teaching others and improving the "
    "process. Each level builds on the one before.")

# ============================================================ 5 · timeline
s = slide("Eight weeks in, fourteen to go", "Program timeline")
glass(s, 0.66, 2.36, 12.26, 2.78, name="!! Glass A")
LX0, LX1, LY = 0.84, 12.74, 3.89
rect(s, LX0, LY, LX1 - LX0, 0.02, fill=GRAY, opacity=45)
MILES = [("Program start", "August 1, 2026", False),
         ("Today", "September 25", True),
         ("Level I exam", "October", False),
         ("Levels L and U", "November to December", False),
         ("Program close", "January 1, 2027", False)]
XS = [1.95 + k * 2.40 for k in range(5)]
rect(s, XS[0], LY - 0.005, XS[1] - XS[0], 0.03, fill=LIME)   # distance done
for k, (name, when, hot) in enumerate(MILES):
    cx = XS[k]
    d = 0.30 if hot else 0.21
    rect(s, cx - d / 2, LY + 0.01 - d / 2, d, d,
         fill=LIME if (hot or k == 0) else WHITE, shape=MSO_SHAPE.OVAL)
    text(s, cx - 1.15, 2.84, 2.30, 0.45, name, 24 if hot else 19,
         LIME if hot else WHITE, bold=True, align=PP_ALIGN.CENTER)
    text(s, cx - 1.15, 4.47, 2.30, 0.3, when, 14, LIME if hot else GRAY,
         bold=hot, align=PP_ALIGN.CENTER)

notes[4] = (
    "Here is where we are in time. The program started on August 1 and "
    "closes on January 1. Today I am eight weeks in, with fourteen to go. "
    "The next step is the Level I exam, planned for October. After that, "
    "November and December are for Levels L and U, where the work moves from "
    "knowing to doing: first with support, then on my own. One thing to keep "
    "in mind: there are no fixed training hours. Most of the learning happens "
    "while we attend real stops at the cells.")

# ============================================================ 6 · Level I
s = slide("Level I: understand it before you touch the robot",
          "What I learned")
glass(s, 0.28, 1.75, 7.90, 4.25, name="!! Glass A")
label(s, 0.68, 2.08, "FIVE TOPICS")
TOPICS = [("Safety", "Working at the cell without putting anyone at risk."),
          ("GMAW welding", "How wire, gas and an electric arc join the steel."),
          ("The WPS", "The approved recipe that every weld has to follow."),
          ("Defects", "Recognizing a bad weld and understanding its cause."),
          ("Parameters", "The settings that shape the weld, and their limits.")]
for k, (t, d) in enumerate(TOPICS):
    y = 2.58 + k * 0.66
    if k:
        rect(s, 0.68, y - 0.14, 7.10, 0.01, fill=WHITE, opacity=14)
    text(s, 0.68, y, 2.2, 0.35, t, 14, WHITE, bold=True)
    text(s, 2.95, y + 0.01, 4.95, 0.35, d, 13, GRAY)

glass(s, 8.55, 1.75, 4.50, 4.25, name="!! Glass B")
label(s, 8.95, 2.08, "HOW I LEARNED IT")
HOW = [("616", "pages of training manual and customer standard, read page "
               "by page."),
       ("Daily", "at the cells, attending robot stops with the technicians."),
       ("25", "questions of a practice exam from previous years, solved.")]
for k, (big, d) in enumerate(HOW):
    y = 2.55 + k * 1.10
    text(s, 8.95, y, 3.8, 0.5, big, 26, LIME, bold=True)
    text(s, 8.95, y + 0.50, 3.75, 0.5, d, 12, GRAY)

notes[5] = (
    "Level I is about understanding before touching anything. It has five "
    "topics. Safety, so nobody gets hurt around the cell. GMAW welding, which "
    "is how the robot joins steel with wire, gas and an electric arc. The "
    "WPS, the approved recipe every weld must follow. Defects, meaning how to "
    "recognize a bad weld and what caused it. And parameters, the settings "
    "that shape the weld and how far they are allowed to move. I learned it "
    "from six hundred pages of material, a practice exam, and every day at "
    "the cells.")

# ============================================================ 7 · progress
s = slide("Four skills on my own, thirteen in progress",
          "Progress on the 23 skills of the program")
glass(s, 0.28, 1.75, 8.25, 4.25, name="!! Glass A")
# (level, name, on my own, in progress, not started) - self assessment,
# same source as the schedule in nivel-I/, plus the 4M improvement.
ROWS = [("I", "Understands", 0, 9, 0),
        ("L", "With support", 3, 2, 0),
        ("U", "Alone", 0, 1, 2),
        ("O", "Improves", 1, 1, 4)]
D, DG = 0.26, 0.40
for k, (lv, nm, own, wip, todo) in enumerate(ROWS):
    y = 2.12 + k * 0.84
    text(s, 0.68, y - 0.06, 0.5, 0.5, lv, 24, LIME, bold=True)
    text(s, 1.22, y - 0.01, 2.2, 0.3, nm, 13, WHITE, bold=True)
    text(s, 1.22, y + 0.26, 2.2, 0.3, "%d skills" % (own + wip + todo), 11,
         DIM)
    for j in range(own + wip + todo):
        x = 3.45 + j * DG
        if j < own:
            rect(s, x, y + 0.06, D, D, fill=LIME, shape=MSO_SHAPE.OVAL)
        elif j < own + wip:
            rect(s, x, y + 0.06, D, D, fill=LIME, opacity=35, line=LIME,
                 lw=1.25, shape=MSO_SHAPE.OVAL)
        else:
            rect(s, x, y + 0.06, D, D, line=GRAY, lw=1.0, line_opacity=60,
                 shape=MSO_SHAPE.OVAL)
# legend, inside the panel and clear of the chevrons
LEG = [("On my own", dict(fill=LIME)),
       ("In progress", dict(fill=LIME, opacity=35, line=LIME, lw=1.25)),
       ("Not started", dict(line=GRAY, lw=1.0, line_opacity=60))]
lx = 3.45
for t, kw in LEG:
    rect(s, lx, 5.52, 0.18, 0.18, shape=MSO_SHAPE.OVAL, **kw)
    text(s, lx + 0.28, 5.49, 1.4, 0.25, t, 11, GRAY)
    lx += 1.62

glass(s, 8.85, 1.75, 4.20, 4.25, name="!! Glass B")
label(s, 9.25, 2.08, "ON MY OWN TODAY")
DONE = ["Changing consumables",
        "Adjusting the key welding settings",
        "Reading weld symbols on drawings",
        "Closing ANDON calls"]
for k, t in enumerate(DONE):
    y = 2.62 + k * 0.72
    rect(s, 9.25, y + 0.09, 0.14, 0.14, fill=LIME, shape=MSO_SHAPE.CHEVRON)
    text(s, 9.55, y, 3.3, 0.6, t, 14, WHITE)
text(s, 9.25, 5.45, 3.6, 0.3, "Level I: exam still pending.", 12, LIME,
     bold=True)

notes[6] = (
    "This is my honest status on the twenty three skills of the program. "
    "Four I already do on my own: changing consumables, adjusting the key "
    "welding settings, reading weld symbols on drawings, and closing ANDON "
    "calls. Thirteen are in progress, including all of Level I, which I have "
    "studied completely but still have to prove in the exam. Six have not "
    "started yet, most of them in the top level. The 4M improvement counts "
    "here too: it is my first step in the improvement skill.")

# ============================================================ 8 · industrialization
s = slide("Most of my time goes to industrialization",
          "How my time is split")
glass(s, 0.28, 1.75, 5.20, 4.25, name="!! Glass A")
text(s, 0.68, 2.00, 4.4, 0.9, "60%", 54, LIME, bold=True)
text(s, 0.68, 2.95, 4.4, 0.6, "Industrialization project, with Engineering",
     14, WHITE, bold=True)
rect(s, 0.68, 3.75, 4.40, 0.16, fill=GRAY, opacity=30)
rect(s, 0.68, 3.75, 4.40 * 0.6, 0.16, fill=LIME)
text(s, 0.68, 4.25, 4.4, 0.6, "40%", 30, WHITE, bold=True)
tb, tf = txbox(s, 0.68, 4.85, 4.4, 0.9)
para(tf, "Support at the robotic cells: stops, adjustments and ANDON calls.",
     12, GRAY, first=True)

glass(s, 5.80, 1.75, 7.25, 4.25, name="!! Glass B")
label(s, 6.20, 2.08, "WHAT INDUSTRIALIZATION MEANS")
tb, tf = txbox(s, 6.20, 2.52, 6.45, 1.2)
para(tf, "Getting a new process ready to run on the floor: equipment, "
     "programs, trials and the release to production.", 15, WHITE,
     first=True, spacing=1.1)
lime_rule(s, 6.20, 3.85)
label(s, 6.20, 4.08, "MY PART")
tb, tf = txbox(s, 6.20, 4.50, 6.45, 1.3)
para(tf, "[Project in one line: the product or line being industrialized.]",
     13, GRAY, italic=True, first=True)
para(tf, "[What I do in it, in one or two short phrases.]", 13, GRAY,
     italic=True, space_before=6)

notes[7] = (
    "About sixty percent of my time goes to an industrialization project "
    "with Engineering. Industrialization means getting a new process ready to "
    "run on the floor: the equipment, the programs, the trials and the "
    "release to production. [Add one or two sentences on the project and your "
    "role.] The other forty percent is support at the robotic cells, and "
    "that is where most of my hands-on learning comes from.")

# ============================================================ 9 · problem
s = slide("A long, uncomfortable weld, done by hand",
          "Improvement  |  The starting point")
glass(s, 0.28, 1.75, 6.35, 4.25, name="!! Glass A")
label(s, 0.68, 2.08, "THE STARTING POINT")
text(s, 0.68, 2.52, 5.6, 0.7, "One process ran in a manual welding booth.",
     17, WHITE)
PAIN = [("Time", "Long welds made every part slow to finish."),
        ("The team member", "Uncomfortable positions, held for a long time, "
                            "part after part.")]
for k, (t, d) in enumerate(PAIN):
    y = 3.35 + k * 0.92
    rect(s, 0.68, y + 0.07, 0.16, 0.16, fill=LIME, shape=MSO_SHAPE.CHEVRON)
    text(s, 1.00, y, 5.3, 0.3, t, 14, WHITE, bold=True)
    text(s, 1.00, y + 0.32, 5.3, 0.5, d, 13, GRAY)
text(s, 0.68, 5.40, 5.6, 0.35, "The idea: let a robot do it.", 15, LIME,
     bold=True)
photo(s, 6.98, 1.75, 6.07, 4.25, "Manual welding booth", "!! Glass B")

notes[8] = (
    "Now the improvement. It started with one process that ran in a manual "
    "welding booth. The welds on this part were long, so every part took a "
    "long time. And for the team member it was hard work: uncomfortable "
    "positions, held for a long time, part after part. My supervisor and I "
    "asked a simple question: why is a person doing the longest and hardest "
    "weld, when we have robots that can do it?")

# ============================================================ 10 · the change
s = slide("From the manual booth to the 4M robotic cell",
          "Improvement  |  What we did")
STEPS = [("01", "Study the part",
          "Which welds, how long they take, and what made them hard."),
         ("02", "Move it to 4M",
          "Bring the part into the automatic cell."),
         ("03", "Program and adjust",
          "Robot path and welding settings, until the weld is right."),
         ("04", "Validate and release",
          "Check weld quality, then hand it over to production.")]
for i, (n, t, d) in enumerate(STEPS):
    x = 0.28 + i * (CW + CG)
    glass(s, x, 1.85, CW, 3.10, name="!! Card %d" % (i + 1))
    text(s, x + 0.34, 2.15, CW - 0.6, 0.55, n, 30, LIME, bold=True)
    text(s, x + 0.34, 2.85, CW - 0.6, 0.7, t, 16, WHITE, bold=True)
    tb, tf = txbox(s, x + 0.34, 3.67, CW - 0.62, 1.2)
    para(tf, d, 12, GRAY, first=True, spacing=1.1)
    if i < 3:
        rect(s, x + CW + 0.07, 3.30, 0.16, 0.16, fill=LIME,
             shape=MSO_SHAPE.CHEVRON)
glass(s, 0.28, 5.30, 12.77, 0.66, name="!! Strip")
text(s, 0.62, 5.52, 2.2, 0.25, "TEAMWORK", 11, LIME, bold=True)
text(s, 2.75, 5.50, 10.0, 0.3, "Designed and implemented together with my "
     "supervisor, from the idea to the release.", 12, WHITE)

notes[9] = (
    "This is how we did it, in four steps. First, we studied the part: which "
    "welds, how long they took, and what made them hard. Second, we moved the "
    "part into the 4M cell, which is an automatic welding cell. Third, we "
    "programmed the robot and adjusted the welding settings until the weld "
    "was right. And fourth, we checked the weld quality and released it to "
    "production. I did this together with my supervisor, from the idea to "
    "the release.")

# ============================================================ 11 · before / after
s = slide("Same part, less time, less strain", "Improvement  |  Results")
glass(s, 0.28, 1.80, 6.15, 2.75, tone="gray", name="!! Glass A")
text(s, 0.70, 2.20, 5.3, 0.3, "BEFORE  |  MANUAL BOOTH", 11, GRAY, bold=True)
text(s, 0.70, 2.72, 5.3, 0.8, "XX min", 36, WHITE, bold=True)
text(s, 0.70, 3.62, 5.3, 0.3, "per part, welded by hand", 12, GRAY)

after = rect(s, 6.90, 1.80, 6.15, 2.75, fill=LIME, opacity=66,
             shape=MSO_SHAPE.ROUNDED_RECTANGLE, name="!! Glass B")
after.adjustments[0] = 0.06
text(s, 7.32, 2.20, 5.3, 0.3, "AFTER  |  4M ROBOTIC CELL", 11, NAVY,
     bold=True)
text(s, 7.32, 2.72, 5.3, 0.8, "XX min", 36, NAVY, bold=True)
text(s, 7.32, 3.62, 5.3, 0.3, "per part, welded by the robot", 12, NAVY)

KPI = [("XX%", "less process time per part"),
       ("XX", "welds moved from the booth to the robot"),
       ("XX min", "of uncomfortable welding removed per shift")]
for i, (big, lab) in enumerate(KPI):
    x = 0.28 + i * 4.37
    text(s, x, 4.95, 4.0, 0.6, big, 32, LIME, bold=True)
    text(s, x, 5.62, 3.9, 0.4, lab, 12, GRAY)

notes[10] = (
    "And these are the results. Before, in the manual booth, each part took "
    "XX minutes. Now, in the 4M cell, it takes XX minutes. That is XX percent "
    "less process time. XX welds moved from a person to the robot, and the "
    "team member no longer spends XX minutes per shift in uncomfortable "
    "positions. For me, this is the part I am proudest of: the process got "
    "faster, and a person’s work got better at the same time.")

# ============================================================ 12 · training update
s = slide("What I would change in how setters learn",
          "Final project  |  Training process update, a proposal")
TRAIN = [("01", "Reviewed",
          "616 pages of training manual and customer standard, page by page, "
          "against what Level I asks."),
         ("02", "Found",
          "Gaps: topics the exam asks about that the material barely covers, "
          "and an error in one key table."),
         ("03", "Rebuilt",
          "The schedule, in the plant’s own format, so plan and reality can "
          "be compared week by week.")]
for i, (n, t, d) in enumerate(TRAIN):
    x = 0.28 + i * 4.37
    glass(s, x, 1.85, 4.05, 3.35, name="!! Card %d" % (i + 1))
    text(s, x + 0.36, 2.15, 3.3, 0.55, n, 30, LIME, bold=True)
    text(s, x + 0.36, 2.90, 3.3, 0.35, t, 16, WHITE, bold=True)
    tb, tf = txbox(s, x + 0.36, 3.40, 3.33, 1.6)
    para(tf, d, 12, GRAY, first=True, spacing=1.1)
glass(s, 0.28, 5.42, 12.77, 0.62, name="!! Strip")
text(s, 0.62, 5.62, 2.2, 0.25, "PROPOSAL", 11, LIME, bold=True)
text(s, 2.75, 5.60, 10.0, 0.3, "Use the corrected material and the new "
     "schedule with the next group of setters.", 12, WHITE)

notes[11] = (
    "The second half of the final project is the training process. I read "
    "the full training material, more than six hundred pages including the "
    "customer standard, and compared it with what Level I asks. I found "
    "gaps: for example, the practice exam spends eight of its twenty five "
    "questions on TIG welding, which the skill matrix does not mention. I "
    "also found an error in one of the key tables. And I rebuilt the training "
    "schedule in the plant format, so we can compare the plan with what "
    "really happened. My proposal is to use both with the next group.")

# ============================================================ 13 · next
s = slide("Next: from knowing it to doing it alone",
          "What comes next, until January")
glass(s, 0.28, 1.75, 7.90, 4.25, name="!! Glass A")
label(s, 0.68, 2.08, "NEXT LEVELS")
NEXT = [("L", "Validate settings and find defects at the cell, with support.",
         "November"),
        ("U", "Create and set up robot programs on my own (SKS).",
         "December"),
        ("O", "Solve problems, share what I learn, keep improving.",
         "Already started with 4M")]
for k, (lv, d, when) in enumerate(NEXT):
    y = 2.62 + k * 1.05
    if k:
        rect(s, 0.68, y - 0.18, 7.10, 0.01, fill=WHITE, opacity=14)
    text(s, 0.68, y - 0.04, 0.5, 0.5, lv, 24, LIME, bold=True)
    text(s, 1.25, y, 6.5, 0.35, d, 14, WHITE)
    text(s, 1.25, y + 0.36, 6.5, 0.3, when, 12, GRAY)

glass(s, 8.55, 1.75, 4.50, 4.25, name="!! Glass B")
label(s, 8.95, 2.08, "WHAT I NEED")
NEED = ["A date for my Level I exam",
        "Supervised time programming robots in SKS",
        "Your feedback on the training proposal"]
for k, t in enumerate(NEED):
    y = 2.62 + k * 0.80
    rect(s, 8.95, y + 0.09, 0.14, 0.14, fill=LIME, shape=MSO_SHAPE.CHEVRON)
    text(s, 9.25, y, 3.55, 0.7, t, 14, WHITE)

notes[12] = (
    "What comes next. In November, Level L: validating settings and finding "
    "defects at the cell, with support. In December, Level U: creating and "
    "setting up robot programs on my own. Level O, solving problems and "
    "improving, already started with 4M. To get there I need three things: "
    "a date for my Level I exam, supervised time programming robots, and "
    "your feedback on the training proposal.")

# ============================================================ 14 · conclusions
s = slide("Conclusions and next steps", "What is in place, and what comes next")
glass(s, 0.28, 1.75, 6.35, 4.05, name="!! Glass A")
label(s, 0.68, 2.08, "IN PLACE")
INPLACE = ["Level I studied in full, and four skills done on my own.",
           "A manual weld moved to the 4M robotic cell: less time, less "
           "strain.",
           "A reviewed study path, and a schedule that shows plan versus "
           "reality."]
glass(s, 6.98, 1.75, 6.07, 4.05, name="!! Glass B")
label(s, 7.38, 2.08, "NEXT")
NEXTS = ["Pass the Level I exam in October.",
         "Reach Levels L and U before January.",
         "Keep measuring 4M, and find the next weld to automate."]
for x, items, w in ((0.68, INPLACE, 5.55), (7.38, NEXTS, 5.27)):
    for k, t in enumerate(items):
        text(s, x, 2.62 + k * 1.00, w, 0.8, t, 14, WHITE)
text(s, 3.40, 6.25, 9.6, 0.4, "I came to learn robotic welding, and I am "
     "already improving it.", 15, LIME, bold=True)

notes[13] = (
    "To close. In place today: Level I studied in full, four skills I do on "
    "my own, a manual weld moved to the 4M cell with less time and less "
    "strain, and a reviewed training path. Next: pass the Level I exam in "
    "October, reach Levels L and U before January, and keep measuring 4M "
    "while we look for the next weld to automate. I came here to learn "
    "robotic welding, and I am already improving it.")

# ============================================================ 15 · closing
s = prs.slides.add_slide(layouts["Closing Slide"])
for p_ in list(s.placeholders):
    p_.element.getparent().remove(p_.element)
text(s, 0.0, 5.15, SW, 0.7, "Thank you", 30, WHITE, bold=True,
     align=PP_ALIGN.CENTER, name="!! Title 1")
text(s, 0.0, 5.95, SW, 0.25, "Diego Adair de León Márquez   |   Engineering "
     "Department", 13, GRAY, align=PP_ALIGN.CENTER)

notes[14] = "Thank you. I am happy to take any questions."

# ---------------------------------------------------------------- notes
for i, sl in enumerate(prs.slides):
    if i in notes:
        sl.notes_slide.notes_text_frame.text = notes[i]

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

def set_morph(sl):
    """Insert the transition after clrMapOvr, where CT_Slide expects it."""
    sld = sl._element
    frag = parse_xml(MORPH)
    anchor = sld.find(qn('p:clrMapOvr'))
    if anchor is None:
        anchor = sld.find(qn('p:cSld'))
    anchor.addnext(frag)

for sl in prs.slides:
    set_morph(sl)

prs.save(OUT)
print("saved", OUT)
