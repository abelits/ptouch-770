#!/usr/bin/env python3.7

# Copyright (c) 2018-2020 Simon Peter
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


from PIL import Image, ImageFont, ImageDraw, ImageOps
import os, sys, subprocess, argparse
import gi

gi.require_version('Gtk', '3.0')
from gi.repository import Gtk

class MyWindow(Gtk.ApplicationWindow):
    def __init__(self, app):
        Gtk.Window.__init__(self, title="Label", application=app)
        self.lines = 0
        self.entries = []
        self.text = []
        self.unprintable = 20
        self.height = 84  # 12mm tape (tested)
        self.rim = 5
        self.font = os.path.dirname(__file__) + "/FreeSansBold.ttf"
        self.set_border_width(10)
        self.max_height = 128 # pixels; for 12mm TZe tape
        self.outfile = "/tmp/out.png"
        self.out = os.path.abspath(self.outfile)
        self.invert = False

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        for i in range(6):
            entry = Gtk.Entry()
            entry.connect("changed", self.on_text_changed)
            vbox.pack_start(entry, False, False, 0)
            self.entries.append(entry)

        fonts = [os.path.dirname(__file__) + "/FreeSans.ttf", os.path.dirname(__file__) + "/FreeSansBold.ttf"]
        fonts_combo = Gtk.ComboBoxText()
        fonts_combo.set_entry_text_column(0)
        for currency in fonts:
            fonts_combo.append_text(currency)
        vbox.pack_start(fonts_combo, False, False, 0)
        fonts_combo.connect("changed", self.on_fonts_combo_changed)

        devices = []
        for device in os.listdir("/dev/"):
            if "ulpt" in device:
                devices.append("/dev/" + device)
            if "rfcomm" in device:
                devices.append("/dev/" + device)

        self.devices_combo = Gtk.ComboBoxText()
        self.devices_combo.set_entry_text_column(0)
        for currency in devices:
            self.devices_combo.append_text(currency)
        vbox.pack_start(self.devices_combo, False, False, 0)

        self.inverted = Gtk.CheckButton("Invert")
        self.inverted.show()
        vbox.pack_start(self.inverted, False, False, 0)
        self.inverted.connect("toggled", self.on_button_toggled, "2")

        self.image = Gtk.Image()
        self.image.set_from_file(self.outfile)
        vbox.pack_start(self.image, False, False, 0)

        self.button = Gtk.Button.new_with_label("Print")
        self.button.connect("clicked", self.on_click_me_clicked)
        vbox.pack_start(self.button, False, False, 0)

        self.add(vbox)

    def on_fonts_combo_changed(self, combo):
        tree_iter = combo.get_active_iter()
        if tree_iter is not None:
            model = combo.get_model()
            row_id, name = model[tree_iter][:2]
            print("Selected: ID=%s" % (row_id))
            self.font = row_id
        else:
            entry = combo.get_child()
            print("Entered: %s" % entry.get_text())
        self.on_text_changed(None)


    def on_button_toggled(self, button, name):
        if button.get_active():
            self.invert = True
        else:
            self.invert = False
        print("Button", name, "was toggled")
        self.on_text_changed(None)

    def on_text_changed(self, entry):
        self.lines = 0
        self.text = []
        # Count the number of lines
        for entry in self.entries:
            if (entry.get_text() != ""):
                self.text.append(entry.get_text())
                self.lines = self.lines + 1

        fontsize = int(round(self.height / self.lines, 0))

        font = ImageFont.truetype(self.font, fontsize)

        # Determine the height in pixels based on letter "A"
        # rather than the actual text so as to not have varying
        # text heights just because some lines contain letters
        # such as "A, g" that have a different height
        text_unused_width, text_height = font.getsize("A")

        # Determine the longest line
        max_width = 0
        for line in self.text:
            text_width, text_unused_height = font.getsize(line)
            width = text_width + self.rim * 2
            if (width > max_width):
                max_width = width

        if(self.invert == True):
            foreground = 255
            background = 0
        else:
            foreground = 0
            background = 255

        self.working_image = Image.new("1", (max_width, self.max_height), background)
        draw = ImageDraw.Draw(self.working_image)

        text_x = self.rim
        text_y = self.unprintable - text_height
        for line in self.text:
            text_y = text_y + text_height * 0.9
            draw.text((text_x, text_y), line, fill=foreground, font=font)

        self.working_image.save(self.out)
        self.image.set_from_file(self.out)

    def on_click_me_clicked(self, button):
        self.working_image = self.working_image.rotate(90, expand=True)
        self.working_image = ImageOps.flip(self.working_image)

        self.working_image.save(self.out)

        tree_iter = self.devices_combo.get_active_iter()
        if tree_iter is not None:
            model = self.devices_combo.get_model()
            row_id, name = model[tree_iter][:2]
            print("Selected: ID=%s" % (row_id))
            device = row_id


        # TODO: Do this natively in Python so that 'convert' will no longer be needed
        #subprocess.run(["convert", self.out, "-monochrome", "-gravity", "center", "-crop", "128x", "-rotate", "90", "-flop", "/tmp/oout.png"])
        subprocess.run([os.path.dirname(__file__) + "/labelmaker.py", self.out, device])
        if os.path.exists(self.out):
            os.remove(self.out)

        #if os.path.exists("/tmp/oout.png"):
        #    os.remove("/tmp/oout.png")

class MyApplication(Gtk.Application):

    def __init__(self):
        Gtk.Application.__init__(self)

    def do_activate(self):
        win = MyWindow(self)
        win.show_all()

    def do_startup(self):
        Gtk.Application.do_startup(self)


app = MyApplication()
exit_status = app.run(sys.argv)
sys.exit(exit_status)
