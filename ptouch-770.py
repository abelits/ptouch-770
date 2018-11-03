#!/usr/bin/python3

# Based on https://github.com/SquirrelCZE/ptouch-770
# On 12mm tape, the topmost 20px are not printed, the following 84 px are printed
# (as determined by experimentation using calibrate_12mm_tape.pbm made with GIMP)
# TODO: Check other tape sizes

from PIL import Image, ImageFont, ImageDraw
import os, subprocess, argparse

unprintable = 20

parser = argparse.ArgumentParser(description="Utility for printing on ptouch 700")
parser.add_argument('--text', type=str, help="string to be printed", default="Two\\nlines")
parser.add_argument('--height', type=int, help="height in pixels of loaded tape", default=84)
parser.add_argument('--rim', type=int, help="rim around the text in pixels", default=5)
parser.add_argument('--font', type=str, help="name of truetype font", default="FreeSansBold.ttf")

args = parser.parse_args()
max_height = args.height + unprintable

text = args.text.split("\\n")
print(text)

lines = len(text)

fontsize = int(round(args.height/lines,0))

font = ImageFont.truetype(args.font, fontsize)

# Determine the longest line
max_width = 0
for line in text:
    text_width, text_height = font.getsize(line)
    width = text_width + args.rim * 2
    if(width > max_width):
        max_width = width

image = Image.new("1", ( max_width, max_height ), 0)
draw = ImageDraw.Draw( image )

text_x = args.rim
text_y = unprintable-text_height
for line in text:
    text_y = text_y + text_height*0.9
    draw.text((text_x,text_y), line, fill=255, font=font)

out = os.path.abspath("out.pbm")
image.save(out)
tool = os.path.abspath(os.path.join(os.path.dirname(__file__), "ptouch-770-write"))

subprocess.run(["sudo", tool, out], check=True)
