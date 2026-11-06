#pragma once

#include "BoardItem.h"

#include <QList>

// Snapshot-based undo. Item lists are cheap value copies (image bytes live in
// Document::assets and are never duplicated here), which keeps every editing
// operation trivially undoable without per-command inverse logic.
class UndoStack
{
public:
    void push(const QList<BoardItem> &before);
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    QList<BoardItem> undo(const QList<BoardItem> &current);
    QList<BoardItem> redo(const QList<BoardItem> &current);
    void clear();

private:
    static constexpr int kLimit = 200;
    QList<QList<BoardItem>> m_undo;
    QList<QList<BoardItem>> m_redo;
};
