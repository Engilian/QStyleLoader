#pragma once
#include <QObject>
#include <functional>

///
/// \brief A class for updating the styles of the widget when the property changes.
///
class QStyleUpdater final
    : public QObject
{
  Q_OBJECT
  class _QStyleUpdater;
  _QStyleUpdater *ptr;
public:
  QStyleUpdater(QWidget *widget = nullptr, QObject *parent = nullptr);
  QStyleUpdater(const QStringList &properties, QWidget *widget = nullptr, QObject *parent = nullptr);
  QStyleUpdater(bool updateChilds, bool allProperies = false, QWidget *widget = nullptr, QObject *parent = nullptr);
  ~QStyleUpdater() override;

public:
  ///
  /// \brief Update widget
  ///
  QWidget *widget() const;

  ///
  /// \brief List of monitored properties
  ///
  QStringList properties() const;

  ///
  /// \brief Update styles of child widgets
  ///
  bool refreshChildWidgets() const;

  ///
  /// \brief Update styles when any property changes
  ///
  bool updateWithAllChanges() const;

public slots:
  ///
  /// \brief Force reload styles
  ///
  void reloadStyle();

  ///
  /// \brief Sets the tracked widget
  /// \param widget
  ///
  void setWidget(QWidget *widget);

  ///
  /// \brief Add a tracked property
  /// \param property
  ///
  void add(const QString &property);

  ///
  /// \brief Remove a tracked property
  /// \param property
  ///
  void remove(const QString &property);

  ///
  /// \brief Sets the list of tracked properties
  /// \param list
  ///
  void setProperties(const QStringList &list);

  ///
  /// \brief Specifies whether to track changes in properties of child widgets.
  /// \details Specifies whether to track changes in properties of child widgets. When you update a property of a child widget, its properties are reloaded.
  /// \param enable
  ///
  void setRefreshChildWidgets(bool enable);

  ///
  /// \brief Toggles the tracking mode to Reload Styles on Any Property Changes
  /// \param enable
  ///
  void setUpdateWithAllChanges(bool enable);

  ///
  /// \brief Child widget tracking filter.
  /// \param filter
  ///
  void setChildFilter(const std::function<bool(QWidget *)> &filter);

signals:
  ///
  /// \brief Style reloaded
  ///
  void styleReloaded(QWidget *widget);
};

class QStyleLoader final
    : public QObject
{
  Q_OBJECT
public:
  struct Item
  {
    enum class Type
    {
      File, Directory
    };
    Type    type;
    QString path;

    Item(Type type = Type::File, const QString &path = "");
    ~Item();

    bool operator==(const Item &) const;
    bool operator!=(const Item &) const;
  };
private:
  class _QStyleLoader;
  _QStyleLoader *ptr;
public:
  explicit QStyleLoader(QObject *parent = nullptr);
  ~QStyleLoader() override;

public:
  static QStyleLoader *instance();

public:
  int count() const;
  Item at(int index) const;
  QList<Item> items() const;
  QStringList fileFilters() const;
  bool contains(const QString &path) const;
  bool containsFile(const QString &path) const;
  bool containsDirectory(const QString &path) const;

  QList<QStyleUpdater *> updaters() const;
  bool containsUpdater(QWidget *widget) const;
  QStyleUpdater *updater(QWidget *widget) const;

  bool autoReloadStyle() const;
public slots:
  void add(Item::Type type, const QString &path);
  void addFile(const QString &path);
  void addDirectory(const QString &path);
  void remove(const QString &path);

  void removeUpdater(QWidget *widget);
  QStyleUpdater *addUpdater(QWidget *widget);
  QStyleUpdater *insertUpdater(QWidget *widget);

  void reloadAllStyle();
  void setAutoReloadStyle(bool enable);

signals:
  void styleReloaded(QStyleUpdater *updater, QWidget *widget);
  void fileStyleChanged(const QString &file);
};
