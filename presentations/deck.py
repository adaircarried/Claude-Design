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
BLUE  = RGBColor(0x00, 0x33, 0xA0)     # Tenneco blue, the accent on white
SLATE = RGBColor(0x6B, 0x7A, 0x83)     # secondary ink on white
FONT  = "Segoe UI"

# Theme tokens. Slides alternate between the dark road and the white layout;
# slide() switches these before any shape is drawn.
TXT, SUB, ACC, MUT = WHITE, GRAY, LIME, DIM
LIGHT = False

def theme(light):
    global TXT, SUB, ACC, MUT, LIGHT
    LIGHT = light
    if light:
        TXT, SUB, ACC, MUT = NAVY, SLATE, BLUE, SLATE
    else:
        TXT, SUB, ACC, MUT = WHITE, GRAY, LIME, DIM
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

def glass(slide, x, y, w, h, bg="road", tone="blue", name=None):
    """A panel whose fill is the background it covers, lifted and saturated.
    On white there is nothing to copy, so it becomes a soft navy tint.

    Always ONE rectangle (p:sp), never a picture plus a separate rim: Morph
    only pairs objects of the same kind, so a picture on a dark slide and a
    rectangle on a white one would never animate into each other, even with
    the same !! name. On dark the copied background goes in as a picture
    fill of that rectangle, and the rim is its own outline."""
    if LIGHT:
        return rect(slide, x, y, w, h, fill=NAVY, opacity=4, line=NAVY,
                    lw=0.75, line_opacity=12, name=name)
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

    sp = rect(slide, x, y, w, h, line=WHITE, lw=0.75, line_opacity=16,
              name=name or "Glass %d" % _glass_n[0])
    _, rid = slide.part.get_or_add_image_part(path)
    spPr = sp._element.spPr
    spPr.remove(spPr.find(qn('a:noFill')))
    spPr.find(qn('a:prstGeom')).addnext(parse_xml(
        '<a:blipFill xmlns:a="http://schemas.openxmlformats.org/drawingml/'
        '2006/main" xmlns:r="http://schemas.openxmlformats.org/'
        'officeDocument/2006/relationships" rotWithShape="1">'
        '<a:blip r:embed="%s"><a:alphaModFix amt="%d"/></a:blip>'
        '<a:stretch><a:fillRect/></a:stretch></a:blipFill>'
        % (rid, GLASS_OPACITY * 1000)))
    return sp

def photo(slide, x, y, w, h, caption, name):
    """Designed space for one of Diego's photos. Delete the label, drop the
    photo on top, and crop it to this frame."""
    glass(slide, x, y, w, h, name=name)
    rect(slide, x + 0.18, y + 0.18, w - 0.36, h - 0.36, line=SUB, lw=0.75,
         line_opacity=35)
    tb, tf = txbox(slide, x, y + h / 2 - 0.30, w, 0.6, align=PP_ALIGN.CENTER)
    para(tf, "PHOTO", 11, ACC, bold=True, first=True)
    para(tf, caption, 12, SUB, space_before=2, align=PP_ALIGN.CENTER)

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

def slide(title, sub, light=False):
    """Dark: Title Image_blank, road background with the chevrons bottom left,
    and nothing may sit in the chevron zone (x 0-2.83, y 6.12 and below).
    Light: Standard_Light, plain white."""
    theme(light)
    s = prs.slides.add_slide(
        layouts["Standard_Light" if light else "Title Image_blank"])
    ph = {p.placeholder_format.idx: p for p in s.placeholders}
    fill_ph(ph[20], title, 28, TXT, True)
    ph[20].name = "!! Title 1"
    fill_ph(ph[21], sub, 13, ACC, False)
    ph[21].name = "!! Subtitle 1"
    return s

def label(slide, x, y, s, size=16, w=6.0, color=None):
    return text(slide, x, y, w, 0.3, s, size, color or ACC, bold=True)

def lime_rule(slide, x, y, w=1.20):
    return rect(slide, x, y, w, 0.03, fill=ACC)

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


