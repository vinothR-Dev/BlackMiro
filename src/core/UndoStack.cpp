#include "UndoStack.h"

void UndoStack::push(const QList<BoardItem> &before)
{
    m_undo.append(before);
    if (m_undo.size() > kLimit)
        m_undo.removeFirst();
    m_redo.clear();
}

QList<BoardItem> UndoStack::undo(const QList<BoardItem> &current)
{
    if (m_undo.isEmpty())
        return current;
    m_redo.append(current);
    return m_undo.takeLast();
}

QList<BoardItem> UndoStack::redo(const QList<BoardItem> &current)
{
    if (m_redo.isEmpty())
        return current;
    m_undo.append(current);
    return m_redo.takeLast();
}

void UndoStack::clear()
{
    m_undo.clear();
    m_redo.clear();
}
