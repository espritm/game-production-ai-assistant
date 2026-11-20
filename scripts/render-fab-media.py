"""Render original vector-style branding and gallery layouts (Pillow, development only)."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import json

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs/fab/media"
OUT.mkdir(parents=True, exist_ok=True)
BG, PANEL, WHITE, MUTED, MINT = "#101C29", "#1A2C3D", "#F2F6FA", "#A9BDCF", "#6CE5C0"

def font(size, bold=False):
    return ImageFont.truetype("C:/Windows/Fonts/segoeui" + ("b" if bold else "") + ".ttf", size)

def icon(size):
    im = Image.new("RGB", (512, 512), BG)
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((70, 78, 442, 380), radius=64, outline=MINT, width=20)
    d.line([(160, 380), (160, 440), (235, 380)], fill=MINT, width=20, joint="curve")
    d.line([(204, 168), (151, 224), (204, 280)], fill=WHITE, width=18, joint="curve")
    d.line([(308, 168), (361, 224), (308, 280)], fill=WHITE, width=18, joint="curve")
    for y in (194, 254):
        d.ellipse((245, y-10, 265, y+10), fill=MINT)
    return im.resize((size, size), Image.Resampling.LANCZOS)

icon(128).save(ROOT / "plugin/Resources/Icon128.png")
icon(512).save(OUT / "icon-512.png")
(OUT / "icon.svg").write_text('''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512"><rect width="512" height="512" rx="64" fill="#101c29"/><g fill="none" stroke-linejoin="round"><rect x="70" y="78" width="372" height="302" rx="64" stroke="#6ce5c0" stroke-width="20"/><path d="M160 380v60l75-60" stroke="#6ce5c0" stroke-width="20"/><path d="M204 168l-53 56 53 56m104-112 53 56-53 56" stroke="#f2f6fa" stroke-width="18"/></g><g fill="#6ce5c0"><circle cx="255" cy="194" r="10"/><circle cx="255" cy="254" r="10"/></g></svg>''', encoding="utf-8")

def base(kicker):
    im = Image.new("RGB", (1920, 1080), BG)
    d = ImageDraw.Draw(im)
    d.line((90, 84, 1830, 84), fill="#30495D", width=2)
    d.text((90, 38), "GPAA  /  GAME PRODUCTION AI ASSISTANT", font=font(23, True), fill=WHITE)
    d.text((90, 980), "MAXIME ESPRIT   /   WINDOWS x64   /   UNREAL EDITOR", font=font(23), fill=MUTED)
    d.text((90, 146), kicker.upper(), font=font(23, True), fill=MINT)
    return im, d

im, d = base("Find the context. Keep creating.")
d.text((90, 233), "Game Production\nAI Assistant", font=font(92, True), fill=WHITE, spacing=3)
d.text((96, 500), "Local project search.\nLocal or cloud answers with references.", font=font(40), fill=MUTED, spacing=16)
im.paste(icon(420), (1370, 245))
for x, title, subtitle in [(96, "SEARCH LOCALLY", "Code + asset metadata"), (690, "CHOOSE YOUR AI", "Local or cloud"), (1280, "QUICK START", "Verified model download")]:
    d.rounded_rectangle((x, 730, x+535, 895), radius=18, fill=PANEL)
    d.text((x+28, 760), title, font=font(26, True), fill=MINT)
    d.text((x+28, 814), subtitle, font=font(27), fill=WHITE)
d.text((96, 635), "FREE PLUGIN  /  Cloud API usage billed separately", font=font(25, True), fill=MINT)
im.save(OUT / "01-cover.png", optimize=True)

capture = OUT / "captures/prompt-v0.3.png"
if capture.exists():
    im, d = base("Inside your Unreal Editor")
    d.text((90, 250), "Ask with\nproject context.", font=font(73, True), fill=WHITE, spacing=10)
    d.text((94, 490), "Streamed answers\nDistinct user messages\nLocal model or your provider", font=font(34), fill=MUTED, spacing=28)
    d.text((94, 770), "Compiled Slate interface preview\nExample conversation, not a model evaluation.", font=font(23), fill=MUTED, spacing=10)
    shot = Image.open(capture).convert("RGB")
    shot.thumbnail((1000, 800), Image.Resampling.LANCZOS)
    im.paste(shot, (830, 180))
    im.save(OUT / "02-in-editor.png", optimize=True)

im, d = base("Choose what stays local")
d.text((90, 230), "Local search. Local or cloud answers.", font=font(72, True), fill=WHITE)
for x, title, lines in [(90, "ON YOUR PC", ["Local search + CPU generation", "Models downloaded on request", "No prompts sent to an AI provider", "Quick Start with integrity checks"]), (1000, "YOUR CLOUD PROVIDER", ["Your question + selected excerpts", "Explicit context-sharing consent", "Personal key in Windows Credential Manager", "Separate provider account and API charges"])]:
    d.rounded_rectangle((x, 390, x+830, 850), radius=24, fill=PANEL)
    d.text((x+36, 428), title, font=font(32, True), fill=MINT)
    for i, text in enumerate(lines):
        d.text((x+36, 515+i*70), text, font=font(28), fill=WHITE)
d.text((96, 895), "Quality depends on model capability. Cloud provider terms and charges apply.", font=font(28), fill=MUTED)
im.save(OUT / "03-local-and-cloud.png", optimize=True)

images = []
for p in sorted(OUT.glob("0*.png")):
    with Image.open(p) as img:
        assert img.size == (1920, 1080)
    assert p.stat().st_size < 3_000_000
    images.append({"file": p.name, "width": 1920, "height": 1080, "bytes": p.stat().st_size})
assert sum(p["bytes"] for p in images) < 25_000_000
(OUT / "media-manifest.json").write_text(json.dumps({"images": images, "source": "Original GPAA layouts; compiled Slate v0.3 interface preview with an explicitly seeded example conversation", "captureCrop": None, "generativeAIImages": False}, indent=2), encoding="utf-8")
print(json.dumps(images))
