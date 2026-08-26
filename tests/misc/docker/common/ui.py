#!/usr/bin/env python3
# One screen that composes a sandbox.sh test command, runs it and shows the
# screens of what failed.  Mouse: a click marks an item or presses a button,
# the wheel scrolls.  Keyboard: arrows, Tab, Space, Enter, q.
#
# Nothing lives here that the command line cannot do; the choices are kept in
# reports/last.ui so that the next time starts where this one ended.
#
# usage: sandbox.sh [env] ui

import curses
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STATE = os.path.join(ROOT, "reports", "last.ui")
TRANSPORTS = [("local", "a local panel, no server"), ("sftp", "the sftp plugin, a stream"),
              ("sh", "shell-link, a stream"), ("ftp", "the ftp plugin, a local copy"),
              ("smb", "the samba plugin, a local copy")]
LOCALES = [("ru_RU.UTF-8", "UTF-8, Cyrillic"), ("en_US.UTF-8", "UTF-8"),
           ("ru_RU.KOI8-R", "8-bit, Cyrillic"), ("C", "no locale at all")]


def ini_sections(path):
    return re.findall(r"^\[(.+)\]$", open(path).read(), re.M)


def toggles():
    out = []
    for line in open(os.path.join(ROOT, "common", "toggles.ini")):
        line = line.strip()
        if not line or line.startswith("#") or "|" not in line:
            continue
        left, desc = line.split("|", 1)
        tag, value = left.split("=", 1)
        out.append((tag.strip(), value.strip(), desc.strip()))
    return out


def envs():
    out = []
    for n in sorted(os.listdir(os.path.join(ROOT, "envs"))):
        d = os.path.join(ROOT, "envs", n)
        if os.path.isfile(os.path.join(d, "docker-compose.yml")):
            try:
                first = open(os.path.join(d, "README.md")).readline().strip("# \n")
            except OSError:
                first = ""
            out.append((n, first))
    return out


def subjects():
    return sorted(n for n in os.listdir(os.path.join(ROOT, "cases"))
                  if os.path.isfile(os.path.join(ROOT, "cases", n, "fixtures.sh")))


def case_dirs(subject):
    text = open(os.path.join(ROOT, "cases", subject, "fixtures.sh")).read()
    return re.findall(r"^mkdir -p ([0-9][^ \n]*)$", text, re.M)


def load_state():
    st = {"env": "debian-12", "subject": "archives", "transports": "local",
          "locale": "ru_RU.UTF-8", "profile": "keep", "toggles": "", "extra": "",
          "keymap": "none", "dirs": ""}
    try:
        for line in open(STATE):
            if "=" in line:
                k, v = line.rstrip("\n").split("=", 1)
                st[k] = v.strip('"')
    except OSError:
        pass
    return st


def save_state(st):
    os.makedirs(os.path.dirname(STATE), exist_ok=True)
    with open(STATE, "w") as f:
        for k in ("env", "subject", "transports", "locale", "profile", "toggles",
                  "extra", "keymap", "dirs"):
            f.write("%s=%s\n" % (k, st[k]))


class Item:
    """One line on the screen: a radio or check entry, a text field, a button."""

    def __init__(self, kind, group, key, label, desc=""):
        self.kind, self.group, self.key, self.label, self.desc = kind, group, key, label, desc
        self.y = self.x = self.w = 0  # where it was drawn last, for the mouse


def colors():
    """mc's dialog look when the terminal has colours, attributes otherwise."""
    c = {"dialog": 0, "title": curses.A_BOLD, "focus": curses.A_REVERSE, "button": curses.A_REVERSE,
         "dim": curses.A_DIM, "head": curses.A_BOLD}
    if curses.has_colors():
        curses.start_color()
        curses.use_default_colors()
        curses.init_pair(1, curses.COLOR_WHITE, curses.COLOR_BLUE)
        curses.init_pair(2, curses.COLOR_BLACK, curses.COLOR_CYAN)
        curses.init_pair(3, curses.COLOR_YELLOW, curses.COLOR_BLUE)
        curses.init_pair(4, curses.COLOR_BLACK, curses.COLOR_CYAN)
        curses.init_pair(5, curses.COLOR_CYAN, curses.COLOR_BLUE)
        c = {"dialog": curses.color_pair(1), "title": curses.color_pair(3) | curses.A_BOLD,
             "focus": curses.color_pair(2), "button": curses.color_pair(4),
             "dim": curses.color_pair(5), "head": curses.color_pair(3)}
    return c


