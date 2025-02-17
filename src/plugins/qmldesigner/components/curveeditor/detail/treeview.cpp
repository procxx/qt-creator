/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Design Tooling
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/
#include "treeview.h"
#include "curveeditormodel.h"
#include "curveitem.h"
#include "treeitem.h"
#include "treeitemdelegate.h"

#include <QHeaderView>
#include <QMouseEvent>

namespace DesignTools {

TreeView::TreeView(CurveEditorModel *model, QWidget *parent)
    : QTreeView(parent)
{
    model->setTreeView(this);

    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    setUniformRowHeights(true);
    setRootIsDecorated(false);
    setMouseTracking(true);
    setHeaderHidden(true);

    setModel(model);

    auto expandItems = [this]() { expandAll(); };
    connect(model, &QAbstractItemModel::modelReset, expandItems);

    auto *delegate = new TreeItemDelegate(model->style(), this);
    setItemDelegate(delegate);

    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);

    connect(selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this,
            &TreeView::changeSelection);
    setStyle(model->style());

    header()->setSectionResizeMode(0, QHeaderView::Stretch);
    header()->setSectionResizeMode(1, QHeaderView::Fixed);
    header()->setSectionResizeMode(2, QHeaderView::Fixed);

    header()->setStretchLastSection(false);
    header()->resizeSection(1, 20);
    header()->resizeSection(2, 20);
}

void TreeView::setStyle(const CurveEditorStyle &style)
{
    QPalette pal = palette();
    pal.setBrush(QPalette::Base, style.backgroundBrush);
    pal.setBrush(QPalette::Button, style.backgroundAlternateBrush);
    pal.setBrush(QPalette::Text, style.fontColor);

    // Tmp to see what happens on windows/macOS.
    pal.setBrush(backgroundRole(), Qt::white);
    pal.setBrush(foregroundRole(), Qt::white);

    setPalette(pal);

    if (auto *delegate = qobject_cast<TreeItemDelegate *>(itemDelegate()))
        delegate->setStyle(style);
}

std::vector<CurveItem *> TreeView::selection()
{
    std::vector<DesignTools::CurveItem *> items;
    for (auto &&index : selectionModel()->selectedRows(0)) {
        if (index.isValid()) {
            auto *treeItem = static_cast<TreeItem *>(index.internalPointer());
            if (auto *propertyItem = treeItem->asPropertyItem())
                items.push_back(new CurveItem(treeItem->id(), propertyItem->curve()));
        }
    }
    return items;
}

void TreeView::changeCurve(unsigned int id, const AnimationCurve &curve)
{
    if (auto *curveModel = qobject_cast<CurveEditorModel *>(model()))
        curveModel->setCurve(id, curve);
}

void TreeView::changeSelection(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(selected)
    Q_UNUSED(deselected)

    std::vector<CurveItem *> curves;
    for (auto index : selectedIndexes()) {
        if (index.isValid() && index.column() == 0) {
            auto *treeItem = static_cast<TreeItem *>(index.internalPointer());
            if (auto *propertyItem = treeItem->asPropertyItem()) {
                auto *citem = new CurveItem(treeItem->id(), propertyItem->curve());
                citem->setValueType(propertyItem->valueType());
                citem->setComponent(propertyItem->component());
                curves.push_back(citem);
            }
        }
    }

    emit curvesSelected(curves);
}

QSize TreeView::sizeHint() const
{
    return QSize(170, 300);
}

void TreeView::mousePressEvent(QMouseEvent *event)
{
    QModelIndex index = indexAt(event->pos());
    if (index.isValid()) {
        auto *treeItem = static_cast<TreeItem *>(index.internalPointer());
        if (index.column() == 1)
            treeItem->setLocked(!treeItem->locked());
        else if (index.column() == 2)
            treeItem->setPinned(!treeItem->pinned());
    }
    QTreeView::mousePressEvent(event);
}

} // End namespace DesignTools.
