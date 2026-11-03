from pathlib import Path

from docx import Document
from docx.enum.section import WD_SECTION
from docx.enum.table import WD_CELL_VERTICAL_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Inches, Pt, RGBColor


ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "design" / "Gloomthorn Story Mode Tutorial Design.docx"
CAPTURE_DIR = ROOT / "output" / "story-mode-design" / "captures"

BODY = RGBColor(39, 42, 40)
GREEN = RGBColor(46, 91, 75)
GREEN_DARK = RGBColor(30, 69, 57)
GOLD = RGBColor(151, 108, 40)
MUTED = RGBColor(102, 108, 104)
PALE = "E7EEE9"
PALE_GOLD = "F2E9D7"
WHITE = RGBColor(255, 255, 255)


MISSIONS = [
    {
        "title": "The Pearl Above the Stage",
        "chapter": "Book One, Chapter 1",
        "lesson": "Selection and movement",
        "roster": "Braun Stonefist",
        "setup": "Braun begins on Blackthorn's home edge. Two objective squares form a short diagonal route.",
        "steps": [
            "Select Braun to reveal his legal destinations.",
            "Move to the first gold objective square.",
            "Let Mirewatch take one action, then move to the second marker.",
        ],
        "story": "The player begins inside Blackthorn doctrine so the faction controlling Mirewatch also teaches the board's basic language. Victor frames position as ownership; Braun demonstrates it.",
        "success": "Braun occupies both marked squares in order.",
        "recovery": "Illegal destinations leave the board unchanged and the objective marker remains visible.",
    },
    {
        "title": "A Cage with a Bill of Sale",
        "chapter": "Book One, Chapter 2",
        "lesson": "Melee attacks and damage",
        "roster": "Blackthorn Lumberjack vs. Bull Gator",
        "setup": "The Lumberjack begins adjacent to a weakened harnessed bull gator on the Mirewatch road.",
        "steps": [
            "Select the Lumberjack and identify the red attack square.",
            "Attack the adjacent gator.",
            "Observe damage, death animation, and removal from the board.",
        ],
        "story": "The road attack from Chapter 2 becomes the first combat lesson. The Company treats a living creature as damaged equipment, reinforcing Blackthorn's worldview without rewriting the novel's protagonists.",
        "success": "The bull gator is destroyed.",
        "recovery": "The target remains highlighted until destroyed; Restart Mission restores the setup.",
    },
    {
        "title": "The Customs Bell",
        "chapter": "Book One, Chapter 2",
        "lesson": "Aim, fire, and lower weapon",
        "roster": "Goblin Sharpshooter vs. Resistance Smuggler",
        "setup": "The Sharpshooter and smuggler share an unobstructed rank at range three.",
        "steps": [
            "Select the Sharpshooter and use Aim; this hands one action to Mirewatch.",
            "On Blackthorn's next action, fire down the unobstructed three-square rank.",
            "Use Lower Weapon when the Sharpshooter must return to its movement state.",
        ],
        "story": "Reed and Donella's disguised cage reaches Blackthorn customs. The bell rings, and the tutorial shifts from close combat to the danger of being seen across open ground.",
        "success": "The Sharpshooter has aimed and the smuggler is destroyed by a legal ranged attack.",
        "recovery": "The contextual button continues to show Lower Weapon while the gun remains raised.",
    },
    {
        "title": "The Town Beneath the Company",
        "chapter": "Book One, Chapter 3",
        "lesson": "Cards, deployment, and controlled ground",
        "roster": "Braun Stonefist and Blackthorn Debt Collector",
        "setup": "Braun controls the home edge. The Debt Collector appears in hand and one legal home square is marked.",
        "steps": [
            "Select the Debt Collector card in hand.",
            "Review the green deployment highlights.",
            "Deploy on the marked, controlled home square.",
        ],
        "story": "Mirewatch's pumps, wages, ferries, and ledgers all point to Blackthorn. Deployment is presented as the Company turning controlled ground into an office.",
        "success": "A Blackthorn Debt Collector exists on the board.",
        "recovery": "Restart Mission returns the card to hand; invalid squares do not consume it.",
    },
    {
        "title": "Terms and Conditions",
        "chapter": "Book One, Chapter 4",
        "lesson": "Activated powers, stealth, summoning, and control",
        "roster": "Blackthorn Foreman, Goblin Ambusher, Blackthorn Lumberjack",
        "setup": "The Foreman, Ambusher, and Lumberjack begin in a compact formation opposite an active Mirewatch force.",
        "steps": [
            "Use Hide to dematerialize the Goblin Ambusher from the opponent's view.",
            "Use the Foreman's Create Lumberjack power while its front square is open.",
            "Preserve or expand Blackthorn control to at least fourteen squares while Mirewatch answers each action.",
        ],
        "story": "The novel's contracts and company rules become spatial pressure: Blackthorn controls choices by controlling the squares around them.",
        "success": "Hide and Create Lumberjack have both been used, and Blackthorn controls at least fourteen squares.",
        "recovery": "The live Lands counter and contextual power labels show which part of the objective remains.",
    },
    {
        "title": "Hospitality Is a Weapon",
        "chapter": "Book One, Chapter 5",
        "lesson": "Combined arms",
        "roster": "Braun, Goblin Ambusher, Goblin Sharpshooter vs. two defenders",
        "setup": "A close-range attacker and ranged attacker face an Informant and Bog Spearman on separate lanes.",
        "steps": [
            "Use Braun or the Ambusher to pressure close targets.",
            "Keep the Sharpshooter's lane clear.",
            "Destroy both Mirewatch defenders.",
        ],
        "story": "Polite rooms and offered shelter become tactical misdirection. The lesson asks the player to read the whole board rather than chase the nearest target.",
        "success": "No Mirewatch pieces remain.",
        "recovery": "The player may retry immediately; hints reinforce role separation rather than prescribe one solution.",
    },
    {
        "title": "The Bluewater Below",
        "chapter": "Book One, Chapter 6",
        "lesson": "Priority targets and protection",
        "roster": "Four Blackthorn units vs. Donella and two escorts",
        "setup": "Donella stands behind a Marshland Veteran and Bog Spearman. Blackthorn fields melee, ranged, and support-range pieces.",
        "steps": [
            "Identify which defenders block or punish each approach.",
            "Remove enough of the escort to open safe attacks.",
            "Concentrate damage on Donella.",
        ],
        "story": "Bluewater routes are promises kept beneath Blackthorn's maps. Donella embodies protection, making her the first explicit priority target.",
        "success": "Donella of the Marsh is defeated.",
        "recovery": "Donella remains gold-marked while alive; Restart Mission restores every escort.",
    },
    {
        "title": "How to Rob an Office Everyone Is Watching",
        "chapter": "Book One, Chapter 7",
        "lesson": "Full tactical encounter",
        "roster": "Blackthorn starter squad vs. Reed, Erevan, and Donella",
        "setup": "Five Blackthorn pieces face three named defenders across the full 8x8 board.",
        "steps": [
            "Establish firing lanes and protected approach squares.",
            "Remove vulnerable defenders without exposing the Sharpshooter.",
            "Defeat Reed, Erevan, and every remaining defender.",
        ],
        "story": "The first arc culminates in the office heist. Reed's plan, Erevan's violence, Donella's protection, and Victor's pursuit all become legible as tactical roles.",
        "success": "No Mirewatch pieces remain.",
        "recovery": "Restart Mission resets the complete encounter. Completion returns the player to the main menu.",
    },
]