class Form:
    def __init__(self, stdscr, env):
        self.scr = stdscr
        self.st = load_state()
        if env:
            self.st["env"] = env
        # a rebuild is asked for each time, not remembered: it costs minutes
        self.radios = {"env": self.st["env"], "subject": self.st["subject"],
                       "locale": self.st["locale"], "profile": "keep",
                       "keymap": self.st["keymap"] or "none"}
        self.checks = {"transports": set(filter(None, self.st["transports"].split(","))),
                       "toggles": set(filter(None, self.st["toggles"].split(","))),
                       "dirs": set(filter(None, self.st["dirs"].split(",")))}
        self.texts = {"extra": self.st["extra"], "profile_text": ""}
        self.cursor = 0
        self.top = 0
        self.build()

    def build(self):
        it = []
        it.append(Item("head", None, None, "Environment"))
        for n, d in envs():
            it.append(Item("radio", "env", n, n, d))
        it.append(Item("head", None, None, "Subject"))
        for n in subjects():
            it.append(Item("radio", "subject", n, n, "cases/" + n))
        it.append(Item("head", None, None, "Transports"))
        for n, d in TRANSPORTS:
            it.append(Item("check", "transports", n, n, d))
        it.append(Item("head", None, None, "Locale (messages stay English)"))
        for n, d in LOCALES:
            it.append(Item("radio", "locale", n, n, d))
        it.append(Item("head", None, None, "Build profile"))
        it.append(Item("radio", "profile", "keep", "keep", "the build there is now: no wait"))
        feats = os.path.join(ROOT, "common", "features.ini")
        text = open(feats).read()
        for n in ini_sections(feats):
            m = re.search(r"^\[%s\]\n(?:configure|cflags) *= *(.*)$" % re.escape(n), text, re.M)
            it.append(Item("radio", "profile", n, n, (m.group(1) if m else "")[:50]))
        it.append(Item("text", None, "profile_text", "more profiles", "comma separated, -name takes one out"))
        it.append(Item("head", None, None, "ini values written before mc starts"))
        for tag, value, desc in toggles():
            it.append(Item("check", "toggles", tag, tag, "%s  (%s)" % (desc, value)))
        it.append(Item("text", None, "extra", "more ini", "[section.]key=value, space separated"))
        it.append(Item("head", None, None, "Keymap"))
        it.append(Item("radio", "keymap", "none", "none", "mc's own bindings"))
        for f in sorted(os.listdir(os.path.join(ROOT, "common", "keymaps"))):
            if f.endswith(".keymap"):
                it.append(Item("radio", "keymap", f[:-7], f[:-7], ""))
        it.append(Item("head", None, None, "Case directories (none marked = all)"))
        for n in case_dirs(self.radios["subject"]):
            it.append(Item("check", "dirs", n, n, ""))
        it.append(Item("button", None, "run", "[ Run ]", ""))
        it.append(Item("button", None, "quit", "[ Quit ]", ""))
        self.items = it
        # the boxed layout: each heading with the items under it
        self.groups = []
        for x in it:
            if x.kind == "head":
                self.groups.append([x.label, []])
            elif x.kind != "button" and self.groups:
                self.groups[-1][1].append(x)
        if self.cursor >= len(it):
            self.cursor = len(it) - 1
        while self.items[self.cursor].kind == "head":
            self.cursor += 1

    # ------------------------------------------------------------ drawing ---

    def put(self, y, x, text, attr, width=None):
        h, w = self.scr.getmaxyx()
        if y < 0 or y >= h or x >= w:
            return
        if width is None:
            width = w - x
        text = text[:min(width, w - x)]
        if width > len(text):
            text = text + " " * (min(width, w - x) - len(text))
        try:
            self.scr.addstr(y, x, text, attr)
        except curses.error:
            pass

    def mark(self, it):
        if it.kind == "radio":
            return "(*)" if self.radios[it.group] == it.key else "( )"
        if it.kind == "check":
            return "[x]" if it.key in self.checks[it.group] else "[ ]"
        return ""

    def box(self, y, x, hgt, wid, title, attr):
        for yy in range(y, y + hgt):
            self.put(yy, x, "", attr, wid)
        try:
            win = self.scr.derwin(hgt, wid, y, x)
            win.attrset(attr)
            win.box()
            if title:
                win.addstr(0, 2, " %s " % title[:wid - 6], attr | curses.A_BOLD)
            win.refresh()
        except curses.error:
            pass

    def draw(self):
        h, w = self.scr.getmaxyx()
        c = self.c
        # a column holds a stack of boxes; two columns, the buttons under them
        left = [g for g in self.groups if g[0].split()[0] in ("Environment", "Subject", "Transports", "Locale", "Keymap", "Case")]
        right = [g for g in self.groups if g not in left]
        need = max(sum(len(g[1]) + 2 for g in left), sum(len(g[1]) + 2 for g in right)) + 7
        if h < need or w < 90:
            return self.draw_list()
        self.scr.erase()
        self.scr.bkgd(" ", c["dialog"])
        self.put(0, 0, "", c["title"])
        self.put(0, 2, "mc sandbox", c["title"])
        self.put(0, w - 44, "click or Space marks, Enter runs, q quits", c["dim"])
        colw = (w - 3) // 2
        for col, (x, groups) in enumerate(((1, left), (2 + colw, right))):
            y = 1
            for title, items in groups:
                hgt = len(items) + 2
                self.box(y, x, hgt, colw, title, c["dialog"])
                for i, it in enumerate(items):
                    it.y, it.x, it.w = y + 1 + i, x + 1, colw - 2
                    focus = self.items.index(it) == self.cursor
                    attr = c["focus"] if focus else c["dialog"]
                    if it.kind == "text":
                        val = self.texts[it.key]
                        line = " %s: %s" % (it.label, val if val else "<" + it.desc + ">")
                        if not val and not focus:
                            attr = c["dim"]
                    else:
                        line = " %s %-16s %s" % (self.mark(it), it.label, it.desc)
                    self.put(it.y, it.x, line, attr, it.w)
                y += hgt
        # the command it all adds up to, and the buttons
        cmd = self.command()
        cy = need - 6 + 1
        title = "the command"
        if self.st["profile"] != "keep":
            title = "the command: a rebuild first, minutes; keep skips it when mc did not change"
        self.box(cy, 1, 4, w - 2, title, c["dialog"])
        self.put(cy + 1, 3, cmd[:w - 6], c["dim"], w - 6)
        self.put(cy + 2, 3, cmd[w - 6:2 * (w - 6)], c["dim"], w - 6)
        by = cy + 5
        bx = w // 2 - 12
        for it in self.items:
            if it.kind == "button":
                it.y, it.x, it.w = by, bx, len(it.label)
                focus = self.items.index(it) == self.cursor
                self.put(by, bx, it.label, c["focus"] if focus else c["button"], len(it.label))
                bx += len(it.label) + 4
        self.scr.refresh()

    def draw_list(self):
        scr = self.scr
        scr.erase()
        h, w = scr.getmaxyx()
        scr.addstr(0, 0, " mc sandbox: click or Space marks, Enter on Run, q quits "[:w - 1], curses.A_REVERSE)
        body = h - 2
        if self.cursor < self.top:
            self.top = self.cursor
        if self.cursor >= self.top + body:
            self.top = self.cursor - body + 1
        for row in range(body):
            i = self.top + row
            if i >= len(self.items):
                break
            it = self.items[i]
            y = row + 1
            attr = curses.A_REVERSE if i == self.cursor and it.kind != "head" else 0
            if it.kind == "head":
                scr.addstr(y, 0, ("-- " + it.label)[:w - 1], curses.A_BOLD)
                continue
            if it.kind == "radio":
                mark = "(*)" if self.radios[it.group] == it.key else "( )"
            elif it.kind == "check":
                mark = "[x]" if it.key in self.checks[it.group] else "[ ]"
            elif it.kind == "text":
                mark = "..."
            else:
                mark = "   "
            if it.kind == "text":
                line = "  %s %-14s %s" % (mark, it.label + ":", self.texts[it.key] or ("<" + it.desc + ">"))
            elif it.kind == "button":
                line = "      %s" % it.label
            else:
                line = "  %s %-16s %s" % (mark, it.label, it.desc)
            it.y, it.x, it.w = y, 0, w - 1
            scr.addstr(y, 0, line[:w - 1], attr)
        cmd = self.command()
        if self.st["profile"] != "keep":
            cmd = "[rebuild first, minutes; keep skips it] " + cmd
        scr.addstr(h - 1, 0, (" " + cmd)[:w - 1], curses.A_DIM)
        scr.refresh()

    # ------------------------------------------------------------ actions ---

    def act(self, i):
        it = self.items[i]
        if it.kind == "radio":
            self.radios[it.group] = it.key
            if it.group == "subject":
                self.build()
        elif it.kind == "check":
            s = self.checks[it.group]
            s.symmetric_difference_update({it.key})
        elif it.kind == "text":
            self.edit(i)
        elif it.kind == "button":
            return it.key
        return None

    def edit(self, i):
        it = self.items[i]
        curses.echo()
        curses.curs_set(1)
        self.put(it.y, it.x, " %s: " % it.label, self.c["focus"], it.w)
        try:
            self.texts[it.key] = self.scr.getstr(it.y, it.x + len(it.label) + 3, it.w - len(it.label) - 4).decode(errors="replace").strip()
        except Exception:
            pass
        curses.noecho()
        curses.curs_set(0)

    def move(self, d):
        n = len(self.items)
        i = self.cursor
        for _ in range(n):
            i = (i + d) % n
            if self.items[i].kind != "head":
                self.cursor = i
                return

    def command(self):
        st = self.st
        st["env"] = self.radios["env"]
        st["subject"] = self.radios["subject"]
        st["transports"] = ",".join(t for t, _ in TRANSPORTS if t in self.checks["transports"]) or "local"
        st["locale"] = self.radios["locale"]
        prof = self.radios["profile"]
        if self.texts["profile_text"]:
            prof = (prof + "," if prof != "keep" else "") + self.texts["profile_text"]
        st["profile"] = prof
        st["toggles"] = ",".join(t for t, _, _ in toggles() if t in self.checks["toggles"])
        st["extra"] = self.texts["extra"]
        st["keymap"] = self.radios["keymap"]
        st["dirs"] = ",".join(d for d in case_dirs(st["subject"]) if d in self.checks["dirs"])
        opts = ""
        for tag, value, _ in toggles():
            if tag in self.checks["toggles"]:
                opts += " -o " + value
        for v in st["extra"].split():
            opts += " -o " + v
        if st["keymap"] != "none":
            opts += " -k " + st["keymap"]
        sb = os.path.join(ROOT, "sandbox.sh")
        cmd = ""
        if prof != "keep":
            cmd = "%s %s build -f %s && " % (sb, st["env"], prof)
        cmd += "%s %s test -c %s -w %s -l %s%s %s" % (sb, st["env"], st["subject"], st["transports"],
                                                      st["locale"], opts, st["dirs"].replace(",", " "))
        return cmd.strip()

    # --------------------------------------------------------------- loop ---

    def hit(self, x, y):
        for i, it in enumerate(self.items):
            if it.kind != "head" and it.y == y and it.x <= x < it.x + it.w:
                return i
        return None

    def run(self):
        curses.curs_set(0)
        curses.mousemask(curses.ALL_MOUSE_EVENTS | curses.REPORT_MOUSE_POSITION)
        self.scr.keypad(True)
        self.c = colors()
        while True:
            self.draw()
            c = self.scr.getch()
            if c == ord("q"):
                return None
            if c in (curses.KEY_DOWN, ord("j")):
                self.move(1)
            elif c in (curses.KEY_UP, ord("k")):
                self.move(-1)
            elif c == ord("\t"):
                self.move(1)
            elif c == curses.KEY_NPAGE:
                for _ in range(10):
                    self.move(1)
            elif c == curses.KEY_PPAGE:
                for _ in range(10):
                    self.move(-1)
            elif c in (ord(" "), ord("\n"), curses.KEY_ENTER, 10, 13):
                r = self.act(self.cursor)
                if r == "run":
                    return self.command()
                if r == "quit":
                    return None
            elif c == curses.KEY_MOUSE:
                try:
                    _, x, y, _, b = curses.getmouse()
                except curses.error:
                    continue
                if b & (curses.BUTTON4_PRESSED):
                    for _ in range(3):
                        self.move(-1)
                    continue
                if b & (getattr(curses, "BUTTON5_PRESSED", 0) or 0):
                    for _ in range(3):
                        self.move(1)
                    continue
                if b & (curses.BUTTON1_CLICKED | curses.BUTTON1_PRESSED | curses.BUTTON1_RELEASED):
                    i = self.hit(x, y)
                    if i is not None:
                        self.cursor = i
                        if b & (curses.BUTTON1_CLICKED | curses.BUTTON1_RELEASED):
                            r = self.act(i)
                            if r == "run":
                                return self.command()
                            if r == "quit":
                                return None


