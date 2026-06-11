#include "treetablemodels.h"

#include <QFont>
#include <QIcon>

using namespace Qt::StringLiterals;

DeviceTreeItem::DeviceTreeItem(const QString &id, DeviceTreeItem *parent)
    : parent_(parent)
    , id_(id)
{}

bool DeviceTreeItem::AppendChild(std::unique_ptr<DeviceTreeItem> &&child, int row)
{
    if(row == -1) {
        child_ids_.push_back(child->id_);
        childs_[&child_ids_.back()] = std::move(child);
        return true;
    }
    if(row < 0 || std::cmp_greater(row, child_ids_.size())) {
        return false;
    }

    auto it_pos = child_ids_.begin();
    std::advance(it_pos, row);
    auto it = child_ids_.insert(it_pos, child->id_);
    childs_[&(*it)] = std::move(child);
    return true;
}

bool DeviceTreeItem::AppendChild(const QString &id, int row)
{
    return AppendChild(std::make_unique<DeviceTreeItem>(id, this), row);
}

bool DeviceTreeItem::DeleteChildRecursively(const QString &id)
{
    bool b = false;
    for(auto& [it, child_ptr]: childs_) {
        b = b || child_ptr->DeleteChildRecursively(id);
    }
    auto it = std::find(child_ids_.begin(), child_ids_.end(), id);
    if(it != child_ids_.end()) {
        childs_.erase(&(*it));
        child_ids_.erase(it);
        b = true;
    }
    return b;
}

bool DeviceTreeItem::DeleteChild(int row)
{
    if(row >= 0 && std::cmp_less(row, child_ids_.size())) {
        auto it = child_ids_.begin();
        std::advance(it, row);
        childs_.erase(&(*it));
        child_ids_.erase(it);
        return true;
    }
    return false;
}

DeviceTreeItem *DeviceTreeItem::Child(int row) const
{
    if(row < 0 || std::cmp_greater_equal(row, child_ids_.size())) return nullptr;
    auto it = child_ids_.cbegin();
    std::advance(it, row);
    return childs_.at(&(*it)).get();
}

DeviceTreeItem *DeviceTreeItem::Child(const QString &id) const
{
    auto it = std::find(child_ids_.begin(), child_ids_.end(), id);
    if(it == child_ids_.end()) return nullptr;
    return childs_.at(&(*it)).get();
}

int DeviceTreeItem::ChildCount() const
{
    return child_ids_.size();
}

QVariant DeviceTreeItem::Data() const
{
    return id_;
}

int DeviceTreeItem::Row() const
{
    if(!parent_) return 0;
    auto it = std::find(parent_->child_ids_.begin(), parent_->child_ids_.end(), id_);
    return std::distance(parent_->child_ids_.begin(), it);
}

DeviceTreeItem *DeviceTreeItem::ParentItem()
{
    return parent_;
}

const QString &DeviceTreeItem::GetId() const
{
    return id_;
}

//=================================================================================
//============ DeviceTreeViewModel ================================================
//=================================================================================

DeviceTreeViewModel::DeviceTreeViewModel(DeviceManager &dev_manager, QObject *parent)
    : QAbstractItemModel(parent)
    , dev_manager_(dev_manager)
{
    build_tree_items_();
}

QVariant DeviceTreeViewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    return {};
}

QModelIndex DeviceTreeViewModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) return {};

    DeviceTreeItem *parentItem = parent.isValid()
                                 ? static_cast<DeviceTreeItem*>(parent.internalPointer())
                                 : root_item_.get();

    if (auto *childItem = parentItem->Child(row))
        return createIndex(row, column, childItem);
    return {};
}

QModelIndex DeviceTreeViewModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) return {};

    auto *childItem = static_cast<DeviceTreeItem*>(index.internalPointer());
    DeviceTreeItem *parentItem = childItem->ParentItem();

    return parentItem != root_item_.get()
               ? createIndex(parentItem->Row(), 0, parentItem) : QModelIndex{};
}

int DeviceTreeViewModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) return 0;

    const DeviceTreeItem *parentItem = parent.isValid()
                                       ? static_cast<const DeviceTreeItem*>(parent.internalPointer())
                                       : root_item_.get();
    return parentItem->ChildCount();
}

int DeviceTreeViewModel::columnCount(const QModelIndex &parent) const
{
    return 2;
}

QVariant DeviceTreeViewModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return {};

    const auto *item = static_cast<const DeviceTreeItem*>(index.internalPointer());

    if(role == Qt::DisplayRole && index.column() == 1) {
        return item->GetId();
    }

    if(role == Qt::DecorationRole && index.column() == 0) {
        bool b = dev_manager_.GetDeviceData(item->GetId()).Online;
        QString icon_path = b ? u":/img/circle_green_checkmark.svg"_s : u":/img/block.png"_s;
        return QIcon(icon_path);
    }

    if (role == Qt::TextAlignmentRole && index.column() == 0) {
        return Qt::AlignCenter;
    }

    return {};
}

void DeviceTreeViewModel::update()
{
    build_tree_items_();
}

void DeviceTreeViewModel::sl_tree_selection_changed(const QModelIndex &current, const QModelIndex &previous)
{
   emit sg_change_tree_item(get_id_(current), get_id_(previous));
}


void DeviceTreeViewModel::build_tree_items_()
{
    beginResetModel();
    root_item_.reset(new DeviceTreeItem(u"root"_s, nullptr));
    for(const auto& dev: dev_manager_.GetDeviceAddresses()) {
        root_item_->AppendChild(dev);
    }
    endResetModel();
}

const QString &DeviceTreeViewModel::get_id_(const QModelIndex &index) const
{
    if (!index.isValid() || !index.parent().isValid()) return empty_str_;
    return static_cast<DeviceTreeItem*>(index.internalPointer())->GetId();
}

//=================================================================================
//============ DeviceTreeViewModel ================================================
//=================================================================================

DeviceDataViewerModel::DeviceDataViewerModel(const QString &address, DeviceManager &dev_manager, QObject *parent)
    : QAbstractTableModel(parent)
    , device_manager_(dev_manager)
    , dev_address_(address)
{}

int DeviceDataViewerModel::rowCount(const QModelIndex &parent) const
{
    return device_manager_.GetVariableNames(dev_address_).size();
}

int DeviceDataViewerModel::columnCount(const QModelIndex &parent) const
{
    return 2;
}

QModelIndex DeviceDataViewerModel::index(int row, int column, const QModelIndex &parent) const
{
    auto vars = device_manager_.GetVariableNames(dev_address_);
    if(row >=0 && std::cmp_less(row, vars.size()) && column >=0 && column < 2) {
        return createIndex(row, column);
    }
    return QModelIndex();
}

QVariant DeviceDataViewerModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch(section) {
        case 0: return QString("Параметр");
        case 1: return QString("Значение");
        }
    }

    if(role == Qt::FontRole && orientation == Qt::Horizontal) {
        QFont font;
        font.setBold(true);
        return font;
    }

    if(role == Qt::TextAlignmentRole) {
        return Qt::AlignCenter;
    }

    return {};
}

QVariant DeviceDataViewerModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid()) return {};
    auto vars = device_manager_.GetVariableNames(dev_address_);

    if(role == Qt::DisplayRole && index.column() == 0) {
        if(std::cmp_less(index.row(), vars.size())) {
            return vars.at(index.row());
        } else {
            return {};
        }
    }

    if(role == Qt::DisplayRole && index.column() == 1) {
        return device_manager_.GetStringValue(dev_address_,  vars.at(index.row()));
    }

    if(role == Qt::BackgroundRole) {
        return {};
    }

    if(role == Qt::TextAlignmentRole) {
        if(index.column() == 0) {
            return Qt::AlignLeft;
        }
        return Qt::AlignCenter;
    }
    return {};
}