def set_cell_shading(cell, fill):
    tc_pr = cell._tc.get_or_add_tcPr()
    shd = tc_pr.find(qn("w:shd"))
    if shd is None:
        shd = OxmlElement("w:shd")
        tc_pr.append(shd)
    shd.set(qn("w:fill"), fill)


def set_cell_margins(cell, top=80, start=120, bottom=80, end=120):
    tc_pr = cell._tc.get_or_add_tcPr()
    tc_mar = tc_pr.first_child_found_in("w:tcMar")
    if tc_mar is None:
        tc_mar = OxmlElement("w:tcMar")
        tc_pr.append(tc_mar)
    for tag, value in (("top", top), ("start", start), ("bottom", bottom), ("end", end)):
        node = tc_mar.find(qn(f"w:{tag}"))
        if node is None:
            node = OxmlElement(f"w:{tag}")
            tc_mar.append(node)
        node.set(qn("w:w"), str(value))
        node.set(qn("w:type"), "dxa")


def set_table_geometry(table, widths_dxa):
    table.autofit = False
    total = sum(widths_dxa)
    tbl_pr = table._tbl.tblPr
    tbl_w = tbl_pr.find(qn("w:tblW"))
    if tbl_w is None:
        tbl_w = OxmlElement("w:tblW")
        tbl_pr.append(tbl_w)
    tbl_w.set(qn("w:w"), str(total))
    tbl_w.set(qn("w:type"), "dxa")
    tbl_ind = tbl_pr.find(qn("w:tblInd"))
    if tbl_ind is None:
        tbl_ind = OxmlElement("w:tblInd")
        tbl_pr.append(tbl_ind)
    tbl_ind.set(qn("w:w"), "120")
    tbl_ind.set(qn("w:type"), "dxa")

    grid = table._tbl.tblGrid
    for child in list(grid):
        grid.remove(child)
    for width in widths_dxa:
        col = OxmlElement("w:gridCol")
        col.set(qn("w:w"), str(width))
        grid.append(col)
    for row in table.rows:
        for index, cell in enumerate(row.cells):
            cell.width = Inches(widths_dxa[index] / 1440)
            tc_w = cell._tc.get_or_add_tcPr().find(qn("w:tcW"))
            if tc_w is None:
                tc_w = OxmlElement("w:tcW")
                cell._tc.get_or_add_tcPr().append(tc_w)
            tc_w.set(qn("w:w"), str(widths_dxa[index]))
            tc_w.set(qn("w:type"), "dxa")
            set_cell_margins(cell)
            cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER


def set_run(run, size=None, color=BODY, bold=None, italic=None):
    run.font.name = "Calibri"
    run._element.get_or_add_rPr().rFonts.set(qn("w:ascii"), "Calibri")
    run._element.get_or_add_rPr().rFonts.set(qn("w:hAnsi"), "Calibri")
    if size is not None:
        run.font.size = Pt(size)
    run.font.color.rgb = color
    if bold is not None:
        run.bold = bold
    if italic is not None:
        run.italic = italic


def add_page_field(paragraph):
    run = paragraph.add_run()
    begin = OxmlElement("w:fldChar")
    begin.set(qn("w:fldCharType"), "begin")
    instr = OxmlElement("w:instrText")
    instr.set(qn("xml:space"), "preserve")
    instr.text = " PAGE "
    separate = OxmlElement("w:fldChar")
    separate.set(qn("w:fldCharType"), "separate")
    text = OxmlElement("w:t")
    text.text = "1"
    end = OxmlElement("w:fldChar")
    end.set(qn("w:fldCharType"), "end")
    run._r.extend([begin, instr, separate, text, end])
    set_run(run, size=9, color=MUTED)


def add_label_paragraph(doc, label, text, keep=False):
    p = doc.add_paragraph()
    p.paragraph_format.space_after = Pt(6)
    p.paragraph_format.line_spacing = 1.25
    p.paragraph_format.keep_with_next = keep
    r = p.add_run(label + ": ")
    set_run(r, bold=True, color=GREEN_DARK)
    set_run(p.add_run(text))
    return p


def add_bullet(doc, text):
    p = doc.add_paragraph(style="List Bullet")
    p.paragraph_format.left_indent = Inches(0.375)
    p.paragraph_format.first_line_indent = Inches(-0.188)
    p.paragraph_format.space_after = Pt(4)
    p.paragraph_format.line_spacing = 1.25
    for run in p.runs:
        set_run(run)
    if not p.runs:
        set_run(p.add_run(text))
    else:
        p.runs[0].text = text
    return p