# ============================================================ 2 · WIN
theme(False)
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


# ============================================================ 3 · scope
s = slide("Program scope and objective",
          "Robotics Setter Technician  |  August 1, 2026 to January 1, 2027",
          light=True)
glass(s, 0.28, 1.75, 7.90, 4.25, name="!! Glass A")
label(s, 0.68, 2.12, "OBJECTIVE")
tb, tf = txbox(s, 0.68, 2.55, 7.1, 0.9)
para(tf, "Become a specialist in robotic welding and advanced technical "
     "support.", 17, TXT, first=True)
lime_rule(s, 0.68, 3.62)
label(s, 0.68, 3.88, "WHY IT MATTERS")
tb, tf = txbox(s, 0.68, 4.30, 7.1, 1.4)
para(tf, "When a welding robot stops, the line stops with it. A setter who "
     "can find the cause and adjust the cell on the spot keeps parts moving "
     "and quality under control.", 14, SUB, first=True, spacing=1.1)

glass(s, 8.55, 1.75, 4.50, 4.25, name="!! Glass B")
label(s, 8.95, 2.12, "FINAL PROJECT")
tb, tf = txbox(s, 8.95, 2.60, 3.75, 3.2)
para(tf, "An implemented improvement", 14, TXT, bold=True, first=True)
para(tf, "In quality, availability or productivity.", 12, SUB,
     space_before=2)
para(tf, "Mine: 4M, implemented, with 47% less cycle time.", 12, ACC,
     space_before=4)
para(tf, "An updated training process", 14, TXT, bold=True,
     space_before=20)
para(tf, "How the next setters learn.", 12, SUB, space_before=2)
para(tf, "Mine: study material reviewed, schedule rebuilt.", 12, ACC,
     space_before=4)


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
# Outer cards reuse the panel names, so the two panels of the slide before
# morph into the first and last card, and back out into the next slide.
CARD_NAMES = ("!! Glass A", "!! Card 2", "!! Card 3", "!! Glass B")
for i, (letter, title, body) in enumerate(LEVELS):
    x = 0.28 + i * (CW + CG)
    glass(s, x, 1.85, CW, 3.35, name=CARD_NAMES[i])
    text(s, x + 0.34, 2.15, CW - 0.6, 0.6, letter, 30, ACC, bold=True)
    text(s, x + 0.34, 2.90, CW - 0.6, 0.7, title, 16, TXT, bold=True)
    tb, tf = txbox(s, x + 0.34, 3.72, CW - 0.62, 1.4)
    para(tf, body, 12, SUB, first=True, spacing=1.1)
glass(s, 0.28, 5.42, 12.77, 0.62, name="!! Strip")
text(s, 0.62, 5.62, 2.2, 0.25, "HOW IT WORKS", 11, ACC, bold=True)
text(s, 2.75, 5.60, 10.0, 0.3, "Each level builds on the one before: first "
     "understand, then do, then own it, then improve it.", 12, TXT)


# ============================================================ 5 · timeline
s = slide("Eight weeks in, fourteen to go", "Program timeline",
          light=True)
glass(s, 0.66, 2.36, 12.26, 2.78, name="!! Glass A")
LX0, LX1, LY = 0.84, 12.74, 3.89
rect(s, LX0, LY, LX1 - LX0, 0.02, fill=SUB, opacity=45)
MILES = [("Program start", "August 1, 2026", False),
         ("Today", "September 25", True),
         ("Level I exam", "October", False),
         ("Levels L and U", "November to December", False),
         ("Program close", "January 1, 2027", False)]
