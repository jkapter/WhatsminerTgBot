#ifndef TREETABLEMODELS_H
#define TREETABLEMODELS_H

#include <QAbstractTableModel>
#include <QObject>

#include "whatsminerdevice.h"
#include "devicemanager.h"

class DeviceTreeItem {
public:
    DeviceTreeItem(const QString& id, DeviceTreeItem* parent);
    bool AppendChild(std::unique_ptr<DeviceTreeItem> &&child, int row = -1);
    bool AppendChild(const QString& id, int row = -1);
    bool DeleteChildRecursively(const QString& id);
    bool DeleteChild(int row);
    DeviceTreeItem* Child(int row) const;
    DeviceTreeItem* Child(const QString& id) const;
    int ChildCount() const;
    QVariant Data() const;
    int Row() const;
    DeviceTreeItem* ParentItem();
    const QString& GetId() const;

private:
    DeviceTreeItem* parent_;
    const QString id_;
    std::unordered_map<const QString*, std::unique_ptr<DeviceTreeItem>> childs_;
    std::list<QString> child_ids_;
};

class DeviceTreeViewModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit DeviceTreeViewModel(DeviceManager& dev_manager, QObject *parent = nullptr);
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    void update();

public slots:
    void sl_tree_selection_changed(const QModelIndex &current, const QModelIndex &previous);

signals:
    void sg_change_tree_item(const QString& previous, const QString& current);

private:
    DeviceManager& dev_manager_;
    std::unique_ptr<DeviceTreeItem> root_item_;
    std::vector<QString> devices_;
    const QString empty_str_ = {};
    void build_tree_items_();
    const QString& get_id_(const QModelIndex& index) const;
};

class DeviceDataViewerModel: public QAbstractTableModel
{
    Q_OBJECT
public:
    DeviceDataViewerModel() = delete;
    explicit DeviceDataViewerModel(const QString& address, DeviceManager& dev_manager, QObject* parent = nullptr);
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    DeviceManager& device_manager_;
    QString dev_address_;
};

#endif // TREETABLEMODELS_H
