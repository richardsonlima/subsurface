/*
 * divelistview.cpp
 *
 * classes for the divelist of Subsurface
 *
 */
#include "divelistview.h"
#include "models.h"
#include "modeldelegates.h"
#include "../display.h"
#include <QApplication>
#include <QHeaderView>
#include <QDebug>
#include <QSettings>
#include <QKeyEvent>
#include <QSortFilterProxyModel>
#include <QAction>
#include <QLineEdit>
#include <QKeyEvent>
#include <QMenu>

DiveListView::DiveListView(QWidget *parent) : QTreeView(parent), mouseClickSelection(false),
	currentHeaderClicked(-1), searchBox(new QLineEdit(this))
{
	setUniformRowHeights(true);
	setItemDelegateForColumn(TreeItemDT::RATING, new StarWidgetsDelegate());
	QSortFilterProxyModel *model = new QSortFilterProxyModel(this);
	model->setSortRole(TreeItemDT::SORT_ROLE);
	model->setFilterKeyColumn(-1); // filter all columns
	setModel(model);
	setSortingEnabled(false);
	header()->setContextMenuPolicy(Qt::ActionsContextMenu);
	QAction *showSearchBox = new QAction(tr("Show Search Box"), this);
	showSearchBox->setShortcut( Qt::CTRL + Qt::Key_F);
	showSearchBox->setShortcutContext(Qt::ApplicationShortcut);
	addAction(showSearchBox);

	searchBox->installEventFilter(this);
	searchBox->hide();
	connect(showSearchBox, SIGNAL(triggered(bool)), this, SLOT(showSearchEdit()));
	connect(searchBox, SIGNAL(textChanged(QString)), model, SLOT(setFilterFixedString(QString)));
}

void DiveListView::unselectDives()
{
	selectionModel()->clearSelection();
}