XS = [1.95 + k * 2.40 for k in range(5)]
rect(s, XS[0], LY - 0.005, XS[1] - XS[0], 0.03, fill=ACC)   # distance done
for k, (name, when, hot) in enumerate(MILES):
    cx = XS[k]
    d = 0.30 if hot else 0.21
    rect(s, cx - d / 2, LY + 0.01 - d / 2, d, d,
         fill=ACC if (hot or k == 0) else TXT, shape=MSO_SHAPE.OVAL)
    text(s, cx - 1.15, 2.84, 2.30, 0.45, name, 24 if hot else 19,
         ACC if hot else TXT, bold=True, align=PP_ALIGN.CENTER)
    text(s, cx - 1.15, 4.47, 2.30, 0.3, when, 14, ACC if hot else SUB,
         bold=hot, align=PP_ALIGN.CENTER)


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
        rect(s, 0.68, y - 0.14, 7.10, 0.01, fill=TXT, opacity=14)
    text(s, 0.68, y, 2.2, 0.35, t, 14, TXT, bold=True)
    text(s, 2.95, y + 0.01, 4.95, 0.35, d, 13, SUB)

glass(s, 8.55, 1.75, 4.50, 4.25, name="!! Glass B")
label(s, 8.95, 2.08, "HOW I LEARNED IT")
HOW = [("616", "pages of training manual and customer standard, read page "
               "by page."),
       ("Daily", "at the cells, attending robot stops with the technicians."),
       ("25", "questions of a practice exam from previous years, solved.")]
for k, (big, d) in enumerate(HOW):
    y = 2.55 + k * 1.10
    text(s, 8.95, y, 3.8, 0.5, big, 26, ACC, bold=True)
    text(s, 8.95, y + 0.50, 3.75, 0.5, d, 12, SUB)


# ============================================================ 7 · progress
s = slide("Four skills on my own, thirteen in progress",
          "Progress on the 23 skills of the program",
          light=True)
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
    text(s, 0.68, y - 0.06, 0.5, 0.5, lv, 24, ACC, bold=True)
    text(s, 1.22, y - 0.01, 2.2, 0.3, nm, 13, TXT, bold=True)
    text(s, 1.22, y + 0.26, 2.2, 0.3, "%d skills" % (own + wip + todo), 11,
         MUT)
    for j in range(own + wip + todo):
        x = 3.45 + j * DG
        if j < own:
            rect(s, x, y + 0.06, D, D, fill=ACC, shape=MSO_SHAPE.OVAL)
        elif j < own + wip:
            rect(s, x, y + 0.06, D, D, fill=ACC, opacity=35, line=ACC,
                 lw=1.25, shape=MSO_SHAPE.OVAL)
        else:
            rect(s, x, y + 0.06, D, D, line=SUB, lw=1.0, line_opacity=60,
                 shape=MSO_SHAPE.OVAL)
# legend, inside the panel and clear of the chevrons
LEG = [("On my own", dict(fill=ACC)),
       ("In progress", dict(fill=ACC, opacity=35, line=ACC, lw=1.25)),
       ("Not started", dict(line=SUB, lw=1.0, line_opacity=60))]
lx = 3.45
for t, kw in LEG:
    rect(s, lx, 5.52, 0.18, 0.18, shape=MSO_SHAPE.OVAL, **kw)
    text(s, lx + 0.28, 5.49, 1.4, 0.25, t, 11, SUB)
    lx += 1.62

glass(s, 8.85, 1.75, 4.20, 4.25, name="!! Glass B")
label(s, 9.25, 2.08, "ON MY OWN TODAY")
DONE = ["Changing consumables",
        "Adjusting the key welding settings",
        "Reading weld symbols on drawings",
        "Closing ANDON calls"]
for k, t in enumerate(DONE):
    y = 2.62 + k * 0.72
    rect(s, 9.25, y + 0.09, 0.14, 0.14, fill=ACC, shape=MSO_SHAPE.CHEVRON)
    text(s, 9.55, y, 3.3, 0.6, t, 14, TXT)
text(s, 9.25, 5.45, 3.6, 0.3, "Level I: exam still pending.", 12, ACC,
     bold=True)


# ============================================================ 8 · industrialization
s = slide("Most of my time goes to industrialization",
          "How my time is split")
glass(s, 0.28, 1.75, 5.20, 4.25, name="!! Glass A")
text(s, 0.68, 2.00, 4.4, 0.9, "60%", 54, ACC, bold=True)
text(s, 0.68, 2.95, 4.4, 0.6, "Industrialization project", 14, TXT,
     bold=True)