def add_callout(doc, title, body, fill=PALE_GOLD):
    table = doc.add_table(rows=1, cols=1)
    set_table_geometry(table, [9360])
    cell = table.cell(0, 0)
    set_cell_shading(cell, fill)
    p = cell.paragraphs[0]
    p.paragraph_format.space_after = Pt(2)
    r = p.add_run(title)
    set_run(r, size=11, bold=True, color=GOLD)
    p2 = cell.add_paragraph()
    p2.paragraph_format.space_after = Pt(0)
    p2.paragraph_format.line_spacing = 1.2
    set_run(p2.add_run(body), size=10.5)
    doc.add_paragraph().paragraph_format.space_after = Pt(2)


def setup_styles(doc):
    section = doc.sections[0]
    section.page_width = Inches(8.5)
    section.page_height = Inches(11)
    section.top_margin = Inches(0.8)
    section.bottom_margin = Inches(0.8)
    section.left_margin = Inches(1)
    section.right_margin = Inches(1)
    section.header_distance = Inches(0.35)
    section.footer_distance = Inches(0.35)

    normal = doc.styles["Normal"]
    normal.font.name = "Calibri"
    normal.font.size = Pt(11)
    normal.font.color.rgb = BODY
    normal.paragraph_format.space_after = Pt(6)
    normal.paragraph_format.line_spacing = 1.25
    for style_name, size, color, before, after in (
        ("Heading 1", 16, GREEN, 18, 10),
        ("Heading 2", 13, GREEN, 14, 7),
        ("Heading 3", 12, GREEN_DARK, 10, 5),
    ):
        style = doc.styles[style_name]
        style.font.name = "Calibri"
        style.font.size = Pt(size)
        style.font.bold = True
        style.font.color.rgb = color
        style.paragraph_format.space_before = Pt(before)
        style.paragraph_format.space_after = Pt(after)
        style.paragraph_format.keep_with_next = True

    header = section.header.paragraphs[0]
    header.alignment = WD_ALIGN_PARAGRAPH.RIGHT
    set_run(header.add_run("GLOOMTHORN | STORY MODE DESIGN"), size=8.5, bold=True, color=MUTED)
    footer = section.footer.paragraphs[0]
    footer.alignment = WD_ALIGN_PARAGRAPH.CENTER
    set_run(footer.add_run("Bayou Bonanza  |  Story Mode Tutorial  |  "), size=9, color=MUTED)
    add_page_field(footer)


def add_cover(doc):
    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(74)
    p.paragraph_format.space_after = Pt(14)
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    set_run(p.add_run("GLOOMTHORN"), size=14, bold=True, color=GOLD)

    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_after = Pt(10)
    set_run(p.add_run("Story Mode Tutorial Design"), size=30, bold=True, color=GREEN_DARK)

    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_after = Pt(28)
    set_run(p.add_run("A single-player campaign for the 8x8 tactical card game"), size=15, color=GREEN)

    add_callout(
        doc,
        "Design intent",
        "Teach the real multiplayer rules through short story scenarios, beginning with the Blackthorn starter roster and increasing complexity one decision at a time.",
        fill=PALE,
    )

    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(24)
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    set_run(p.add_run("Narrative structure: Gloomthorn - Book One - Revised Edition"), size=11, italic=True, color=MUTED)
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    set_run(p.add_run("Implementation scope: playable opening arc, Chapters 1-7"), size=11, color=MUTED)
    doc.add_page_break()


