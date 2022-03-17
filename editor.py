import sys
import array
import os
import curses
import time

YELLOW = 1
WHITE = 2
CYAN = 3
MAGENTA = 4
RED = 5
N_COLORS = 5

COLOR_MODE = {
        YELLOW: 'compile',
        WHITE: 'compile-next',
        CYAN: 'comment',
        MAGENTA: 'define',
        RED: 'execute'
        }

class Buffer:
    def __init__(self, n_cols, n_rows):
        self.row = 0
        self.col = 0
        self.n_cols = n_cols
        self.n_rows = n_rows
        self.erase()
        self.mark_start = None

    def erase(self):
        self.text = [[(' ', CYAN)]*self.n_cols for _ in range(self.n_rows)]

    def normalize(self):
        # Will mess up string literals
        #self.text = [[(' ', CYAN) if c == ' ' else (c, color)
        #              for c, color in row]
        #             for row in self.text]
        assert len(self.text) == self.n_rows
        assert all(len(row) == self.n_cols for row in self.text)

    def draw(self, scr):
        scr.erase()
        marked = False
        for i, row in enumerate(self.text):
            for j, (c, color) in enumerate(row):
                if self.mark_start == (i, j):
                    if self.mark_start < (self.row, self.col):
                        marked = True
                if self.row == i and self.col == j:
                    marked = False
                marked_color = color + N_COLORS if marked else color
                scr.addstr(i, j, c, curses.color_pair(marked_color))


    def save(self, filename):
        values = []
        for row in self.text:
            for c, color in row:
                word = ord(c) if color == RED else color + (ord(c) << 8)
                values.append(word)

        with open(filename, 'wb') as f:
            array.array('H', values).tofile(f)

    def load(self, filename):
        size = self.n_rows*self.n_cols
        a = array.array('H', [])
        with open(filename, 'rb') as f:
            a.fromfile(f, size)

        i = 0
        for row in self.text:
            row.clear()
            for j in range(self.n_cols):
                word = a[i]
                i += 1
                low = word & 0x7f
                high = (word >> 8) & 0x7f
                if low in (YELLOW, WHITE, CYAN, MAGENTA):
                    color = low
                    c = chr(high)
                else:
                    color = RED
                    c = chr(low)
                assert c != chr(0), (i, '0x%04x'%word)
                row.append((c, color))

 
def main(scr):
    bg = curses.COLOR_BLACK
    marked_bg = curses.COLOR_GREEN
    curses.init_pair(CYAN, curses.COLOR_CYAN, bg) 
    curses.init_pair(WHITE, curses.COLOR_WHITE, bg) 
    curses.init_pair(YELLOW, curses.COLOR_YELLOW, bg) 
    curses.init_pair(RED, curses.COLOR_RED, bg) 
    curses.init_pair(MAGENTA, curses.COLOR_MAGENTA, bg) 
    curses.init_pair(CYAN+N_COLORS, curses.COLOR_CYAN, marked_bg) 
    curses.init_pair(WHITE+N_COLORS, curses.COLOR_WHITE, marked_bg) 
    curses.init_pair(YELLOW+N_COLORS, curses.COLOR_YELLOW, marked_bg) 
    curses.init_pair(RED+N_COLORS, curses.COLOR_RED, marked_bg) 
    curses.init_pair(MAGENTA+N_COLORS, curses.COLOR_MAGENTA, marked_bg) 
    scr.clear()
    buf = Buffer(64, 16)
    filename = "default.block" if len(sys.argv) < 2 else sys.argv[1]
    if os.path.exists(filename):
        buf.load(filename)
    color = CYAN
    command_mode = False
    clip = None
    while True:
        buf.normalize() # inefficient, here to clean up after previous bugs
        buf.draw(scr)
        max_y, max_x = scr.getmaxyx()
        scr.addstr(max_y-1, 0,
                ('C' if command_mode else 'E')+' '+COLOR_MODE[color])
        key = scr.getch(buf.row, buf.col)
        keyname = curses.keyname(key)
        if command_mode:
            if key == ord('X'):
                break
            elif key == ord('S'):
                buf.save(filename)
                scr.clear()
                scr.refresh()
                time.sleep(0.5)
            elif key == ord('R'):
                color = RED
            elif key == ord('Y'):
                color = YELLOW
            elif key == ord('W'):
                color = WHITE
            elif key == ord('C'):
                color = CYAN
            elif key == ord('M'):
                color = MAGENTA
            elif key == ord('H'):
                buf.mark_start = (buf.row, buf.col)
            elif key == ord('U'):
                buf.mark_start = None
            elif key == ord('D'):
                if buf.mark_start:
                    if buf.mark_start[0] == buf.row and \
                            buf.mark_start[1] < buf.col:
                        start = buf.mark_start
                        clip = buf.text[buf.row][buf.mark_start[1]:buf.col]
                        buf.text[buf.row] = \
                            buf.text[buf.row][:buf.mark_start[1]] + \
                            buf.text[buf.row][buf.col:] + \
                            [(' ', CYAN)]*len(clip)
                        end = (buf.row, buf.col)
                        buf.row, buf.col = start
                buf.mark_start = None
            elif key == ord('P'):
                if clip:
                    must_be_blank = buf.text[buf.row][-len(clip):]
                    if must_be_blank == [(' ', CYAN)]*len(clip):
                        buf.text[buf.row] = buf.text[buf.row][:buf.col] + \
                                clip + buf.text[buf.row][buf.col:-len(clip)]
                        clip = None
            command_mode = False
        else:
            if 0x20 <= key <= 0x7f:
                buf.text[buf.row] = (buf.text[buf.row][:buf.col] +
                                     [(chr(key), color)] +
                                     buf.text[buf.row][buf.col:-1])
                #buf.text[buf.row][buf.col] = (chr(key), color)
                buf.col = (buf.col + 1) % buf.n_cols
            elif key == curses.KEY_DC:
                buf.text[buf.row] = (buf.text[buf.row][:buf.col] + 
                                     buf.text[buf.row][buf.col+1:] +
                                     [(' ', CYAN)])
            elif key == curses.KEY_LEFT:
                buf.col = max(buf.col-1, 0)
            elif key == curses.KEY_RIGHT:
                buf.col = min(buf.col+1, buf.n_cols-1)
            elif key == curses.KEY_UP:
                buf.row = max(buf.row-1, 0)
            elif key == curses.KEY_DOWN:
                buf.row = min(buf.row+1, buf.n_rows-1)
            elif key == curses.KEY_END:
                buf.col = buf.n_cols-1
            elif key == curses.KEY_HOME:
                buf.col = 0
            elif key == curses.KEY_F4:
                command_mode = True

curses.wrapper(main)