rect(s, 0.68, 3.75, 4.40, 0.16, fill=SUB, opacity=30)
rect(s, 0.68, 3.75, 4.40 * 0.6, 0.16, fill=ACC)
text(s, 0.68, 4.25, 4.4, 0.6, "40%", 30, TXT, bold=True)
tb, tf = txbox(s, 0.68, 4.85, 4.4, 0.9)
para(tf, "Support at the robotic cells: stops, adjustments and ANDON calls.",
     12, SUB, first=True)

glass(s, 5.80, 1.75, 7.25, 4.25, name="!! Glass B")
label(s, 6.20, 2.08, "THE PROJECT")
tb, tf = txbox(s, 6.20, 2.52, 6.45, 1.0)
para(tf, "Industrialization of the new Cummins project, together with "
     "Engineering.", 15, TXT, first=True, spacing=1.1)
lime_rule(s, 6.20, 3.62)
label(s, 6.20, 3.88, "WHAT INDUSTRIALIZATION MEANS")
tb, tf = txbox(s, 6.20, 4.32, 6.45, 1.3)
para(tf, "Getting a new process ready to run on the floor: equipment, "
     "programs, trials and the release to production.", 14, SUB,
     first=True, spacing=1.1)

# ============================================================ 9 · improvement idea
s = slide("Improvement idea: from a manual booth to the 4M cell",
          "Improvement idea and implementation  |  Done",
          light=True)
glass(s, 0.28, 1.75, 6.35, 4.25, name="!! Glass A")
IDEA = [("THE PROBLEM",
         "Long welds, done by hand in a manual booth: 146 s of welding per "
         "part, in uncomfortable positions."),
        ("THE IDEA",
         "Move the process to 4M, an automatic robotic cell, with the same "
         "fixture.")]
for k, (lab, d) in enumerate(IDEA):
    y = 2.08 + k * 1.22
    label(s, 0.68, y, lab)
    tb, tf = txbox(s, 0.68, y + 0.42, 5.6, 0.8)
    para(tf, d, 14, TXT, first=True, spacing=1.1)
lime_rule(s, 0.68, 4.60)
label(s, 0.68, 4.80, "IMPLEMENTED")
tb, tf = txbox(s, 0.68, 5.20, 5.6, 0.7)
para(tf, "Running in 4M today. The robot welds, and the team member only "
     "loads and unloads.", 14, ACC, bold=True, first=True, spacing=1.1)
photo(s, 6.98, 1.75, 6.07, 2.02, "Before: manual welding booth",
      "!! Glass B")
photo(s, 6.98, 3.98, 6.07, 2.02, "After: 4M robotic cell", "!! Glass C")

# ============================================================ 10 · results
s = slide("Same part, 47% less cycle time", "Improvement  |  Results, in "
          "seconds per part")
glass(s, 0.28, 1.75, 8.25, 4.25, name="!! Glass A")
label(s, 0.68, 2.08, "CYCLE TIME PER PART")
# (name, load, weld, unload): same fixture, so load and unload do not move
CT = [("Manual booth", 20, 146, 8, False),
      ("4M robotic cell", 20, 65, 8, True)]
SCALE = 6.0 / 174                        # inches per second
BX = 0.68
for k, (nm, ld, wd, ul, auto) in enumerate(CT):
    y = 2.62 + k * 1.22
    text(s, BX, y, 4.0, 0.3, nm, 14, TXT, bold=True)
    x = BX
    for sec, kind in ((ld, "load"), (wd, "weld"), (ul, "unload")):
        w = sec * SCALE
        if kind == "weld":
            seg = rect(s, x, y + 0.38, w - 0.03, 0.46,
                       fill=ACC if auto else SUB, opacity=100 if auto else 70)
            ink = NAVY
        else:
            seg = rect(s, x, y + 0.38, w - 0.03, 0.46, fill=SUB, opacity=30)
            ink = TXT
        seg.text_frame.vertical_anchor = MSO_ANCHOR.MIDDLE
        para(seg.text_frame, str(sec), 11 if sec < 10 else 12, ink,
             bold=True, first=True, align=PP_ALIGN.CENTER)
        x += w
    text(s, x + 0.12, y + 0.42, 1.3, 0.4, "%d s" % (ld + wd + ul), 20,
         ACC if auto else TXT, bold=True)