def add_overview(doc):
    doc.add_heading("Design Summary", level=1)
    doc.add_paragraph(
        "Story Mode is the game's single-player tutorial and narrative campaign. Its authored setups feed directly into the authoritative multiplayer GameEngine, preserving the same card definitions, health, powers, movement rules, targeting, damage, concealment, control, costs, traits, victory checks, and deployment interactions. Mirewatch decisions use the multiplayer AI, so the tutorial never teaches a substitute ruleset that must later be unlearned."
    )
    add_callout(
        doc,
        "Narrative point of view",
        "The opening lessons place the player inside Blackthorn field doctrine. This satisfies the need to begin with the Blackthorn starter deck while making the moral tension explicit: the player learns how the occupying Company thinks before the story increasingly centers Reed, Erevan, Donella, and Mirewatch's resistance.",
    )

    doc.add_heading("Learning Model", level=2)
    for text in (
        "One new concept per early mission; later missions recombine earlier concepts.",
        "Gold objective markers show the immediate goal without drawing a route for the player.",
        "Green and red legal-square highlights remain the primary rules language.",
        "Every normal move, attack, card play, or activated power hands the next action to the opponent. Command, Relentless, and discard timing retain their normal multiplayer exceptions.",
        "Mirewatch chooses one legal response at a time, including attacks, cards, movement, stance changes, concealment, and summoning when those options improve its position.",
        "Character dialogue supplies motive and tone; the coaching panel supplies the actionable instruction.",
        "Restart Mission is always available. Failure costs time, not campaign progress.",
        "Completion persists locally and unlocks the next chapter briefing.",
    ):
        add_bullet(doc, text)

    doc.add_page_break()
    doc.add_heading("Implemented Campaign at a Glance", level=2)
    table = doc.add_table(rows=1, cols=4)
    table.style = "Table Grid"
    set_table_geometry(table, [720, 3120, 2520, 3000])
    headers = ["#", "Mission", "Primary lesson", "Completion"]
    for index, value in enumerate(headers):
        cell = table.rows[0].cells[index]
        set_cell_shading(cell, PALE)
        p = cell.paragraphs[0]
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER if index == 0 else WD_ALIGN_PARAGRAPH.LEFT
        set_run(p.add_run(value), size=10, bold=True, color=GREEN_DARK)
    for index, mission in enumerate(MISSIONS, 1):
        row = table.add_row().cells
        values = [str(index), mission["title"], mission["lesson"], mission["success"]]
        for column, value in enumerate(values):
            p = row[column].paragraphs[0]
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER if column == 0 else WD_ALIGN_PARAGRAPH.LEFT
            set_run(p.add_run(value), size=9.5)
    set_table_geometry(table, [720, 3120, 2520, 3000])

    doc.add_page_break()
    doc.add_heading("Presentation Pattern", level=2)
    doc.add_paragraph(
        "Each mission opens with three click-through briefing beats. A large character portrait establishes the speaker, a speech bubble carries adapted dialogue, and a compact lesson plate states the mechanic and objective. The board then repeats the objective in a persistent coach panel."
    )
    briefing = CAPTURE_DIR / "01-story-briefing.png"
    deployment = CAPTURE_DIR / "02-story-deployment.png"
    if briefing.exists():
        p = doc.add_paragraph()
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        p.add_run().add_picture(str(briefing), width=Inches(5.75))
        caption = doc.add_paragraph("Figure 1. Character-led mission briefing with explicit lesson and objective.")
        caption.alignment = WD_ALIGN_PARAGRAPH.CENTER
        caption.runs[0].italic = True
        caption.runs[0].font.color.rgb = MUTED
    if deployment.exists():
        p = doc.add_paragraph()
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        p.add_run().add_picture(str(deployment), width=Inches(5.75))
        caption = doc.add_paragraph("Figure 2. Deployment lesson using the live hand, controlled squares, and objective marker.")
        caption.alignment = WD_ALIGN_PARAGRAPH.CENTER
        caption.runs[0].italic = True
        caption.runs[0].font.color.rgb = MUTED


