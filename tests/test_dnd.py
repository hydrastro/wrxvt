"""Drag and drop (XDND): files and text dropped on the window."""
import unittest

from harness import TerminalTest

try:
    import Xlib  # noqa: F401
    HAVE_XLIB = True
except ImportError:
    HAVE_XLIB = False

# runs inside the terminal: a drag source that drops on its own window,
# the way a file manager does
SOURCE = r'''
import os, time
from Xlib import X, display, Xatom
from Xlib.protocol import event

def drop(types, data, more_types=False):
    d = display.Display()
    atom = lambda name: d.intern_atom(name)
    target = int(os.environ['WINDOWID'])
    src = d.screen().root.create_window(0, 0, 1, 1, 0, X.CopyFromParent)
    src.set_selection_owner(atom('XdndSelection'), X.CurrentTime)
    type_atoms = [atom(t) for t in types]
    if more_types:
        src.change_property(atom('XdndTypeList'), Xatom.ATOM, 32, type_atoms)
    def send(name, l1=0, l2=0, l3=0, l4=0):
        ev = event.ClientMessage(window=target, client_type=atom(name),
                                 data=(32, [src.id, l1, l2, l3, l4]))
        d.send_event(target, ev)
        d.flush()
    def wait(name, timeout=3):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            while d.pending_events():
                e = d.next_event()
                if e.type == X.SelectionRequest:
                    prop = e.property or e.target
                    if e.target in type_atoms:
                        e.requestor.change_property(prop, e.target, 8, data)
                    else:
                        prop = X.NONE
                    reply = event.SelectionNotify(time=e.time, requestor=e.requestor,
                                                  selection=e.selection, target=e.target,
                                                  property=prop)
                    e.requestor.send_event(reply)
                    d.flush()
                elif e.type == X.ClientMessage and e.client_type == atom(name):
                    return e.data[1]
            time.sleep(0.01)
        raise TimeoutError(name)
    padded = (type_atoms + [0, 0, 0])[:3]
    send('XdndEnter', 5 << 24 | (1 if more_types else 0), *padded)
    send('XdndPosition', 0, 10 << 16 | 10, X.CurrentTime, atom('XdndActionCopy'))
    status = wait('XdndStatus')
    if not status[1] & 1:
        send('XdndLeave')
        return 'refused'
    send('XdndDrop', 0, X.CurrentTime)
    finished = wait('XdndFinished')
    return finished[1]
'''


@unittest.skipUnless(HAVE_XLIB, 'python-xlib not installed')
class DragAndDrop(TerminalTest):
    def test_files_become_quoted_paths(self):
        run = self.run_in_terminal(SOURCE + r'''
uris = b'file:///home/me/notes.txt\r\nfile://host/home/me/My%20Music/it%27s.flac\r\n# a comment\r\n'
result['finished'] = drop(['text/uri-list', 'UTF8_STRING'], uris)
result['typed'] = read_until('flac\' ', 3)
''')
        self.assertEqual(run.result['finished'], 1)
        self.assertEqual(run.result['typed'], "/home/me/notes.txt '/home/me/My Music/it'\\''s.flac' ")

    def test_text_is_a_paste(self):
        run = self.run_in_terminal(SOURCE + r'''
write('\x1b[?2004h')
result['finished'] = drop(['text/html', 'text/plain;charset=utf-8'], 'dropped tëxt'.encode())
result['typed'] = read_until('201~', 3)
''')
        self.assertEqual(run.result['finished'], 1)
        self.assertEqual(run.result['typed'], '\x1b[200~dropped tëxt\x1b[201~')

    def test_many_types_and_refusal(self):
        run = self.run_in_terminal(SOURCE + r'''
types = ['image/png', 'application/x-a', 'application/x-b', 'text/uri-list']
result['many'] = drop(types, b'file:///tmp/x.png\r\n', more_types=True)
result['typed'] = read_until('png ', 3)
result['refused'] = drop(['image/png'], b'')
''')
        self.assertEqual(run.result['many'], 1)
        self.assertEqual(run.result['typed'], '/tmp/x.png ')
        self.assertEqual(run.result['refused'], 'refused')


if __name__ == '__main__':
    unittest.main()