# legend
lx = BX
for t, kw in (("Load and unload", dict(fill=SUB, opacity=30)),
              ("Welding by hand", dict(fill=SUB, opacity=70)),
              ("Welding by robot", dict(fill=ACC))):
    rect(s, lx, 5.12, 0.18, 0.18, **kw)
    text(s, lx + 0.28, 5.09, 1.8, 0.25, t, 11, SUB)
    lx += 2.10
text(s, BX, 5.48, 7.5, 0.3, "Same fixture, so loading and unloading take "
     "the same time. The gain is all in the welding.", 11, MUT)

glass(s, 8.85, 1.75, 4.20, 4.25, name="!! Glass B")
KPI = [("−47%", "cycle time, 174 s to 93 s"),
       ("−56%", "welding time, 146 s to 65 s"),
       ("81 s", "saved on every part")]
for k, (big, lab) in enumerate(KPI):
    y = 2.02 + k * 1.30
    text(s, 9.25, y, 3.6, 0.6, big, 32, ACC, bold=True)
    text(s, 9.25, y + 0.66, 3.6, 0.3, lab, 12, SUB)

# ============================================================ 11 · training update
s = slide("What I would change in how setters learn",
          "Final project  |  Training process update, a proposal",
          light=True)
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
    glass(s, x, 1.85, 4.05, 3.35,
          name=("!! Glass A", "!! Card 2", "!! Glass B")[i])
    text(s, x + 0.36, 2.15, 3.3, 0.55, n, 30, ACC, bold=True)
    text(s, x + 0.36, 2.90, 3.3, 0.35, t, 16, TXT, bold=True)
    tb, tf = txbox(s, x + 0.36, 3.40, 3.33, 1.6)
    para(tf, d, 12, SUB, first=True, spacing=1.1)
glass(s, 0.28, 5.42, 12.77, 0.62, name="!! Strip")
text(s, 0.62, 5.62, 2.2, 0.25, "PROPOSAL", 11, ACC, bold=True)
text(s, 2.75, 5.60, 10.0, 0.3, "Use the corrected material and the new "
     "schedule with the next group of setters.", 12, TXT)


# ============================================================ 12 · next
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
        rect(s, 0.68, y - 0.18, 7.10, 0.01, fill=TXT, opacity=14)
    text(s, 0.68, y - 0.04, 0.5, 0.5, lv, 24, ACC, bold=True)
    text(s, 1.25, y, 6.5, 0.35, d, 14, TXT)
    text(s, 1.25, y + 0.36, 6.5, 0.3, when, 12, SUB)

glass(s, 8.55, 1.75, 4.50, 4.25, name="!! Glass B")
label(s, 8.95, 2.08, "WHAT I NEED")
NEED = ["A date for my Level I exam",
        "Supervised time programming robots in SKS",
        "Your feedback on the training proposal"]
for k, t in enumerate(NEED):
    y = 2.62 + k * 0.80
    rect(s, 8.95, y + 0.09, 0.14, 0.14, fill=ACC, shape=MSO_SHAPE.CHEVRON)
    text(s, 9.25, y, 3.55, 0.7, t, 14, TXT)


# ============================================================ 13 · conclusions
s = slide("Conclusions and next steps", "What is in place, and what comes next",
          light=True)
glass(s, 0.28, 1.75, 6.35, 4.05, name="!! Glass A")
label(s, 0.68, 2.08, "IN PLACE")
INPLACE = ["Level I studied in full, and four skills done on my own.",
           "A manual weld moved to the 4M robotic cell: 47% less cycle "
           "time, less strain.",
           "A reviewed study path, and a schedule that shows plan versus "
           "reality."]