def add_missions(doc):
    doc.add_page_break()
    doc.add_heading("Playable Opening Arc", level=1)
    doc.add_paragraph(
        "The following eight missions are implemented in the client. Dialogue is original tutorial copy adapted to the novel's events and characters; it is not presented as verbatim novel text."
    )
    for index, mission in enumerate(MISSIONS, 1):
        if index > 1:
            doc.add_page_break()
        doc.add_heading(f"Mission {index}: {mission['title']}", level=2)
        add_label_paragraph(doc, "Story source", mission["chapter"])
        add_label_paragraph(doc, "Mechanic", mission["lesson"])
        add_label_paragraph(doc, "Playable roster", mission["roster"])
        add_label_paragraph(doc, "Board setup", mission["setup"])
        add_label_paragraph(doc, "Narrative purpose", mission["story"])
        doc.add_heading("Player sequence", level=3)
        for step in mission["steps"]:
            add_bullet(doc, step)
        add_label_paragraph(doc, "Success", mission["success"])
        add_label_paragraph(doc, "Recovery", mission["recovery"])


def add_expansion(doc):
    doc.add_page_break()
    doc.add_heading("Book One Expansion Roadmap", level=1)
    doc.add_paragraph(
        "The opening arc deliberately stops after Chapter 7, once the player can read and operate the board. Later missions should continue the novel while shifting from instruction to authored tactical challenges."
    )
    phases = [
        ("Act II: Consequences", "Chapters 8-11", "Status effects, recovery, activated abilities, turn timing", "The Cost of Being Seen; The Man Who Wants Out; Twenty Debtors; The Lesson at Night"),
        ("Act III: The Shrouded Vault", "Chapters 12-16", "Spells, hidden information, multi-stage objectives, acceptable losses", "Invitations; A Beautiful Plan; No Plan Saves Everyone; The Mystery Was Published; Wounds That Vote"),
        ("Act IV: Charter Day", "Chapters 17-19", "Resources, structures, taxes, public control thresholds", "Making Credit; The Public Lie; Title Follows Burden"),
        ("Act V: Beyond Mirewatch", "Chapters 20-24", "Faction switching, traits, unusual movement, escort maps", "The Stair Between Gossiping Trees; A Factory in Heaven; The Memory That Contradicts; Allies Acquired Dishonestly; The Road That Keeps One"),
        ("Act VI: The Choice", "Chapters 25-30", "Full decks, asymmetric victory conditions, boss logic, final matches", "The Monster Has Rules through A Town That Owns Itself"),
    ]
    table = doc.add_table(rows=1, cols=4)
    table.style = "Table Grid"
    set_table_geometry(table, [1800, 1320, 2880, 3360])
    for index, header in enumerate(("Campaign act", "Novel span", "Rules introduced", "Scenario spine")):
        set_cell_shading(table.rows[0].cells[index], PALE)
        set_run(table.rows[0].cells[index].paragraphs[0].add_run(header), size=10, bold=True, color=GREEN_DARK)
    for phase in phases:
        cells = table.add_row().cells
        for index, value in enumerate(phase):
            set_run(cells[index].paragraphs[0].add_run(value), size=9.5)
    set_table_geometry(table, [1800, 1320, 2880, 3360])

    doc.add_heading("Recommended Scenario Types", level=2)
    for text in (
        "Escape: reach a board edge with at least one named unit.",
        "Escort: move a vulnerable objective piece while maintaining adjacent protection.",
        "Holdout: control specified squares for a fixed number of turns.",
        "Heist: interact with structures in sequence while avoiding or delaying defenders.",
        "Public choice: choose between mutually exclusive objectives that alter later dialogue or loadouts.",
        "Boss encounter: defeat a named hero whose action state changes as health thresholds are crossed.",
        "Full match: use a legal deck and standard victory rules with only narrative presentation layered on top.",
    ):
        add_bullet(doc, text)


