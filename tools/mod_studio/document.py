"""JSON document history shared by every Mod Studio editing pane."""
import copy
import time


class Document:
    def __init__(self, pack, path=None):
        self.pack = pack
        self.path = path
        self.selection = 'pack'
        self._saved = copy.deepcopy(pack)
        self._last = self._snapshot()
        self._before = None
        self._undo = []
        self._redo = []
        self._group = None
        self._group_time = 0

    @property
    def dirty(self):
        return self.pack != self._saved

    def _snapshot(self):
        return copy.deepcopy(self.pack), self.selection

    def select(self, selection):
        self.selection = selection
        self._last = self._last[0], selection
        self._group = None

    def _restore(self, snapshot):
        pack, selection = snapshot
        self.pack.clear()
        self.pack.update(copy.deepcopy(pack))
        self.selection = selection
        self._last = self._snapshot()
        self._group = None

    def _record(self, before, group=None):
        after = self._snapshot()
        if before[0] == after[0]:
            self._last = after
            return False
        now = time.monotonic()
        if not (group is not None and group == self._group and
                now - self._group_time < 0.75):
            self._undo.append(before)
        self._redo.clear()
        self._last = after
        self._group, self._group_time = group, now
        return True

    def begin_edit(self):
        if self._before is not None:
            raise RuntimeError('An edit transaction is already open')
        self._before = self._snapshot()
        self._group = None

    def commit_edit(self):
        if self._before is None:
            return False
        before, self._before = self._before, None
        return self._record(before)

    def cancel_edit(self):
        if self._before is None:
            return False
        before, self._before = self._before, None
        self._restore(before)
        return True

    def set_value(self, path, value):
        if not path:
            raise ValueError('A field path is required')
        target = self.pack
        for key in path[:-1]:
            target = target[key]
        own_transaction = self._before is None
        if own_transaction:
            self.begin_edit()
        target[path[-1]] = copy.deepcopy(value)
        if own_transaction:
            return self.commit_edit()
        return True

    def checkpoint(self, group=None):
        if self._before is not None:
            return False
        return self._record(self._last, group)

    def mark_saved(self):
        self.commit_edit()
        self._saved = copy.deepcopy(self.pack)
        self._last = self._snapshot()
        self._group = None

    def undo(self):
        self.commit_edit()
        if not self._undo:
            return False
        self._redo.append(self._snapshot())
        self._restore(self._undo.pop())
        return True

    def redo(self):
        self.commit_edit()
        if not self._redo:
            return False
        self._undo.append(self._snapshot())
        self._restore(self._redo.pop())
        return True
