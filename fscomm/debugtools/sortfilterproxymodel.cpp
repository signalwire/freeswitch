#include <QtGui>
#include "sortfilterproxymodel.h"

ConsoleModel::ConsoleModel (QObject *parent)
        : QAbstractTableModel(parent)
{
    QSettings settings;
    batchSize = settings.value("Console/batchSize", 200).toInt();
    insertionTimer = new QBasicTimer;
    insertionTimer->start(0, this);
}

int ConsoleModel::rowCount ( const QModelIndex & parent ) const
{
    if (parent.isValid())
        return 0;
    return _listDisplayModel.count();
}

int ConsoleModel::columnCount ( const QModelIndex & /*parent*/ ) const
{
    return 1;
}

QVariant ConsoleModel::data ( const QModelIndex & index, int role ) const
{
    if (!index.isValid())
        return QVariant();
    return _listDisplayModel.at(index.row())->data(role);
}

void ConsoleModel::clear()
{
    _listDisplayModel.clear();
    reset();
}

void ConsoleModel::appendRow ( QStandardItem* item )
{
    _listInsertModel.append(item);
    insertionTimer->start(0, this);
}

void ConsoleModel::timerEvent(QTimerEvent *e)
{

    if (e->timerId() == insertionTimer->timerId())
    {
        if (!_listInsertModel.isEmpty())
        {
            int inserted_items = 0;
            int toBeInserted = 0;
            if (_listInsertModel.size() < batchSize)
            {
                toBeInserted = _listInsertModel.size() - 1;
            } else {
                 toBeInserted = batchSize - 1;
            }
            emit beforeInserting();
            beginInsertRows( QModelIndex(), _listDisplayModel.size(), _listDisplayModel.size() + toBeInserted );
            while( !_listInsertModel.isEmpty() && inserted_items <= batchSize)
            {
                _listDisplayModel.append(_listInsertModel.takeFirst());
                inserted_items++;
            }
            endInsertRows();
            emit afterInserting();
        } else {
            insertionTimer->stop();
        }
    }
}

SortFilterProxyModel::SortFilterProxyModel(QObject *parent)
        : QSortFilterProxyModel(parent)
{
    reverseFlag = false;
    for(int i = 0; i < 8; i++)
        loglevels.insert(i, true);
}

void SortFilterProxyModel::toggleReverseFlag()
{
    reverseFlag = !reverseFlag;
    invalidateFilter();
}

void SortFilterProxyModel::setLogLevelFilter(int level)
{
    loglevels.replace(level, loglevels.value(level) == false);
    // Let us filter
    invalidateFilter();
}

void SortFilterProxyModel::setUUIDFilterLog(QString uuid)
{
    _uuid = uuid;
    invalidateFilter();
}

bool SortFilterProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    QModelIndex index0 = sourceModel()->index(source_row, 0, source_parent);
    bool uuidMatch = true;

    if (!_uuid.isEmpty())
        uuidMatch = (sourceModel()->data(index0, ConsoleModel::UUIDRole).toString() == _uuid);

    bool res = (loglevels.value(sourceModel()->data(index0, Qt::UserRole).toInt()) == true
            && sourceModel()->data(index0).toString().contains(filterRegExp())
            && uuidMatch);
    if (reverseFlag)
        return !res;
    else
        return res;
}