def add_ui_and_qa(doc):
    doc.add_page_break()
    doc.add_heading("Dialogue and Interface Rules", level=1)
    doc.add_heading("Briefings", level=2)
    for text in (
        "Three beats maximum before early tutorial missions; later story missions may use a fourth only for a player choice.",
        "One speaker per beat, one portrait, and no more than roughly forty words in the speech bubble.",
        "Dialogue carries character and stakes. Mechanical instructions stay in the lesson plate and coach panel.",
        "Use adapted original tutorial dialogue unless a line is intentionally quoted and reviewed against the novel.",
    ):
        add_bullet(doc, text)

    doc.add_heading("In-match coaching", level=2)
    for text in (
        "Keep the objective visible until completion.",
        "Highlight only the current target; do not pre-solve a route.",
        "Use the existing legal-move colors so Story Mode and multiplayer remain visually consistent.",
        "When cards are in hand, reserve clear bottom-bar space so cards never cover the coaching text.",
        "End Turn functions as the normal multiplayer Pass action. Restart Mission stays separate, while Back is isolated in the left gutter.",
        "Completion changes End Turn to Continue Story; the last mission changes it to Finish Story.",
    ):
        add_bullet(doc, text)

    doc.add_heading("Progression and persistence", level=2)
    doc.add_paragraph(
        "Completion count is stored locally in the SteamTactics application-data directory. Opening Story Mode resumes at the newest unlocked mission. Restarting a mission does not reduce progress. The persistence format is intentionally small so it can later be migrated to account-backed campaign state without changing scenario data."
    )

    doc.add_page_break()
    doc.add_heading("Acceptance Checklist", level=1)
    checks = [
        ("Build", "Debug client compiles from build_debug_client.bat."),
        ("Offline entry", "Story Mode opens without matchmaking or game services."),
        ("Rules fidelity", "Movement, attack, deployment, damage, and control use shared game rules."),
        ("Unit fidelity", "Story units retain their normal health, action profiles, keywords, and activated powers."),
        ("Action cadence", "Every normal player action yields exactly one computer action before control returns, subject only to normal Command, Relentless, and discard exceptions."),
        ("Specialists", "Ambusher can Hide and Reveal; Foreman can create Lumberjacks; Sharpshooter can Aim, Fire, and Lower Weapon."),
        ("Opponent turns", "Mirewatch units pursue legal attacks, useful powers, healing, and positional advances."),
        ("Failure", "Losing the Blackthorn force or Braun in later missions produces a clear retry state."),
        ("Visual hierarchy", "Briefing title, speaker, dialogue, lesson, and action prompt do not overlap."),
        ("Board readability", "Objective marker does not obscure pieces or legal-action highlights."),
        ("Deployment", "Hand cards remain selectable and never cover coaching copy."),
        ("Recovery", "Restart Mission restores the original scenario state."),
        ("Progress", "Completing a mission unlocks the next briefing and survives a client restart."),
        ("Finale", "Completing Mission 8 returns cleanly to the main menu."),
    ]
    table = doc.add_table(rows=1, cols=2)
    table.style = "Table Grid"
    set_table_geometry(table, [1800, 7560])
    for index, header in enumerate(("Check", "Pass condition")):
        set_cell_shading(table.rows[0].cells[index], PALE)
        set_run(table.rows[0].cells[index].paragraphs[0].add_run(header), size=10, bold=True, color=GREEN_DARK)
    for label, detail in checks:
        cells = table.add_row().cells
        set_run(cells[0].paragraphs[0].add_run(label), size=10, bold=True, color=GREEN_DARK)
        set_run(cells[1].paragraphs[0].add_run(detail), size=10)
    set_table_geometry(table, [1800, 7560])

    add_callout(
        doc,
        "Scope boundary",
        "The eight opening missions are playable now. The Chapter 8-30 material above is the designed continuation roadmap, not a claim that those later scenarios are already implemented.",
    )


def build():
    doc = Document()
    setup_styles(doc)
    add_cover(doc)
    add_overview(doc)
    add_missions(doc)
    add_expansion(doc)
    add_ui_and_qa(doc)
    doc.core_properties.title = "Gloomthorn Story Mode Tutorial Design"
    doc.core_properties.subject = "Single-player tutorial campaign design for Bayou Bonanza"
    doc.core_properties.author = "Tandem Tales"
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    doc.save(OUTPUT)
    print(OUTPUT)


if __name__ == "__main__":
    build()