void DiveListView::selectDive(struct dive *dive, bool scrollto)
{
	QSortFilterProxyModel *m = qobject_cast<QSortFilterProxyModel*>(model());
	QModelIndexList match = m->match(m->index(0,0), TreeItemDT::NR, dive->number, 1, Qt::MatchRecursive);
	QModelIndex idx = match.first();

	QModelIndex parent = idx.parent();
	if (parent.isValid())
		expand(parent);
	selectionModel()->select( idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
	if (scrollto)
		scrollTo(idx, PositionAtCenter);
}

void DiveListView::showSearchEdit()
{
	searchBox->show();
	searchBox->setFocus();
}

bool DiveListView::eventFilter(QObject* , QEvent* event)
{
	if(event->type() != QEvent::KeyPress){
		return false;
	}
	QKeyEvent *keyEv = static_cast<QKeyEvent*>(event);
	if (keyEv->key() != Qt::Key_Escape){
		return false;
	}

	searchBox->clear();
	searchBox->hide();
	QSortFilterProxyModel *m = qobject_cast<QSortFilterProxyModel*>(model());
	m->setFilterFixedString(QString());
	return true;
}

// NOTE! This loses trip selection, because while we remember the
// dives, we don't remember the trips (see the "currentSelectedDives"
// list). I haven't figured out how to look up the trip from the
// index. TRIP_ROLE vs DIVE_ROLE?
void DiveListView::headerClicked(int i)
{
	QItemSelection oldSelection = selectionModel()->selection();
	QList<struct dive*> currentSelectedDives;
	DiveTripModel::Layout newLayout;
	bool first = true;

	newLayout = i == (int) TreeItemDT::NR ? DiveTripModel::TREE : DiveTripModel::LIST;

	Q_FOREACH(const QModelIndex& index , oldSelection.indexes()) {
		if (index.column() != 0) // We only care about the dives, so, let's stick to rows and discard columns.
			continue;

		struct dive *d = (struct dive *) index.data(TreeItemDT::DIVE_ROLE).value<void*>();
		if (d)
			currentSelectedDives.push_back(d);
	}

	unselectDives();

	/* No layout change? Just re-sort, and scroll to first selection, making sure all selections are expanded */
	if (currentLayout == newLayout) {
		sortByColumn(i);
	} else {
		// clear the model, repopulate with new indexes.
		reload(newLayout, false);
		sortByColumn(i, Qt::DescendingOrder);
	}

	QSortFilterProxyModel *m = qobject_cast<QSortFilterProxyModel*>(model());

	// repopulat the selections.
	Q_FOREACH(struct dive *d, currentSelectedDives) {
		selectDive(d, first);
		first = false;
	}
}

void DiveListView::reload(DiveTripModel::Layout layout, bool forceSort)
{
	currentLayout = layout;

	header()->setClickable(true);
	connect(header(), SIGNAL(sectionPressed(int)), this, SLOT(headerClicked(int)), Qt::UniqueConnection);

	QSortFilterProxyModel *m = qobject_cast<QSortFilterProxyModel*>(model());
	QAbstractItemModel *oldModel = m->sourceModel();
	if (oldModel)
		oldModel->deleteLater();

	DiveTripModel *tripModel = new DiveTripModel(this);
	tripModel->setLayout(layout);

	m->setSourceModel(tripModel);

	if(!forceSort)
		return;

	sortByColumn(0, Qt::DescendingOrder);
	if (amount_selected && selected_dive >= 0) {
		selectDive(current_dive, true);
	} else {
		QModelIndex firstDiveOrTrip = m->index(0,0);
		if (firstDiveOrTrip.isValid()) {
			if (m->index(0,0, firstDiveOrTrip).isValid())
				setCurrentIndex(m->index(0,0, firstDiveOrTrip));
			else
				setCurrentIndex(firstDiveOrTrip);
		}
	}
}

void DiveListView::reloadHeaderActions()
{
	// Populate the context menu of the headers that will show
	// the menu to show / hide columns.
	if (!header()->actions().size()) {
		QAction *visibleAction = new QAction("Visible:", header());
		header()->addAction(visibleAction);
		QSettings s;
		s.beginGroup("DiveListColumnState");
		for(int i = 0; i < model()->columnCount(); i++) {
			QString title = QString("%1").arg(model()->headerData(i, Qt::Horizontal).toString());
			QString settingName = QString("showColumn%1").arg(i);
			QAction *a = new QAction(title, header());
			bool shown = s.value(settingName, true).toBool();
			a->setCheckable(true);
			a->setChecked(shown);
			a->setProperty("index", i);
			a->setProperty("settingName", settingName);
			connect(a, SIGNAL(triggered(bool)), this, SLOT(toggleColumnVisibilityByIndex()));
			header()->addAction(a);
			setColumnHidden(i, !shown);
		}
		s.endGroup();
	}
}

void DiveListView::toggleColumnVisibilityByIndex()
{
	QAction *action = qobject_cast<QAction*>(sender());
	if (!action)
		return;

	QSettings s;
	s.beginGroup("DiveListColumnState");
	s.setValue(action->property("settingName").toString(), action->isChecked());
	s.endGroup();
	s.sync();
	setColumnHidden(action->property("index").toInt(), !action->isChecked());
}

void DiveListView::currentChanged(const QModelIndex& current, const QModelIndex& previous)
{
	if (!current.isValid())
		return;
	const QAbstractItemModel *model = current.model();
	int selectedDive = 0;
	struct dive *dive = (struct dive*) model->data(current, TreeItemDT::DIVE_ROLE).value<void*>();
	if (!dive)  // it's a trip! select first child.
		dive = (struct dive*) model->data(current.child(0,0), TreeItemDT::DIVE_ROLE).value<void*>();
	selectedDive = get_divenr(dive);
	scrollTo(current);
	if (selectedDive == selected_dive)
		return;
	Q_EMIT currentDiveChanged(selectedDive);
}

void DiveListView::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
	QItemSelection newSelected = selected.size() ? selected : selectionModel()->selection();
	QItemSelection newDeselected = deselected;

	disconnect(selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(selectionChanged(QItemSelection,QItemSelection)));
	disconnect(selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),         this, SLOT(currentChanged(QModelIndex,QModelIndex)));

	Q_FOREACH(const QModelIndex& index, newSelected.indexes()) {
		if (index.column() != 0)
			continue;

		const QAbstractItemModel *model = index.model();
		struct dive *dive = (struct dive*) model->data(index, TreeItemDT::DIVE_ROLE).value<void*>();
		if (!dive) { // it's a trip!
			if (model->rowCount(index)) {
				QItemSelection selection;
				struct dive *child = (struct dive*) model->data(index.child(0,0), TreeItemDT::DIVE_ROLE).value<void*>();
				while (child) {
					select_dive(get_index_for_dive(child));
					child = child->next;
				}
				selection.select(index.child(0,0), index.child(model->rowCount(index) -1 , 0));
				selectionModel()->select(selection, QItemSelectionModel::Select | QItemSelectionModel::Rows);
				selectionModel()->setCurrentIndex(index, QItemSelectionModel::Select | QItemSelectionModel::NoUpdate);
				if (!isExpanded(index))
					expand(index);
			}
		} else {
			select_dive(get_index_for_dive(dive));
		}
	}
	Q_FOREACH(const QModelIndex& index, newDeselected.indexes()) {
		if (index.column() != 0)
			continue;
		const QAbstractItemModel *model = index.model();
		struct dive *dive = (struct dive*) model->data(index, TreeItemDT::DIVE_ROLE).value<void*>();
		if (!dive) { // it's a trip!
			if (model->rowCount(index)) {
				struct dive *child = (struct dive*) model->data(index.child(0,0), TreeItemDT::DIVE_ROLE).value<void*>();
				while (child) {
					deselect_dive(get_index_for_dive(child));
					child = child->next;
				}
			}
		} else {
			deselect_dive(get_index_for_dive(dive));
		}
	}

	QTreeView::selectionChanged(selectionModel()->selection(), newDeselected);
	connect(selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(selectionChanged(QItemSelection,QItemSelection)));
	connect(selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),         this, SLOT(currentChanged(QModelIndex,QModelIndex)));
	// now that everything is up to date, update the widgets
	Q_EMIT currentDiveChanged(selected_dive);
}

void DiveListView::mousePressEvent(QMouseEvent *event)
{
	// all we care about is the unmodified right click
	if ( ! (event->modifiers() == Qt::NoModifier && event->buttons() & Qt::RightButton)) {
		event->ignore();
		QTreeView::mousePressEvent(event);
		return;
	}
	QMenu popup(this);
	popup.addAction(tr("expand all"), this, SLOT(expandAll()));
	QAction *collapseAllAction = popup.addAction(tr("collapse all"), this, SLOT(collapseAll()));
	if (popup.exec(event->globalPos()) == collapseAllAction)
		selectDive(current_dive, true);
}