def pager(stdscr, path):
    lines = open(path, errors="replace").read().split("\n")
    top = 0
    curses.mousemask(curses.ALL_MOUSE_EVENTS)
    while True:
        stdscr.erase()
        h, w = stdscr.getmaxyx()
        stdscr.addstr(0, 0, (" " + os.path.relpath(path, ROOT) + "   (q back, wheel or arrows)")[:w - 1], curses.A_REVERSE)
        for row in range(h - 1):
            if top + row < len(lines):
                stdscr.addstr(row + 1, 0, lines[top + row][:w - 1])
        stdscr.refresh()
        c = stdscr.getch()
        if c in (ord("q"), ord("\n"), 10):
            return
        if c in (curses.KEY_DOWN, ord("j")):
            top = min(top + 1, max(0, len(lines) - h + 1))
        elif c in (curses.KEY_UP, ord("k")):
            top = max(top - 1, 0)
        elif c == curses.KEY_NPAGE:
            top = min(top + h - 2, max(0, len(lines) - h + 1))
        elif c == curses.KEY_PPAGE:
            top = max(top - h + 2, 0)
        elif c == curses.KEY_MOUSE:
            try:
                _, _, _, _, b = curses.getmouse()
            except curses.error:
                continue
            if b & curses.BUTTON4_PRESSED:
                top = max(top - 3, 0)
            elif b & (getattr(curses, "BUTTON5_PRESSED", 0) or 0):
                top = min(top + 3, max(0, len(lines) - h + 1))