glass(s, 6.98, 1.75, 6.07, 4.05, name="!! Glass B")
label(s, 7.38, 2.08, "NEXT")
NEXTS = ["Pass the Level I exam in October.",
         "Reach Levels L and U before January.",
         "Keep measuring 4M, and find the next weld to automate."]
for x, items, w in ((0.68, INPLACE, 5.55), (7.38, NEXTS, 5.27)):
    for k, t in enumerate(items):
        text(s, x, 2.62 + k * 1.00, w, 0.8, t, 14, TXT)
text(s, 0.68 if LIGHT else 3.40, 6.25, 9.6, 0.4, "I came to learn robotic welding, and I am "
     "already improving it.", 15, ACC, bold=True)


# ============================================================ 14 · closing
theme(False)
s = prs.slides.add_slide(layouts["Closing Slide"])
for p_ in list(s.placeholders):
    p_.element.getparent().remove(p_.element)
text(s, 0.0, 5.15, SW, 0.7, "Thank you", 30, WHITE, bold=True,
     align=PP_ALIGN.CENTER, name="!! Title 1")
text(s, 0.0, 5.95, SW, 0.25, "Diego Adair de León Márquez   |   Engineering "
     "Department", 13, GRAY, align=PP_ALIGN.CENTER)


# ---------------------------------------------------------------- key points
# Not a script: the few points Diego must not forget on each slide.
KEY = [
    ["Name, UPA, Mechatronics, 9th term.",
     "Program: Robotics Setter Technician, since August 1.",
     "Plant tutor: Fernando Robledo.",
     "Today: what the program asks, what I learned, the improvement."],
    ["Win: earn the trust of our people and customers.",
     "Trust is earned on the floor, not given by a title.",
     "My example: the longest, hardest weld is now on a robot (4M)."],
    ["Objective: specialist in robotic welding and technical support.",
     "Why: when a welding robot stops, the line stops.",
     "Final project has two halves, and I have both: 4M and the training "
     "update."],
    ["ILUO: understand, do with support, do alone, improve and teach.",
     "Same path the plant uses for any skill.",
     "Each level builds on the one before."],
    ["8 weeks done, 14 to go.",
     "Level I exam in October.",
     "No fixed training hours: I learn while attending real stops."],
    ["Five topics, in plain words: safety, GMAW, WPS, defects, parameters.",
     "WPS = the approved recipe for every weld.",
     "Learned from 616 pages, a practice exam and daily work at the cells."],
    ["Name the four on my own: consumables, key settings, weld symbols, "
     "ANDON.",
     "Level I studied in full, but the exam is still pending.",
     "4M counts as my first step in the improvement skill."],
    ["60% of my time: industrialization of the Cummins project.",
     "Industrialization = getting a new process ready for the floor.",
     "The other 40% at the cells is where the hands-on learning comes from."],
    ["Problem: 146 s of welding by hand, in uncomfortable positions.",
     "Idea: move it to 4M, same fixture.",
     "Done with my supervisor, and running today.",
     "The team member now only loads and unloads."],
    ["Cycle time 174 s to 93 s: 47% less.",
     "Welding 146 s to 65 s: 56% less.",
     "Load and unload did not change (same fixture), so the gain is all "
     "welding.",
     "81 s saved on every part, and less strain for the team member."],
    ["It is a proposal, not something already adopted.",
     "Example gap: 8 of 25 exam questions are TIG, and the matrix never "
     "names it.",
     "The error: the transfer mode table in the manual.",
     "The new schedule compares plan against reality, week by week."],
    ["L in November, U in December, O already started with 4M.",
     "Ask clearly: an exam date, supervised SKS time, feedback on the "
     "proposal."],
    ["Close with the lime line.",
     "Pause, then open for questions."],
    ["Thank them. Take questions."],
]
for i, pts in enumerate(KEY):
    notes[i] = "\n".join("• " + p for p in pts)

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
