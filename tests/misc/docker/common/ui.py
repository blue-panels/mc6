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


class Form:
    def __init__(self, stdscr, env):
        self.scr = stdscr
        self.st = load_state()
        if env:
            self.st["env"] = env
        self.radios = {"env": self.st["env"], "subject": self.st["subject"],
                       "locale": self.st["locale"], "profile": self.st["profile"] or "keep",
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
        it.append(Item("head", None, None, "Build profile (a rebuild before the run)"))
        it.append(Item("radio", "profile", "keep", "keep", "the build there is now"))
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
        it.append(Item("head", None, None, ""))
        it.append(Item("button", None, "run", "[ Run ]", ""))
        it.append(Item("button", None, "quit", "[ Quit ]", ""))
        self.items = it
        if self.cursor >= len(it):
            self.cursor = len(it) - 1
        while self.items[self.cursor].kind == "head":
            self.cursor += 1

    # ------------------------------------------------------------ drawing ---

    def draw(self):
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
            scr.addstr(y, 0, line[:w - 1], attr)
        cmd = self.command()
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
        h, w = self.scr.getmaxyx()
        y = i - self.top + 1
        curses.echo()
        curses.curs_set(1)
        self.scr.addstr(y, 0, " " * (w - 1))
        self.scr.addstr(y, 0, "  %s: " % it.label)
        try:
            self.texts[it.key] = self.scr.getstr(y, len(it.label) + 4, w - len(it.label) - 6).decode(errors="replace").strip()
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

    def run(self):
        curses.curs_set(0)
        curses.mousemask(curses.ALL_MOUSE_EVENTS | curses.REPORT_MOUSE_POSITION)
        self.scr.keypad(True)
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
                    i = self.top + y - 1
                    if 0 <= i < len(self.items) and self.items[i].kind != "head":
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