def failures(stdscr, report):
    """A list of the screens a run left behind; Enter or a click opens one."""
    screens = []
    for d in sorted(os.listdir(report)):
        p = os.path.join(report, d)
        if os.path.isdir(p):
            screens += [os.path.join(p, f) for f in sorted(os.listdir(p)) if f.endswith(".screen")]
    index = os.path.join(report, "index.md")
    entries = [index] + screens
    cur = 0
    curses.mousemask(curses.ALL_MOUSE_EVENTS)
    while True:
        stdscr.erase()
        h, w = stdscr.getmaxyx()
        stdscr.addstr(0, 0, (" %s: the report and %d screens   (Enter opens, q quits)" % (os.path.basename(report), len(screens)))[:w - 1], curses.A_REVERSE)
        for i, e in enumerate(entries[:h - 2]):
            stdscr.addstr(i + 1, 0, ("  " + os.path.relpath(e, report))[:w - 1], curses.A_REVERSE if i == cur else 0)
        stdscr.refresh()
        c = stdscr.getch()
        if c == ord("q"):
            return
        if c in (curses.KEY_DOWN, ord("j")):
            cur = min(cur + 1, len(entries) - 1)
        elif c in (curses.KEY_UP, ord("k")):
            cur = max(cur - 1, 0)
        elif c in (ord("\n"), 10, 13, curses.KEY_ENTER, ord(" ")):
            pager(stdscr, entries[cur])
        elif c == curses.KEY_MOUSE:
            try:
                _, _, y, _, b = curses.getmouse()
            except curses.error:
                continue
            i = y - 1
            if 0 <= i < len(entries) and b & (curses.BUTTON1_CLICKED | curses.BUTTON1_RELEASED):
                cur = i
                pager(stdscr, entries[cur])


def main():
    env = sys.argv[1] if len(sys.argv) > 1 else None
    form = None

    def pick(stdscr):
        nonlocal form
        form = Form(stdscr, env)
        return form.run()

    cmd = curses.wrapper(pick)
    if not cmd:
        return 0
    save_state(form.st)
    os.makedirs(os.path.join(ROOT, "reports"), exist_ok=True)
    with open(os.path.join(ROOT, "reports", "last.cmd"), "w") as f:
        f.write(cmd + "\n")
    print(cmd)
    print()
    report = None
    proc = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    for line in proc.stdout:
        sys.stdout.write(line)
        sys.stdout.flush()
        m = re.match(r"report: /reports/(\S+)", line)
        if m:
            report = os.path.join(ROOT, "reports", m.group(1))
    proc.wait()
    if report and os.path.isdir(report):
        curses.wrapper(failures, report)
    return proc.returncode


if __name__ == "__main__":
    sys.exit(main())
