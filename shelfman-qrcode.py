#! /usr/bin/python3
#
# QR Code Version 6: 41x41: L=134 M=106	Q=74 H=58	bytes
# QR Code Version 3: 29x29: L=53  M=42	Q=32 H=24	bytes. more fits, when only alphanumeric + space + $ % * + - . / :
#
# Version3 with box=4, border=0 results in 116 x 116 pixels.
#
# Requires:
#	sudo apt install python3-qrcode
#
# shelfman_qr.py V0.2
# (C) 2025, distribute under GPLv1
#
# v0.1, 2025-07-13, jw    - initial draught, qrcode only
# v0.2, 2025-12-21, jw    - size checking for printer, texts added.
# v0.3, 2025-12-22, jw    - added argparse and -p to call ptouch-print directly.

import sys, argparse, subprocess
import qrcode, uuid

# PIL is no overhead here, as qrcode uses PIL internally.
from PIL import Image, ImageDraw, ImageFont

version="0.3"


# config for brother D410
max_height = 120    # my tape can print 120, although the printer could print 128.
big_font_size = 48
small_font_size = 20
hspace = 16
vspace = 8
title_text = 'JW'
label_text = 'shelfman.de/'
code_text = '-'
outfile = "shelfman_guid_qr.png"

def parse_args():
    parser = argparse.ArgumentParser(description=f"Version {version} - Generate and print QR codes via ptouch-print")
    group = parser.add_mutually_exclusive_group()
    group.add_argument("-n", "--noop", action="store_true", help="Do nothing, just exercise internal mechanics")
    group.add_argument("-i", "--info", action="store_true", help="Call ptouch-print --info")
    group.add_argument("-p", "--print", dest="do_print", action="store_true", help="Generate QR code and send via ptouch-print")
    parser.add_argument("-o", "--output", default=outfile, help=f"Output file (default: {outfile})")
    # maybe restict the letter by adding below: choices=["X", "I", "C", "L"]
    parser.add_argument("letter", nargs="?", default="X", help="Label type: X=any, I=item, C=container, L=location (default: X)")
    return parser.parse_args()


# Generate a random UUID (GUID)
guid = uuid.uuid4()
xuid = bytes(x ^ y for x, y in zip(guid.bytes[:8], guid.bytes[8:])).hex()
uid16 = xuid[:8]+'-'+xuid[8:12]+'-'+xuid[12:]

args = parse_args()

if args.noop:
    print("NOOP: exercising internal mechanics only")
    sys.exit(0)

if args.info:
    sys.exit(subprocess.run(["ptouch-print", "--info"], check=True))


# Prepare the data
data = f"SFM-{args.letter}-{uid16}"

label_text = label_text + args.letter + '/'
code_text = uid16

# Create a QRCode object with version 6 or 3
qr = qrcode.QRCode(
    version=3,  # QR Code version 6 (41x41 modules), version 3 (29x29)
    error_correction=qrcode.constants.ERROR_CORRECT_Q,
    box_size=4,
    border=0,
)

# Add data and make the QR code
qr.add_data(data)
qr.make(fit=True)

# Generate the image
img = qr.make_image(fill_color="black", back_color="white")
(width, height) = img.get_image().size    # width may be wider
if height > max_height:
  print(f"QR Code height in pixel {height} does not fit into {max_height}", file=sys.stderr)
  sys.exit(1)
pad = int((max_height-height)/2) # we place the qr code vertically centered. And same left margin than top and bottom.

small_font = ImageFont.load_default(small_font_size)  # Aileron regular or built-in bitmap font.
big_font   = ImageFont.load_default(big_font_size)    # size does not apply when falling back to the buil-tin.

draw = ImageDraw.Draw(img)  # just for measuring
(_, _, title_w, _) = draw.textbbox((0, 0), title_text, font=big_font)
(_, _, label_w, _) = draw.textbbox((0, 0), label_text, font=small_font)
(_, _, code_w,  _) = draw.textbbox((0, 0), code_text,  font=small_font)

text_w = max(title_w, label_w, code_w)

img2 = Image.new(img.mode, (pad + width + hspace + text_w + hspace, max_height), color="white")
img2.paste(img.get_image(), (pad, pad))
draw = ImageDraw.Draw(img2)

y = vspace/2
draw.text((pad + width + hspace + (text_w - title_w)/2, y), title_text, font=big_font, fill='black')
y = y + big_font_size + 1.5 * vspace
draw.text((pad + width + hspace + (text_w - label_w)/2, y), label_text, font=small_font, fill='black')
y = y + small_font_size + vspace
draw.text((pad + width + hspace + (text_w - code_w)/2,  y), code_text,  font=small_font, fill='black')

# Save the image
img2.save(args.output)
l = len(data)
print(f"QR Code generated with {l} bytes of data {width} x {height}: {data} into file {args.output}")

if args.do_print:
    print("...printing ...")
    sys.exit(subprocess.run( ["ptouch-print", "--image", args.output], check=True))
