
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QDeclarativeContext>

class QDeclarativeView;
class QTreeView;

#include "dynamictreemodel.h"

Q_DECLARE_METATYPE(QModelIndex)

class KBreadcrumbNavigationFactory;

class MainWindow : public QWidget
{
  Q_OBJECT
public:
  MainWindow(QWidget* parent = 0, Qt::WindowFlags f = 0);

public slots:
  /** Returns whether or not the child collection at row @param row has children. */
  bool childCollectionHasChildren( int row );
  int selectedCollectionRow();

private:
  QTreeView *m_treeView;
  DynamicTreeModel *m_treeModel;
  QDeclarativeView *m_declarativeView;
  KBreadcrumbNavigationFactory *m_bnf;
};

#endif

